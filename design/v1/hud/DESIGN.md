# MyiUI 游戏内 HUD 设计说明

> **模组**：MyiUI · Fabric 1.21.6  
> **视觉体系**：Glass v2（延续主菜单）  
> **交付物**：`hud-gameplay.html` · `hud-spec.json` · `screens/` · `css/hud-system.css`

---

## 1. 设计目标

将主菜单的毛玻璃 Glass v2 语言延伸至**游戏内 HUD**，替换原版红心/鸡腿图标排列，改用**渐变填充轨道 + 半透明面板**，同时完整兼容 **AppleSkin** 模组的饱和度/耗尽/食物 tooltip 数据层。

设计原则：
- **功能性优先**：信息可读性不低于原版，数值标签可选开启
- **艺术性平衡**：毛玻璃 + 细描边 + 单一 accent 光晕，避免厚重实体条
- **Mod 友好**：CSS 变量 + data 属性驱动，Fabric 运行时注入即可

---

## 2. 视觉 Token（对齐主菜单）

| Token | 值 | 用途 |
|-------|-----|------|
| `--myiui-accent` | `#5AC8FA` | 选中槽位、氧气条、tooltip 高亮 |
| `--myiui-glass-bg` | `rgba(255,255,255,0.08)` | HUD 面板底 |
| `--myiui-glass-border` | `rgba(255,255,255,0.22)` | 面板描边 |
| `--myiui-danger` | `oklch(62% 0.18 25)` | 低血、低耐久 |
| `--myiui-success` | `oklch(68% 0.14 155)` | 满血、耐久良好 |
| `--myiui-radius-md` | `14px` | 状态面板、物品栏托盘 |
| `--myiui-radius-sm` | `8px` | 单格槽位 |
| blur | `18–24px`（默认 20） | backdrop-filter |
| 字体 | Alibaba PuHuiTi / PingFang SC | 标签；mono 用于数值 |

---

## 3. 布局与锚点

**基准分辨率**：1920×1080

| 组件 | 锚点 | 偏移 |
|------|------|------|
| 状态栏 | 左下（原版 MC 位置） | `left: 20px`, `bottom: 84px`（物品栏上方） |
| 物品栏 | 底部居中 | `bottom: 16px`, `left: 50%` |
| 安全区 | bottom-safe-area | 12–20px（默认 16px ≈ 1.48vmin） |

### GUI Scale 缩放

```css
/* 运行时注入 */
--myiui-hud-scale: 1; /* scale 2→0.85, scale 3→1.0, scale 4→1.15 */
```

所有 px 尺寸在 `hud-spec.json` 中标注等效 vmin，Fabric 渲染器按 `baseSize × scaleFactor × (screenHeight / 1080)` 计算。

---

## 4. 状态栏规格

### 4.1 生命条（Health）

- **结构**：毛玻璃面板内单行轨道
- **填充**：绿色渐变（`--myiui-health-fill`），宽度绑定 `--health-pct`
- **低血**（≤6）：切换 danger 渐变 + `myiui-pulse-danger` 轻微 pulse
- **受伤**：`data-damaged="true"` 触发 0.4s 亮度闪烁
- **伤害吸收**：金色叠加层（`--absorption-pct`），`mix-blend-mode: screen`，z-index 高于生命 fill
- **数字标签**：右上角 mono caption `当前 / 最大`

### 4.2 饥饿条（Hunger）— AppleSkin 三层结构

```
┌─────────────────────────────────┐  ← 毛玻璃面板
│ 底层轨道 (rgba 黑 0.35)          │
│   ├─ 饱和度层 saturationFill     │  z:1  黄金 #E8C547 半透明
│   └─ 饥饿层 hungerFill           │  z:2  暖橙/琥珀渐变
│ 耗尽指示 exhaustion (2px 底线)   │  z:3  仅 appleskin 模式
└─────────────────────────────────┘
```

**CSS 变量绑定**（Fabric 每 tick 或事件更新）：

