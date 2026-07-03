# MyUI 入场动画 — 设计规格书

> **Agent 指令：** 实现任何一帧前，先读 `timeline.json` 与 `myui-intro-screen.html`。本文档解释「为什么」与「精确参数」；HTML 是像素级参考。

---

## 1. 产品意图

| 项 | 值 |
|---|---|
| 场景 | Mod 加载完成、进入游戏主菜单之前 |
| 时长 | **8000ms**（`--dur`） |
| 调性 | 电影感、高级感；MyUI 毛玻璃 + 单一 cyan accent |
| 叙事 | 黑屏觉醒 → Logo 显现 → 全景虹膜穿透 → 就绪 → 入主菜单 |
| 结束行为 | 自动淡入 `TitleScreen`；任意键/点击可跳过（800ms 防抖） |
| 无障碍 | `prefers-reduced-motion` 时压缩至 ~1s 后跳转 |

**禁止文案：** 不出现「注入」「inject」等字样（已改为「加载中」「准备就绪」）。

---

## 2. 设计 Token（必须与 MyUI 一致）

来源：`tokens.json` / `DESIGN.md` / `:root` in HTML。

### 2.1 颜色

| Token | 值 | 用途 |
|-------|-----|------|
| `--accent` | `#5AC8FA` | 描边、进度环、accent 粒子 |
| `--accent-72` | `rgba(90,200,250,0.72)` | 渐变线、扫描线中心 |
| `--accent-24` | `rgba(90,200,250,0.24)` | 光晕、环边框 |
| `--accent-12` | `rgba(90,200,250,0.12)` | Logo 外发光 |
| `--bg-deep` | `oklch(12% 0.02 250)` | 退出淡入底色 |
| `--bg-overlay` | `rgba(8,12,20,0.55)` | 全景遮罩 |
| `--glass-bg` | `rgba(255,255,255,0.08)` | HUD 日志面板 |
| `--glass-bg-strong` | `rgba(255,255,255,0.14)` | Logo 卡片、进度面板 |
| `--glass-border` | `rgba(255,255,255,0.22)` | 2px 描边 |
| `--glass-border-accent` | `rgba(90,200,250,0.45)` | 焦点/扩散环 |
| `--fg` | `oklch(96% 0.005 250)` | 主文字 |
| `--fg-muted` | `oklch(72% 0.01 250)` | 「加载中」标签 |
| `--fg-dim` | `oklch(52% 0.012 250)` | 日志、跳过提示 |
| `--success` | `oklch(68% 0.14 155)` | 就绪 chip 边框/圆点 |

### 2.2 缓动

| Token | 曲线 | 用于 |
|-------|------|------|
| `--ease-premium` | `cubic-bezier(0.16, 1, 0.3, 1)` | 主时间轴、Logo、虹膜、退出 |
| `--ease-cinema` | `cubic-bezier(0.77, 0, 0.175, 1)` | 扫描线、光扫、shimmer |
| `--spring` | `cubic-bezier(0.34, 1.56, 0.64, 1)` | 交互态（本屏较少） |

### 2.3 字体

| 角色 | Stack |
|------|-------|
| Display | `-apple-system, 'SF Pro Display', 'Segoe UI', system-ui, sans-serif` |
| Body | `-apple-system, 'SF Pro Text', 'Segoe UI', system-ui, sans-serif` |
| Mono | `'SF Mono', 'JetBrains Mono', ui-monospace, monospace` |

MC 移植：使用 Mod 内嵌 TTF 或 `Minecraft.DEFAULT` + 自定义 `Font` 近似；Mono 用于 boot log。

### 2.4 圆角

- Logo 容器：`30px`（120×120 方块）
- Glass 面板：`--radius-md` 14px / `--radius-lg` 20px

---

## 3. 渲染层叠（Z-Order，从底到顶）

```
z=0   .panorama-wrap > .panorama     全景背景（被 iris clip）
z=0   .overlay                       深色遮罩（5.2s 后淡入）
z=1   .void                          纯黑幕（0→58% 渐隐）
z=2   .ambient-glow, .light-sweep    环境光
z=3   .scan-line                     水平扫描线
z=4   .letterbox-top/bottom          宽银幕黑边（4.2s 收起）
z=5   .vignette                      暗角
z=6   .film-grain                    胶片颗粒（opacity 0.045）
z=7   #particles (canvas)            粒子
z=8   .stage > .emblem-wrap           Logo + 字标
z=9   .boot-hud                       左下日志 + 右下进度环
z=10  .success-chip                   「准备就绪」
z=11  .skip-hint                      跳过提示（opacity 0.35）
z=12  .exit-fade                      退出全屏淡入
```

**Agent 注意：** Minecraft `Screen` 中按此顺序绘制；后绘制的在上层。

---

## 4. 四幕时间轴（8000ms 主时钟）

百分比基于 `--dur = 8000ms`。完整 JSON 见 `timeline.json`。

### 幕 I — 觉醒（0–1.4s）

