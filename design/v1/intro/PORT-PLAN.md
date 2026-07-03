# MyUI 入场动画 — Fabric 搬运实现规划

> **Agent 任务：** 将 `myui-intro-screen.html` 中的入场动画 **1:1 搬运** 到 Minecraft Fabric Mod 代码库。  
> **入口文档：** 先读 [`AGENT-BRIEF.md`](./AGENT-BRIEF.md)，再读本文档 + `DESIGN-SPEC.md` + `timeline.json` + HTML 源文件。

---

## 0. Agent 工作流（按顺序执行）

```
1. 读取 AGENT-BRIEF.md（总纲 + 验收 + Prompt 模板）
2. 读取目标 Mod 仓库结构（packages、现有 Screen、Mixin）
3. 读取 DESIGN-SPEC.md + timeline.json + layer-map.json + particles-spec.json
4. 打开 myui-intro-screen.html 对照视觉效果
5. 按 §4 任务清单逐项实现
6. 按 §7 Acceptance Criteria 自测
7. 提交 PR，附前后对比截图/录屏
```

**不要** 重新设计动画；**不要** 简化时序除非 §6 降级表明确允许。

---

## 1. 目标架构（Fabric 1.20+ 示例）

```
src/main/java/com/yourmod/myui/
├── screen/
│   ├── IntroAnimationScreen.java      # 主入口 Screen
│   ├── MyUITitleScreen.java           # 动画结束跳转目标（或复用现有）
│   └── intro/
│       ├── IntroTimeline.java         # 8000ms 主时钟 + 插值
│       ├── IntroLayerRenderer.java    # 按 z-order 调度绘制
│       ├── IntroParticleEngine.java   # 移植 canvas 粒子逻辑
│       ├── IntroEasing.java           # easePremium / easeCinema
│       └── IntroTokens.java           # 颜色常量（来自 tokens.json）
├── client/
│   ├── MyUIClient.java                # ClientModInitializer
│   └── IntroScreenHandler.java        # 首次启动 / 重播逻辑
└── resources/
    ├── assets/myui/textures/
    │   ├── panorama.png               # 或用原版全景
    │   └── film_grain.png             # 可选噪声贴图
    └── assets/myui/lang/
        └── zh_cn.json                 # 「加载中」「准备就绪」等
```

---

## 2. CSS / Web → Minecraft 映射表

| Web 概念 | Fabric / MC 实现 | 备注 |
|----------|------------------|------|
| `Screen` | `IntroAnimationScreen extends Screen` | `super(Text.literal(""))` |
| CSS `@keyframes` | `IntroTimeline.getProgress(trackId, elapsedMs)` | 用 `easePremium(t)` 插值 |
| `clip-path: circle()` | `RenderSystem.enableScissor` 或 `Stencil` 或 shader | 圆形 mask：CPU 算半径，每帧 scissor 近似 |
| `backdrop-filter: blur` | 预模糊 panorama 纹理 **或** Mod 自定义 shader | MC 无原生 CSS blur；见 §6 |
| `rgba` glass 面板 | `fill(x,y,w,h, colorARGB)` alpha 混合 | `RenderSystem.enableBlend()` |
| Canvas 粒子 | `IntroParticleEngine` + `GuiGraphics`/自定义 `VertexConsumer` | 逻辑照抄 JS |
| `requestAnimationFrame` | `Screen.render()` 每帧调用，`partialTick` 插值 | `elapsed = startTime - Util.getMillis()` |
| `window.location` | `client.setScreen(new MyUITitleScreen())` | 主线程 |
| keydown / click skip | `keyPressed` / `mouseClicked` | 800ms debounce |
| SVG stroke-dashoffset | 逐段 `GuiGraphics` 画线 + `dashProgress` | 或用预渲染 texture |
| `--dur: 8000ms` | `public static final int DURATION_MS = 8000` | 单源常量 |
| `prefers-reduced-motion` | Mod config / Minecraft accessibility options | 映射到 1000ms 短路径 |

---

## 3. 核心类职责

### 3.1 `IntroAnimationScreen`