| 变量 | 范围 | 来源 |
|------|------|------|
| `--hunger-pct` | 0–100% | `foodLevel / 20` |
| `--saturation-pct` | 0–100% | `saturationLevel / 20` |
| `--exhaustion-pct` | 0–100% | `exhaustionLevel / 4` |

**data 属性**（可选，便于调试/测试）：
- `data-hunger`、`data-saturation`、`data-exhaustion`
- `data-appleskin="true|false"` — 模式开关

**关键约束**：饱和度层必须是**独立 DOM 元素 / 独立 CSS 层**，不与 hungerFill 合并，以便 AppleSkin 数据独立驱动动画。

### 4.3 护甲 / oxygen 次要行

- 默认折叠（`max-height: 0`）
- 护甲 > 0 或水下时展开
- 护甲：10 个 mini shield pip，不抢主视觉
- 氧气：4px 高 accent 细条

---

## 5. AppleSkin 兼容策略

### 5.1 配置项

```json
// myiui config / hud-spec.json
"appleskinCompat": {
  "configKey": "myiui.hud.appleskinCompat",
  "default": true
}
```

### 5.2 两种模式

| 模式 | 饱和度层 | 耗尽指示 | Tooltip |
|------|----------|----------|---------|
| **开启** | 显示 | 显示（exhaustion > 0） | hover 食物时 glass 卡片 |
| **关闭** | 隐藏 | 隐藏 | 隐藏 |

### 5.3 Fabric 集成建议

1. **检测 AppleSkin**：`FabricLoader.getInstance().isModLoaded("appleskin")`
2. **数据获取**：
   - 饥饿/饱和/耗尽：`player.getHungerManager()` 标准 API
   - 预计恢复血量：若 AppleSkin 存在，反射或 mixin 调用其 `FoodHelper`；否则隐藏 tooltip 的 healEstimate 行
3. **更新频率**：每 client tick 更新 CSS 变量；饱和度变化可独立 transition（0.4s）
4. **Tooltip 触发**：hover 快捷栏/物品栏中的食物 ItemStack 时，在 HUD  hunger 行上方显示 glass tooltip
5. **不冲突**：MyiUI HUD 完全替换原版 `InGameHud` 渲染，AppleSkin 的 HUD mixin 需 redirect 到 MyiUI 层或禁用其原版 overlay

### 5.4 Tooltip 字段

| 字段 | 格式 | 颜色 |
|------|------|------|
| 饥饿 | `18.0` | 默认 |
| 饱和度 | `12.5` | gold |
| 耗尽 | `1.75` | 默认 |
| 预计恢复 | `+6.0 HP` | success |

---

## 6. 物品栏 Hotbar

- **9 格** + 右侧副手提示区（虚线框「副」）
- 每格：Liquid Glass compact 槽（Flat 降级时为 `.myiui-glass-compact`）
- **选中态**：accent Fresnel 加强 + 外发光 + 底部 pill 下划线
- **子元素**：
  - 物品图标占位
  - 数量角标（>1 时显示，mono tabular）
  - 耐久度 2px 底条（低于 15% 转 danger）
  - 冷却 conic-gradient 遮罩

托盘为横向 glass 底板，比状态栏更紧凑（padding 6×10）。

---

## 7. z-index 层级

| 层 | z-index | 内容 |
|----|---------|------|
| 游戏场景 | 0 | 背景占位 |
| 准星 | 50 | 十字 |
| HUD 状态栏 | 110 | 生命/饥饿 |
| HUD 物品栏 | 120 | Hotbar |
| Tooltip | 200 | AppleSkin 食物数值 |
| 演示控制 | 300 | 预览页专用（游戏中不存在） |

---

## 8. 文件结构

```
myiui-64e1/
├── hud-gameplay.html      # 可交互全屏预览
├── hud-spec.json          # 总规格（token、绑定、缩放）
├── DESIGN.md              # 本文件
├── css/
│   └── hud-system.css     # 可复用样式（与 menu-system.css 并列）
└── screens/
    ├── hud_status.json    # 状态栏布局规格
    └── hud_hotbar.json    # 物品栏布局规格
```

