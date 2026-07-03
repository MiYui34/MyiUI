# MyUI 入场动画 — Cursor Agent 搬运总纲

> **读者：** 在 Mod 代码库中工作的 Cursor Agent。  
> **目标：** 将 Open Design 原型 `myui-intro-screen.html` **完整、可验收地** 搬运到 Minecraft Fabric Mod，结束后进入 `MyUITitleScreen`（对应原型 `title-screen.html`）。  
> **禁止：** 重新设计时序、删减四幕结构、添加「注入/inject」文案。

---

## 0. 30 秒理解

| 项 | 值 |
|---|---|
| 总时长 | **8000 ms** |
| 视觉风格 | 深色画布 + `#5AC8FA` 毛玻璃 + 电影感四幕 |
| 结束行为 | 自动进入主菜单；800 ms 后可跳过 |
| 唯一视觉源 | 项目根目录 `myui-intro-screen.html` |
| 机器时间轴 | `docs/intro-animation/timeline.json` |
| 菜单衔接 | `title-screen.html`（含「重播入场」链接） |

**四幕叙事：** 觉醒（黑屏光扫）→ 标识（Logo + 字标）→ 穿透（虹膜 + 全景渐清 + HUD）→ 完成（准备就绪 → 淡出）

---

## 1. 必读文件（按顺序）

```
1. docs/intro-animation/AGENT-BRIEF.md     ← 本文件（总纲）
2. docs/intro-animation/DESIGN-SPEC.md     ← 设计规格（层叠、组件、粒子）
3. docs/intro-animation/PORT-PLAN.md       ← Fabric 类结构 + 任务清单 + 验收
4. docs/intro-animation/timeline.json      ← 8000ms 各 track 关键帧
5. docs/intro-animation/layer-map.json     ← DOM 层 + z-index + 布局锚点
6. docs/intro-animation/particles-spec.json ← 粒子算法（从 HTML script 提取）
7. docs/intro-animation/tokens-intro.json  ← Java ARGB 颜色常量
8. docs/intro-animation/easing-reference.js ← 缓动函数（可直译 Java）
9. myui-intro-screen.html                  ← 像素级参考（CSS + JS 源码）
10. DESIGN.md + tokens.json                ← MyUI 全局设计系统
11. title-screen.html                      ← 动画结束跳转目标
```

---

## 2. Agent 执行流程（硬性顺序）

### Step 1 — 侦察目标仓库

- 定位现有 `Screen` 基类、包名、MC 版本（1.20.x / 1.21.x 等）
- 确认是否已有自定义 `TitleScreen` 或 Mixin 注入点
- 确认 panorama 来源：原版旋转全景 / Mod 纹理 / 静态 PNG

### Step 2 — 脚手架（Phase A）

创建以下类（包名按目标仓库调整）：

```
screen/IntroAnimationScreen.java      // 主 Screen
screen/intro/IntroTimeline.java       // 8000ms 时钟 + track 插值
screen/intro/IntroLayerRenderer.java  // 按 layer-map.json z-order 绘制
screen/intro/IntroParticleEngine.java // 移植 particles-spec.json
screen/intro/IntroEasing.java         // easePremium / easeCinema
screen/intro/IntroTokens.java         // 来自 tokens-intro.json
client/IntroScreenHandler.java        // 首次启动 / 重播逻辑
```

**第一步验收：** 空 Screen 可手动打开，黑底 + 居中占位文字，按 Esc 可退出。

### Step 3 — 时间轴驱动（Phase A–B）

- 常量 `DURATION_MS = 8000`（与 `timeline.json` 一致）
- 实现 `IntroTimeline.progress(trackId, elapsedMs)` 返回 0..1
- 实现 `IntroTimeline.sampleKeyframes(trackId, elapsedMs)` 用于 iris / pan-sharpen / void-fade
- 缓动：从 `easing-reference.js` 移植 `easePremium` / `easeCinema`

**不要** 用 CSS animation；**必须** 每帧根据 `elapsedMs` 计算属性。

### Step 4 — 背景层（Phase B）

按 `layer-map.json` 从 z=0 向上绘制：

1. **panorama-iris** — 圆形 clip 揭示（0%→150% radius），见 `timeline.json` → `iris-open`
2. **pan-sharpen** — blur 48→0、brightness 0.7→1、scale 1.18→1.05
3. **overlay** — `@5200ms` 淡入 `bg-overlay`
4. **void** — 黑幕 opacity 1→0（38%/58%/100% 关键帧）

降级：MC 无 CSS blur → 用 mipmap 缩放或跳 blur，保留 brightness/saturation 动画。

### Step 5 — 电影层（Phase C）

| 层 | 开始 | 要点 |
|---|---|---|
| ambient-glow | 400ms | 中心 radial cyan，呼吸 scale 0.85→1.08 |
| light-sweep | 300ms | 105° 渐变带，translateX -60%→60% |
| scan-line | 500ms | 1px 水平线 top -1→100% |
| letterbox | 4200ms 出 | 上下各 12vh 黑条 → height 0 |
| vignette | 2800ms | radial 暗角 opacity→0.85 |
| film-grain | 800ms | noise 纹理 alpha 0.045（可省略） |

### Step 6 — Logo 与字标（Phase D）

- **emblem-glass** 120×120，圆角 30px，glass-bg-strong + 2px border
- **SVG 三层 rect** 描边动画：dashoffset 120→0，各 path 延迟见 `timeline.json` → `stroke-draw`
- **双扩散环** 140px / 180px，scale 0.4→2.8，opacity 0→0.65→0
- **shimmer** 1800ms 高光带横向扫过
- **MyUI 逐字** 2000ms 起，间隔 140ms，translateY 120%→0
- **tagline** `Fabric UI Mod`，2700ms
- **accent 细线** 48px，2900ms scaleX 0→1
- **emblem-out** 5200ms 淡出