```java
// 伪代码 — Agent 按目标 MC 版本调整 API
public class IntroAnimationScreen extends Screen {
    private long startTime;
    private boolean redirected;
    private final IntroTimeline timeline;
    private final IntroParticleEngine particles;
    private final IntroLayerRenderer layers;

    @Override
    protected void init() {
        this.startTime = Util.getMillis();
        particles.init(width, height);
    }

    @Override
    public void render(GuiGraphics g, int mouseX, int mouseY, float partialTick) {
        long elapsed = Util.getMillis() - startTime;
        if (!redirected && elapsed >= IntroTimeline.DURATION_MS) goToTitle();
        layers.render(g, elapsed, width, height);
        particles.render(g, elapsed, width, height);
        if (elapsed > 4000) drawSkipHint(g);
    }

    @Override
    public boolean keyPressed(int keyCode, ...) {
        if (Util.getMillis() - startTime > 800) goToTitle();
        return true;
    }

    private void goToTitle() {
        if (redirected) return;
        redirected = true;
        // 480ms fade 可在此 Screen 或下一 Screen 处理
        Minecraft.getInstance().setScreen(new MyUITitleScreen());
    }
}
```

### 3.2 `IntroTimeline`

- 读取 `timeline.json`（Gradle resource）或硬编码常量
- 提供：
  - `float progress(String trackId, long elapsedMs)` → 0..1
  - `float easedProgress(String trackId, long elapsedMs, EasingType type)`
  - 专用方法：`getIrisRadiusPct(elapsed)`、`getPanBlur(elapsed)`、`getVoidOpacity(elapsed)`

**插值示例（iris）：**

```java
// keyframes from timeline.json iris-open
float t = elapsed / 8000f;
float radius = t < 0.42f ? 0 :
    lerpKeyframes(t, new float[]{0.42f,0.58f,0.72f,0.88f,1f},
                      new float[]{0, 28, 58, 92, 150});
```

### 3.3 `IntroLayerRenderer`

按 DESIGN-SPEC §3 z-order 依次调用：

1. `renderPanoramaIris(g, elapsed)` — 绑定纹理 + clip
2. `renderOverlay(g, elapsed)`
3. `renderVoid(g, elapsed)`
4. `renderAmbientGlow(g, elapsed)`
5. `renderLightSweep(g, elapsed)`
6. `renderScanLine(g, elapsed)`
7. `renderLetterbox(g, elapsed)`
8. `renderVignette(g, elapsed)`
9. `renderFilmGrain(g, elapsed)`
10. `renderEmblem(g, elapsed)` — Logo + rings + wordmark
11. `renderBootHud(g, elapsed)`
12. `renderSuccessChip(g, elapsed)`
13. `renderExitFade(g, elapsed)`

每个方法只读 `elapsed`，无副作用（函数式）。

### 3.4 `IntroParticleEngine`

**直接从 HTML `<script>` 移植**，保持：

- 三类粒子：`burst` / `ambient` / `bokeh`
- 数量公式、`burstPhaseEndMs=3200`、`globalFadeStartMs=6800`
- `drawGlowArc` 用 `GuiGraphics.fill` + 渐变纹理或分段近似

MC 绘制光晕可选方案：
- 预生成 radial gradient 纹理 `particle_glow.png`
- 或 `LightTexture` + additive blend 多个半透明 quad

### 3.5 `IntroTokens`

```java
public final class IntroTokens {
    public static final int ACCENT        = 0xFF5AC8FA;
    public static final int ACCENT_72     = 0xB85AC8FA; // 72% alpha approx
    public static final int GLASS_BG      = 0x14FFFFFF;
    public static final int GLASS_STRONG  = 0x24FFFFFF;
    public static final int GLASS_BORDER  = 0x38FFFFFF;
    public static final int BG_DEEP       = 0xFF0A0E14; // oklch(12% 0.02 250) 近似
    public static final int SUCCESS       = 0xFF30D158; // 近似
    // ...
}
```

---

## 4. 分步任务清单（Todo 模板）

Agent 实现时逐步勾选：

### Phase A — 脚手架

- [ ] **A1** 创建 `IntroAnimationScreen` 空壳，可从 TitleScreen 手动触发
- [ ] **A2** 添加 `IntroTimeline`，常量 `DURATION_MS=8000`，实现 `easePremium` / `easeCinema`
- [ ] **A3** 导入 `timeline.json` 或等价常量类 `IntroTracks`