---

## 9. 预览页交互说明

打开 `hud-gameplay.html` 可切换：

- **生命**：满血 / 低血 / 危急 / 受伤闪烁 / 伤害吸收
- **饥饿**：满/半/饥饿 / 高饱和 / 无饱和 + 耗尽滑块
- **AppleSkin**：开/关饱和度层与 tooltip
- **次要行**：护甲 12 点 / 水下氧气
- **物品栏**：1–9 槽选中
- **GUI Scale**：0.85× – 1.15×
- **玻璃模式**：Flat / Liquid Clear / Liquid Tinted + 夜间 darkness

---

## 10. 验收清单

- [x] Glass v2 token 与主菜单一致
- [x] 渐变轨道替代原版图标排列
- [x] 饥饿三层结构 + 独立饱和度层
- [x] `data-saturation` / `--saturation-pct` 等绑定预留
- [x] `appleskin-compat` 模式开关
- [x] Glass tooltip + mono 数字
- [x] Hotbar 9 格 + 选中 accent pill
- [x] 耐久/冷却/数量角标
- [x] 1920×1080 基准 + scale 说明
- [x] 可交互状态变体切换
- [x] JSON 规格文件完整

---

## 11. 后续 Fabric 实现提示

1. 创建 `MyiuiHudRenderer` 继承或替换 `InGameHud` 对应绘制段
2. 从 `hud-spec.json` 加载尺寸/token（或编译为 Java 常量）
3. 每 tick 调用 `updateCssVars(player)` 同步 `--health-pct` 等
4. AppleSkin 存在时注册其 food tooltip 回调
5. GUI Scale 从 `client.options.getGuiScale()` 映射到 `--myiui-hud-scale`

---

## 12. Liquid Glass 集成

