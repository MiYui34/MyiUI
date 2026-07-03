# MyUI 入场动画 — Agent 文档索引

> **目标读者：** Cursor Agent / 任何需要将 HTML 原型搬运到 Minecraft Fabric Mod 的开发者。  
> **唯一视觉参考源（Source of Truth）：** 项目根目录 `myui-intro-screen.html`（与 `intro-screen.html` 内容同步）。

---

## 快速启动（复制给 Cursor Agent）

```
请阅读以下文档，将 MyUI 入场动画完整搬运到 Fabric Mod：

1. docs/intro-animation/AGENT-BRIEF.md     ← 总纲（从这里开始）
2. docs/intro-animation/DESIGN-SPEC.md       — 设计规格（层叠、时序、Token、粒子算法）
3. docs/intro-animation/PORT-PLAN.md         — Fabric 实现规划（类结构、渲染映射、任务清单）
4. docs/intro-animation/timeline.json        — 机器可读时间轴（8000ms 主时钟）
5. docs/intro-animation/layer-map.json       — DOM 层 z-order + 布局锚点
6. docs/intro-animation/particles-spec.json  — 粒子系统完整算法
7. docs/intro-animation/tokens-intro.json    — Java ARGB 颜色 + 尺寸 + 文案
8. docs/intro-animation/easing-reference.js  — 缓动函数（可直译 Java）
9. myui-intro-screen.html                    — 可运行 HTML 原型（对照实现）
10. DESIGN.md + tokens.json                  — MyUI 全局设计系统

验收标准见 AGENT-BRIEF.md §4 与 PORT-PLAN.md §7。
完成后动画应：总时长 8s、可跳过、结束后进入 TitleScreen、视觉与原型偏差 < 5%。
```

**Mod 仓库会话 Prompt 模板：** 见 [AGENT-BRIEF.md §5](./AGENT-BRIEF.md#5-粘贴到-mod-仓库的-cursor-prompt)

---

## 文档结构

| 文件 | 用途 |
|------|------|
| **[AGENT-BRIEF.md](./AGENT-BRIEF.md)** | **Agent 入口总纲** — 执行顺序、映射速查、验收、Prompt |
| [DESIGN-SPEC.md](./DESIGN-SPEC.md) | 设计意图、视觉层、组件规格、缓动、文案、粒子系统 |
| [PORT-PLAN.md](./PORT-PLAN.md) | Fabric 架构、类清单、CSS→MC 映射、分步任务、风险与降级 |
| [timeline.json](./timeline.json) | 各元素 start/duration/easing 的 JSON 时间轴 |
| [layer-map.json](./layer-map.json) | 渲染层 z-index、选择器、布局、track 绑定 |
| [particles-spec.json](./particles-spec.json) | burst/ambient/bokeh 算法 + drawGlowArc |
| [tokens-intro.json](./tokens-intro.json) | ARGB 常量、尺寸、文案、lang key |
| [easing-reference.js](./easing-reference.js) | easePremium / easeCinema + keyframe 插值 |

---

## 流程关系

```
index.html / 游戏启动
       ↓
intro-screen（本动画，8s）
       ↓ 自动过渡 / 跳过
title-screen.html（主菜单，含「重播入场」）
       ↓ 180ms crossfade（motion.json）
singleplayer / multiplayer / options-*
```

入场动画 **不使用** 菜单系统的 180ms crossfade；那是主菜单子页之间的过渡。

---

## 版本

| 项 | 值 |
|---|---|
| 动画版本 | v2（电影感四幕，8 秒） |
| 最后同步源文件 | `myui-intro-screen.html` |
| 文案 | 加载中 / 准备就绪（无「注入」字样） |
| 同步日期 | 2026-06-30 |