| 元素 | 开始 | 时长 | 行为 |
|------|------|------|------|
| `.void` | 0 | 8000ms | opacity 1→0（38% 前全黑，58% 时 0.65，100% 时 0） |
| `.ambient-glow` | 400ms | 3200ms | 中心 radial cyan 呼吸，peak opacity 1 →  settle 0.35 |
| `.light-sweep` | 300ms | 2200ms | 105° 斜向高光带，translateX -60%→60% |
| `.scan-line` | 500ms | 1400ms | 1px 线从 top:-1 → top:100%，accent 渐变 + glow |
| `.film-grain` | 800ms | 1600ms | SVG noise overlay，最终 opacity **0.045** |

### 幕 II — 标识（1.2–5.2s）

| 元素 | 开始 | 时长 | 行为 |
|------|------|------|------|
| `.emblem-wrap` 进入 | 1200ms | 1600ms | scale 0.78→1.02→1，translateY 12→-4→0 |
| `.emblem-ring--1` | 1400ms | 2200ms | 140px 圆环 scale 0.4→2.8，opacity 0→0.65→0 |
| `.emblem-ring--2` | 1800ms | 2600ms | 180px，边框更淡 |
| `.emblem-glass` | — | — | 120×120，blur 28px，border 2px，阴影见 HTML |
| `.emblem-glass::after` shimmer | 1800ms | 2800ms | 高光带横向扫过 |
| SVG stroke-draw | 1500ms | 1800ms | dashoffset 120→0，各 path 延迟 1500/1650/1780/1950ms |
| 字标 M/y/U/I | 2000ms 起 | 各 900ms | 逐字 translateY 120%→0，间隔 **140ms** |
| `.wordmark-line` | 2900ms | 1000ms | 48px 宽 accent 细线 scaleX 0→1 |
| Tagline | 2700ms | 800ms | 「Fabric UI Mod」，11px uppercase tracking 0.08em |
| `.emblem-wrap` 退出 | 5200ms | 1200ms | opacity 0，scale 0.96，translateY -32 |

### 幕 III — 穿透（3.36–6.4s）

| 元素 | 开始 | 行为 |
|------|------|------|
| `.panorama-wrap` iris | 42% (3360ms) 起 | `clip-path: circle()` 0%→28%→58%→92%→150% |
| `.panorama` sharpen | 同步 | blur 48→28→10→0；scale 1.18→1.05；saturate/brightness 恢复 |
| `.letterbox` | 4200ms | 12vh 黑边 height→0 |
| `.vignette` | 2800ms | radial 暗角 opacity→0.85 |
| `.overlay` | 5200ms | bg-overlay opacity→1 |
| `.boot-hud` 进入 | 3400ms | translateY 20→0 |
| Boot log 4 行 | 3600/4200/4800/5600ms | 逐行淡入；末行 `--success` 色 |
| Progress ring | 0–8000ms | stroke-dashoffset 169→0 |
| `.boot-hud` 退出 | 6400ms | opacity 0 |

### 幕 IV — 完成（6.6–8s）

| 元素 | 开始 | 行为 |
|------|------|------|
| `.success-chip` | 6600ms | 「准备就绪」+ 绿色 pulse dot，1400ms flash |
| 粒子 globalFade | 6800ms | 1200ms 内 alpha→0 |
| `.exit-fade` | 7400ms | `--bg-deep` 全屏 opacity→1 |
| 跳转 | 8000ms | → TitleScreen |

---

## 5. 组件规格

### 5.1 Logo Emblem（SVG 64×64 viewBox）

三层堆叠圆角矩形 + 装饰线 + 圆点：

```
Layer 1: rect x=10 y=14 w=28 h=36 rx=6  stroke rgba(255,255,255,0.35) w=2
Layer 2: rect x=18 y=10 w=28 h=36 rx=6  stroke rgba(255,255,255,0.55) w=2  delay +150ms
Layer 3: rect x=26 y=6  w=28 h=36 rx=6  stroke #5AC8FA w=2.5              delay +280ms
Lines:   path M34 22h12 / M34 30h8       stroke #5AC8FA                     delay +450ms
Dot:     circle cx=50 cy=18 r=2.5 fill #5AC8FA                            delay +600ms
```

**stroke-dasharray / dashoffset：** 120 → 0，模拟描边绘制。

### 5.2 Wordmark

- 文本：`MyUI`（4 个 `<span>` 独立动画）
- 字号：`clamp(40px, 7vw, 58px)`，weight 600，letter-spacing **-0.03em**
- 副标：`Fabric UI Mod`，11px，uppercase，tracking **0.08em**

### 5.3 Boot HUD

**左：Boot Log 面板**

- 尺寸：max-width 440px，padding 16×18
- 字体：mono 11px，line-height 1.7
- 前缀：`>` 使用 `--accent`
- 4 行文案（顺序固定）：
  1. `init fabric-renderer … ok`
  2. `load glass-shader pipeline`
  3. `bind title-screen overlay`
  4. `ready — entering menu`（success 色）

**右：Progress Panel**

- 圆环：60×60 SVG，track `rgba(255,255,255,0.06)`，fill `--accent`，stroke-width 2.5
- 圆周长：**169**（r=27）
- 标签：「加载中」，11px uppercase tracking 0.06em