> **参考实现**：[Jacquesqwq/LiquidGlassShader](https://github.com/Jacquesqwq/LiquidGlassShader)  
> **版本**：Liquid Glass v2（`liquidGlassV2Clear.frag` / `liquidGlassV2Tinted.frag`）  
> **预览文件**：`hud-gameplay.html` · `css/hud-system.css` · `hud-spec.json → liquidGlass`

### 12.1 设计意图

在 Glass v2 基础上，将 HUD 面板从「普通圆角毛玻璃」升级为 **Liquid Glass** 材质语言，对齐参考项目的六项核心特征：

| 特征 | Shader 实现 | CSS 预览近似 |
|------|-------------|--------------|
| 超椭圆 Superellipse | `sdSuperellipse(p, uPowerFactor, 1.0)` | `border-radius` + `@supports corner-shape: superellipse()` |
| 折射 Refraction | `pow(f(dist), uRefractionPower)` 偏移 UV | `::before` 径向渐变 + `transform` 微偏移 |
| 磨砂 Frost | `uNoise` 颗粒 + 去饱和 | `.myiui-liquid-glass__noise` SVG feTurbulence |
| 色散 Chromatic | `fresnel × uChromaStrength` RGB 分离 | `::after` 红/蓝渐变边 |
| Fresnel 边缘高光 | `pow(1-dist, 3)` × tint/glow | `::before/::after` 边缘 accent 光晕 |
| Tinted 冰蓝 | `uTintColor` + `uTintStrength` + `uDarkness` | `--myiui-lg-tint-*` · 对齐 `#5AC8FA` |

**HUD 默认模式**：`Liquid Tinted`（`tintStrength ≈ 0.12`，`tint RGB ≈ 0.35, 0.78, 1.0`）。  
**选中物品槽**：同 Tinted，但 Fresnel 边缘强度提升（见 `.myiui-hotbar-slot[data-selected="true"]`）。

**AppleSkin 三层饥饿结构不变**——Liquid Glass 仅作用于面板容器，不合并 saturation / hunger / exhaustion 层。

### 12.2 两种玻璃模式 + 降级

| 模式 | CSS 类 | Shader | 用途 |
|------|--------|--------|------|
| **Flat** | `.myiui-glass` | 无 | 无 shader 降级 / 低性能设备 |
| **Liquid Clear** | `.myiui-liquid-glass--clear` | `liquidGlassV2Clear` | 更通透 · 强 directional glow |
| **Liquid Tinted** | `.myiui-liquid-glass--tinted` | `liquidGlassV2Tinted` | HUD 默认 · 冰蓝染色 |

预览页演示面板可切换三种模式；`data-darkness="true"` 模拟夜间 `uDarkness ≈ 0.35`。

### 12.3 CSS 类结构

```
.myiui-liquid-glass              ← 基类（backdrop + 超椭圆 + 伪元素）
  .myiui-liquid-glass--clear     ← Clear 变体
  .myiui-liquid-glass--tinted    ← Tinted 变体（HUD 默认）
  .myiui-liquid-glass-compact    ← 物品栏单格
  .myiui-liquid-glass__noise     ← 磨砂颗粒层（DOM 子元素）
```

应用范围（布局不变，仅换材质）：
- 状态栏面板 `.myiui-status-panel`
- 物品栏托盘 `.myiui-hotbar-tray`
- 9 格槽位 `.myiui-hotbar-slot`
- AppleSkin tooltip（保持独立 glass 卡片）

### 12.4 运行时 Fabric 实现路径

```
游戏帧缓冲
    │
    ▼
FBO 截屏（HUD 后方画面）
    │
    ▼
高斯模糊 pass
  · blurIterations（默认 1）
  · blurRadius（默认 2px）
  · blurDownScale（默认 0.5）
    │
    ▼
Single-pass Liquid Glass shader
  · Clear  → drawClear(...)
  · Tinted → drawTinted(...)  ← HUD 默认
    │
    ▼
ImGui / MyiUI HUD quads（状态栏、hotbar、tooltip 区域）
```

**Shader 参数**完整映射见 `hud-spec.json → liquidGlass.uniformMapping`（含 `uPowerFactor`、`uNoise`、`uRefractionPower`、`uChromaStrength`、`uTintStrength`、`uDarkness` 等）。

**GLSL 适配**：参考仓库为 GLSL 1.20 + `gl_TexCoord`/`varying` 风格；Fabric 1.21.6 阶段需迁移至当前 RenderSystem shader API，保留 single-pass 与 uniform 语义即可。

**降级策略**：
1. 检测 GPU / shader 编译失败 → 回退 `.myiui-glass`（Flat）
2. 模糊 pass 不可用时 → Flat + 静态半透明底
3. 配置项建议：`myiui.hud.liquidGlassMode = flat | clear | tinted`

### 12.5 预览 vs 实机差异

| 效果 | 预览 HTML | 实机 Shader |
|------|-----------|-------------|
| 折射 | CSS 渐变偏移近似 | 真实 blur 纹理 UV 扭曲 |
| 超椭圆 | border-radius / corner-shape | sdSuperellipse discard |
| 色散 | 边缘渐变模拟 | 分通道 texture2D 采样 |
| 磨砂 | SVG noise overlay | uNoise sin/grain |

预览目的：**布局、层级、模式切换、Fresnel 节奏**——非像素级 shader 还原。

### 12.6 验收补充（Liquid Glass）

- [x] `.myiui-liquid-glass` 基类 + Clear / Tinted 变体
- [x] 超椭圆 / Fresnel / noise / chroma 伪元素分层
- [x] 状态栏 + hotbar 托盘 + 槽位应用 Liquid Glass
- [x] 选中槽位加强 Fresnel
- [x] 演示面板三种玻璃模式 + 夜间 darkness
- [x] `hud-spec.json → liquidGlass` shader 参数映射
- [x] AppleSkin 三层饥饿结构未改动
