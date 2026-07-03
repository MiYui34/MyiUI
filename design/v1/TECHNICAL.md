## Technical notes (v1)

> **v2 架构（当前）**：注入链路由 JDK Attach + TCP/SHM 改为 **JVMTI + JNI**。Injector 仅注入 `myiui-overlay.dll`；DLL 在 Render 线程 hook `lwjgl` 后通过内嵌 `Preloader.class` + `agent.jar` 启动 Agent；ImGui 与 Java 之间经 `NativeBridge` / `GameDataBridge` 同进程通信。详见仓库根目录 `README.md`。

### Typography

Main menu uses **Alibaba PuHuiTi** (阿里巴巴普惠体). Place font files under `assets/fonts/`:

- `AlibabaPuHuiTi-3-75-SemiBold.ttf` (brand / profile title; also used for body if Regular is absent)
- `AlibabaPuHuiTi-2-55-Regular.ttf` (optional, lighter body / nav)

Download from [Alibaba Font](https://www.alibabagroup.com/en/ir/alibaba-font). Injector syncs `assets/fonts/` to `%LOCALAPPDATA%\\MyiUI\\runtime\\assets\\fonts\\`.

### Glassmorphism

Open Design specifies backdrop blur + tint. Runtime implementation uses **documented degradation**:

- Semi-transparent tinted panels + border (from `tokens.json`)
- No separable Gaussian blur on live background in v1 (GPU cost / GL state)
- When OD daemon is available, a future v2 may add FBO blur if design requires

### Screen JSON schema (Open Design v1.1)

Screen layouts live in `design/v1/screens/*.json` (synced from OD project **MyiUI**):

- **Hub / list screens**: `title`, `layout`, `contentPanelWidth`, `sections[]` with `nav_list` items (`id`, `label`, `target` → `options-*.html`)
- **Options detail**: `sections[].rows[]` with `id`, `type`, `label`, optional `min`/`max`/`options`
- Row `type`: `toggle` | `slider` | `enum` | `keybind` | `pack_list`

`components.json` (v1.1) uses camelCase keys: `screenShell.contentPanelWidth`, `navList.itemMinHeight`, etc. Overlay `config_loader` accepts both legacy snake_case and OD formats.

Preview HTML: `design/v1/preview/` (`menu-system.css`, `menu-system.js`, per-screen `*.html`).

### Page transition

`motion.json` → `page_transition`:

- `duration_ms` (default 180), `reduce_motion_ms` (80), `slide_px` (0 = crossfade only)
- Overlay `ScreenRouter` applies alpha fade on every Push/Pop; no instant hard cuts

### Open Design

Design source of truth: Open Design project **MyiUI** (`9f115b0a-8545-4e0e-8545-a130d2dbc55e`). Last full menu run: `8526d41d-c2e2-4f19-8f8f-6d5da4a15682` (succeeded). Sync OD output to `design/v1/` via MCP `get_artifact` or copy from `%APPDATA%\\Open Design\\namespaces\\release-stable-win\\data\\projects\\<id>\\`.

OD daemon default: `http://127.0.0.1:54984` (Cursor MCP may need port alignment).

### Runtime sync

Injector copies `design/v1/screens/`, `components.json`, `motion.json`, and menu config to `%LOCALAPPDATA%\\MyiUI\\runtime\\`.