### 5.4 Success Chip

- 文案：「准备就绪」
- padding 16×28，border `rgba(48,209,88,0.3)`，green dot 8px + glow
- 居中 flash 后淡出

### 5.5 全景背景

- 内容：Minecraft 风格 panorama SVG（见 HTML data URI）或游戏内 `PanoramaRenderer` 纹理
- 初始：`blur(48px) saturate(0.5) brightness(0.7) scale(1.18)`
- 8s 后：`blur(0) saturate(1.05) brightness(1) scale(1.05)`
- 8.2s 后可选慢速 drift 动画（80s 周期，移植时可省略）

### 5.6 Iris Reveal

- 形状：圆形 clip，中心 (50%, 50%)
- 关键半径（占 viewport 百分比）：0 → 0 → 28 → 58 → 92 → 150

---

## 6. Canvas 粒子系统

> 源码：`myui-intro-screen.html` `<script>` 块，`#particles` canvas。

### 6.1 初始化

```javascript
burstCount  = min(48, floor(innerWidth / 18))
ambientCount = min(35, floor(innerWidth / 28))
bokehCount   = min(18, floor(innerWidth / 50))
dpr          = min(devicePixelRatio, 2)
```

### 6.2 粒子类型

| type | 行为 |
|------|------|
| `burst` | 从屏幕中心放射，带拖尾线；3.2s 后减速；出界重生 |
| `ambient` | 随机漂移向上，小圆点 |
| `bokeh` | 大光斑 + radial gradient glow，sin 呼吸 alpha |

### 6.3 每帧逻辑（requestAnimationFrame）

1. `elapsed = timestamp - startTime`
2. `globalFade`：elapsed > 6800 时，`1 - (elapsed-6800)/1200`
3. `burstPhase`：elapsed < 3200 → 1，否则线性降至 0
4. 更新 bokeh 位置；绘制 `drawGlowArc`
5. 更新 burst/ambient；burst 相同时画拖尾线
6. elapsed < 5200 且 burstPhase > 0.1：中心 `drawGlowArc(cx,cy, 40*breath, 0.12*burstPhase*globalFade)`
7. elapsed < DURATION+500 继续 RAF

### 6.4 drawGlowArc

```javascript
radialGradient: center alpha*0.35 → 0.4处 alpha*0.08 → edge 0
radius: r * 4
color: rgba(90, 200, 250, …) 或 white for non-accent
```

### 6.5 颜色

- accent 粒子：`rgba(90, 200, 250, alpha * 0.9)`，约 20% 粒子
- 普通：`rgba(255, 255, 255, alpha * 0.55)`

---

## 7. 交互与状态机

```
[INIT] → 播放动画
   ├─ keydown / click (elapsed > 800ms) → [SKIP] → goToMenu()
   ├─ timeout 8000ms → [COMPLETE] → goToMenu()
   └─ prefers-reduced-motion → timeout 1000ms → goToMenu()

goToMenu():
  - redirected = true
  - cancelAnimationFrame
  - body.is-skipping（所有 animation duration → 0.01ms）
  - exit-fade opacity 1, transition 480ms
  - setTimeout 500ms → TitleScreen
```

---

## 8. 响应式

| 断点 | 调整 |
|------|------|
| ≤640px | boot-hud 纵向堆叠；letterbox 8vh |
| reduced-motion | 隐藏 scan/letterbox/sweep/grain；iris+pan 900ms；其余 400ms |

---

## 9. 与主菜单衔接

- HTML 原型：`window.location.href = 'title-screen.html'`
- Fabric：`client.setScreen(new MyUITitleScreen())`
- 建议：首次启动播放；`title-screen` 页脚提供「重播入场」入口（已实现于 `title-screen.html`）

---

## 10. 验收检查清单（设计侧）

- [ ] 总时长 8s ± 100ms
- [ ] 无任何「注入/inject」文案
- [ ] Accent 色仅 `#5AC8FA` 系，无 indigo 默认色
- [ ] Logo 描边动画 + 逐字 MyUI + 双扩散环可见
- [ ] 全景 iris 穿透 + blur 渐清可感知
- [ ] Boot log 4 行按时间出现；进度环与 8s 同步
- [ ] 「准备就绪」chip 在 6.6s 闪现
- [ ] 跳过有效；reduced-motion 1s 跳转
- [ ] 退出后进入主菜单，无闪屏

---

## 11. 参考文件

| 文件 | 说明 |
|------|------|
| **`AGENT-BRIEF.md`** | **Cursor Agent 入口总纲** |
| `myui-intro-screen.html` | **主参考实现** |
| `timeline.json` | 机器可读时间轴 |
| `layer-map.json` | 渲染层 z-order + 布局 |
| `particles-spec.json` | 粒子算法规格 |
| `tokens-intro.json` | Java ARGB + 文案 |
| `easing-reference.js` | 缓动函数参考 |
| `PORT-PLAN.md` | Fabric 搬运规划 |
| `DESIGN.md` | MyUI 全局设计系统 |
| `tokens.json` | Token 导出 |
| `title-screen.html` | 动画结束目标页 |