### Phase B — 背景与 Iris

- [ ] **B1** 加载 panorama 纹理（复用 MC 旋转全景或 Mod assets）
- [ ] **B2** 实现 `pan-sharpen`：blur 用 mipmap/多 pass downscale 模拟，或跳 blur 仅做 brightness/saturation
- [ ] **B3** 实现圆形 iris clip（scissor 正方形近似或 shader）
- [ ] **B4** `void` 黑幕 + `overlay` 遮罩 alpha 动画

### Phase C — 电影层

- [ ] **C1** `ambient-glow`：径向渐变 quad
- [ ] **C2** `light-sweep`：旋转渐变条带 translate
- [ ] **C3** `scan-line`：1px 水平线 + 位置插值
- [ ] **C4** `letterbox`：上下黑条 height 12vh→0
- [ ] **C5** `vignette`：全屏 radial 暗角
- [ ] **C6** `film-grain`：平铺 noise 纹理 alpha 0.045

### Phase D — Logo 与字标

- [ ] **D1** `emblem-glass` 120×120 圆角 panel（blur 可降级为半透明）
- [ ] **D2** SVG 三层 rect → `GuiGraphics` 描边或预渲染 `emblem.png`
- [ ] **D3** stroke-dash 动画（dashProgress 0→1）
- [ ] **D4** 双扩散环 scale + fade
- [ ] **D5** shimmer 高光扫过
- [ ] **D6** MyUI 逐字 reveal（4 字符，140ms stagger）
- [ ] **D7** tagline + accent 细线

### Phase E — Boot HUD

- [ ] **E1** 左下 glass log 面板 + 4 行 mono 文案（时间点见 timeline.json）
- [ ] **E2** 右下 progress ring（SVG arc → `GuiGraphics` arc 或 texture）
- [ ] **E3** 「加载中」标签
- [ ] **E4** HUD 3.4s 入、6.4s 出

### Phase F — 粒子

- [ ] **F1** 移植 `initParticles` / spawn 函数
- [ ] **F2** 移植 `drawParticles` 主循环
- [ ] **F3** `drawGlowArc` 光晕
- [ ] **F4** resize 处理（窗口变化重新 init）

### Phase G — 完成与衔接

- [ ] **G1** 「准备就绪」success chip @ 6600ms
- [ ] **G2** exit-fade @ 7400ms
- [ ] **G3** @ 8000ms `setScreen(MyUITitleScreen)`
- [ ] **G4** skip：key/mouse + 800ms debounce
- [ ] **G5** reduced-motion 1000ms 短路径
- [ ] **G6** 首次启动自动显示 Intro；TitleScreen 加重播入口

### Phase H — 集成

- [ ] **H1** 在 `MyUIClient` 或 Mixin `TitleScreen` 注入入口逻辑
- [ ] **H2** lang 文件：`intro.loading`、`intro.ready`、`intro.skip`
- [ ] **H3** config：`enableIntroAnimation`、`reducedMotion`

---

## 5. 启动入口策略

**推荐方案 A（Mixin）：**

```java
@Mixin(TitleScreen.class)
public class TitleScreenMixin {
    @Inject(method = "init", at = @At("HEAD"), cancellable = true)
    private void myui$maybeShowIntro(CallbackInfo ci) {
        if (IntroScreenHandler.shouldPlayIntro()) {
            Minecraft.getInstance().setScreen(new IntroAnimationScreen());
            ci.cancel(); // 或不要 cancel，改为 next tick 切换
        }
    }
}
```

**方案 B：** 在 Mod 初始化完成后 `client.execute(() -> setScreen(new IntroAnimationScreen()))`

`IntroScreenHandler.shouldPlayIntro()`：
- 首次启动 true
- 读 config / 本地 NBT 标记
- 「重播入场」按钮清除标记或 force flag

---

## 6. 平台限制与降级策略

| 效果 | 理想实现 | 可接受降级 | 不可省略 |
|------|----------|------------|----------|
| 毛玻璃 blur | 自定义 shader | 更高 alpha 半透明，无 blur | 半透明 + 边框 |
| Iris clip | Stencil / shader | 方形 scissor 近似 | 必须有「揭示」感 |
| Panorama blur | Shader gaussian | 多档 mipmap 缩放 | brightness 0.7→1 |
| Film grain | noise texture | 省略 | — |
| 粒子 bokeh | additive glow | 简化为小圆点 | 必须有中心 burst |
| SVG stroke-draw | 矢量 | 静态 Logo fade-in | Logo 必须可见 |
| 8s 时序 | 精确 | ±200ms | 四幕结构 |