降级：SVG stroke-draw → 静态 emblem 纹理 + fade-in。

### Step 7 — Boot HUD（Phase E）

- **3400ms** 自底部 translateY 20→0 进入
- **左下 boot-log** max-width 440px，mono 11px，4 行按 3600/4200/4800/5600ms 出现
- **右下 progress-ring** 圆周长 169，0–8000ms dashoffset 169→0
- **标签** 「加载中」
- **6400ms** HUD 淡出

### Step 8 — 粒子（Phase F）

**完整算法见 `particles-spec.json`**，逻辑与 `myui-intro-screen.html` `<script>` 一致：

- 三类：`burst` / `ambient` / `bokeh`
- 数量：`min(48, width/18)` / `min(35, width/28)` / `min(18, width/50)`
- `burstPhaseEndMs=3200`，`globalFadeStartMs=6800`
- `drawGlowArc` 径向渐变光晕

### Step 9 — 完成与衔接（Phase G）

| 时间 | 事件 |
|---|---|
| 6600ms | 「准备就绪」success chip 闪现 |
| 6800ms | 粒子 globalFade 开始 |
| 7400ms | exit-fade 全屏 `--bg-deep` |
| 8000ms | `setScreen(MyUITitleScreen)` |

**跳过：** key/mouse，`elapsed > 800ms` → 480ms exit-fade → TitleScreen  
**reduced-motion：** 1000ms 短路径

### Step 10 — 集成（Phase H）

- 冷启动：`IntroScreenHandler.shouldPlayIntro()` → 先播 Intro
- 主菜单：`title-screen.html` 页脚「重播入场」→ 对应 Mod 入口
- lang：`intro.loading` / `intro.ready` / `intro.skip`

---

## 3. Web → Minecraft 映射速查

| HTML/CSS | Fabric 实现 |
|---|---|
| `@keyframes` + `animation-delay` | `IntroTimeline` + `elapsedMs` 插值 |
| `clip-path: circle()` | Stencil / scissor 近似 / shader |
| `backdrop-filter: blur` | 半透明面板（无 blur）或自定义 shader |
| `canvas` 粒子 | `IntroParticleEngine` + `GuiGraphics` |
| `requestAnimationFrame` | `Screen.render()` 每帧 |
| `window.location.href` | `client.setScreen(new MyUITitleScreen())` |
| `:root` CSS 变量 | `IntroTokens.java` |
| `prefers-reduced-motion` | Mod config / MC 无障碍选项 |

完整映射表见 `PORT-PLAN.md` §2。

---

## 4. 验收标准（全部通过才算完成）

### 功能

- [ ] 冷启动播放完整 8s 动画后进入主菜单
- [ ] 800ms 后任意键/点击可跳过
- [ ] 主菜单可重播入场
- [ ] 无「注入 / inject」文案

### 视觉（与 HTML 录屏并排对比）

- [ ] 0–1.4s：黑屏 + 扫描线 + 环境光
- [ ] 1.2–5.2s：Logo 弹入 + 描边 + MyUI 逐字 + 双环
- [ ] 3.4–6.4s：圆形 iris + 全景渐清 + Boot HUD
- [ ] 6.6s：「准备就绪」chip
- [ ] 7.4s：淡出转场

### 技术

- [ ] 主线程 ≤ 16ms/frame（粒子数可调）
- [ ] GUI Scale 1–4 下 Logo 居中、HUD 不裁切
- [ ] 粒子池固定大小，无每帧大量 alloc

---

## 5. 粘贴到 Mod 仓库的 Cursor Prompt

将 `docs/intro-animation/` 整个目录复制到 Mod 根目录后，在新 Cursor 会话粘贴：

```
任务：将 MyUI 入场动画从 Open Design 原型完整搬运到本 Fabric Mod。

必读（按顺序）：
1. docs/intro-animation/AGENT-BRIEF.md
2. docs/intro-animation/DESIGN-SPEC.md
3. docs/intro-animation/PORT-PLAN.md
4. docs/intro-animation/timeline.json
5. docs/intro-animation/layer-map.json
6. docs/intro-animation/particles-spec.json
7. myui-intro-screen.html（视觉源，与 intro-screen.html 同步）

要求：
- 严格 8000ms 四幕时序，不重新设计
- 按 AGENT-BRIEF.md §2 步骤顺序实现
- 粒子算法原样移植 particles-spec.json
- 毛玻璃 blur 不可用时按 PORT-PLAN.md §6 降级
- 完成后满足 AGENT-BRIEF.md §4 全部验收项

第一步：扫描本仓库 Screen 结构，创建 IntroAnimationScreen 空壳并注册测试入口。
```

---

## 6. 与菜单系统的关系

入场动画 **不属于** 菜单 crossfade 体系（`motion.json` 的 180ms page_transition 仅用于 `title-screen` ↔ 子页）。

```
游戏启动
  → IntroAnimationScreen（8s，本动画）
  → MyUITitleScreen / title-screen.html
  → singleplayer / multiplayer / options-* （180ms crossfade）
```

菜单 HTML 与 JSON schema 见项目根目录 `components.json`、`screens/*.json`、`css/menu-system.css`。

---

## 7. 版本

| 项 | 值 |
|---|---|
| 动画版本 | v2 · 电影感四幕 · 8s |
| 源文件 | `myui-intro-screen.html` |
| 同步日期 | 2026-06-30 |
| 文案 | 加载中 / 准备就绪（无注入字样） |

源文件变更时：同步更新 `timeline.json` → 重新录屏对比 → 更新 `IntroTimeline.DURATION_MS`。