---

## 7. Acceptance Criteria（必须全部通过）

### 功能

1. 冷启动（或 force）播放完整入场，**8 秒后**进入主菜单
2. 任意键 / 鼠标点击（**800ms 后**）跳过至主菜单
3. 主菜单可触发「重播入场」
4. 无「注入 / inject」文案

### 视觉（对照 HTML 录屏）

5. 0–1s：黑屏 + 扫描线/光晕
6. 1–3s：Logo 弹入 + 描边动画 + MyUI 逐字
7. 3–5s：圆形 iris 打开 + 背景变清晰
8. 3.4–6.4s：Boot log 逐行 + 进度环
9. 6.6s：「准备就绪」chip
10. 7.4s：淡出转场

### 技术

11. 不阻塞游戏主线程 > 16ms/frame（粒子数可调）
12. 1920×1080 与 854×480 GUI scale 均可读
13. 内存：粒子池固定大小，无每帧 new 大量对象

---

## 8. 测试计划

```markdown
1. 正常播放：录屏 8s，与 HTML 原型并排对比
2. Skip：500ms 点击无效，1000ms 点击有效
3. 窗口 resize：粒子不崩溃
4. GUI Scale 1/2/3/4：Logo 居中、HUD 不裁切
5. reduced-motion：1s 内进主菜单
6. 重播：从 TitleScreen 再次进入 Intro
7. 性能：F3 debug 下 FPS ≥ 30（中端机器）
```

---

## 9. 资源导出清单（从 HTML 提取）

Agent 可从 Open Design 项目复制到 Mod `assets/`：

| 源 | 目标 | 说明 |
|----|------|------|
| HTML panorama SVG | `textures/gui/panorama.png` | 导出 1920×1080 PNG |
| Emblem SVG | `textures/gui/emblem.png` @2x | 可选，简化描边动画 |
| film grain SVG | `textures/gui/noise_tile.png` | 128×128 平铺 |
| `tokens.json` | `IntroTokens.java` | 颜色常量 |
| `timeline.json` | `src/main/resources/myui/intro_timeline.json` | 运行时读取 |

---

## 10. Cursor Agent Prompt 模板

将以下内容粘贴到 **Mod 代码库** 的新 Cursor 会话：

---

**任务：** 将 MyUI 入场动画从 Open Design 原型搬运到本 Fabric Mod。

**参考包（从设计仓库复制 `docs/intro-animation/` 整个目录 + `myui-intro-screen.html` 到 Mod）：**
- `AGENT-BRIEF.md` — **从这里开始**
- `DESIGN-SPEC.md` — 设计规格
- `PORT-PLAN.md` — 本文件
- `timeline.json` / `layer-map.json` / `particles-spec.json` / `tokens-intro.json`
- `easing-reference.js` — 缓动参考
- `myui-intro-screen.html` — 视觉源

**要求：**
1. 严格遵循 8000ms 时间轴与四幕结构
2. 按 PORT-PLAN §4 任务清单顺序实现
3. CSS 效果按 §2 映射表转为 MC 渲染
4. 粒子系统逻辑从 HTML `<script>` 原样移植
5. 完成后满足 §7 Acceptance Criteria

**约束：**
- 使用项目现有包名与 MC 版本 API
- 不引入「注入」相关文案
- 毛玻璃 blur 不可用 shader 时用 §6 降级方案

**第一步：** 扫描本仓库是否已有 `TitleScreen` / Screen 基类，创建 `IntroAnimationScreen` 空壳并注册测试入口。

---

## 11. 版本同步

| 设计仓库文件 | 变更时动作 |
|--------------|------------|
| `myui-intro-screen.html` | 更新 `timeline.json` + 重新录屏对比 |
| 文案变更 | 同步 `lang/zh_cn.json` |
| 时长变更 | 改 `DURATION_MS` + 所有 track |

**当前同步版本：** v2 · 8000ms · 2026-06-30
