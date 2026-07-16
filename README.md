# MyiUI — Dual-Star Architecture

Modern Web UI for Minecraft via two fully decoupled modules:

1. **Fabric Client Mod** (`mod/`) — data source + vanilla HUD/title suppressor, broadcasts over WebSocket  
2. **Electron Overlay** (`electron/`) — transparent always-on-top click-through window that renders all UI/animations

Supported game versions (Stonecutter multi-compile, one jar per version):

- **1.21** … **1.21.11**（全部正式补丁）
- **26.1**、**26.1.1**、**26.1.2**、**26.2**

> 说明：官方没有 `26.1.11`；若你指的是 26.1 系列，已按 `26.1` / `26.1.1` / `26.1.2` 配置。

## Layout

```
mod/          Fabric + Stonecutter (Mojang mappings)
electron/     Electron overlay (transparent / alwaysOnTop / mouse passthrough)
protocol/     WebSocket JSON schema + shared types
design/v1/    Visual source of truth (tokens, HUD, menu, island, intro, chat)
```

## Requirements

- JDK **21+** (26.x nodes need **Java 25** toolchain — Foojay resolver downloads it)
- Node.js 18+
- Fabric Loader ≥ 0.17

## Build

```powershell
.\build.bat
```

Or separately:

```powershell
cd mod
.\gradlew.bat buildAndCollect

cd ..\electron
npm install
```

Switch active Minecraft version (Stonecutter):

```powershell
cd mod
.\gradlew.bat "Set active project to 1.21.6"
.\gradlew.bat "Set active project to 26.1.x"
```

Build a specific node:

```powershell
.\gradlew.bat :1.21.6:build
.\gradlew.bat :26.1.x:build
```

Docs: [Stonecutter beginner's guide](https://stonecutter.kikugie.dev/wiki/start/) · [Porting to 26.1](https://docs.fabricmc.net/develop/porting/)

## Run

1. Install the built jar from `mod/build/libs/` into your Fabric client's `mods/` folder (with Fabric API).
2. Start Minecraft (prefer **borderless windowed** — exclusive fullscreen can fight always-on-top overlays).
3. Start the overlay:

```powershell
cd electron
npm start
```

The mod listens on `ws://127.0.0.1:25566`. When the overlay connects and sends `OVERLAY_READY`, vanilla title/HUD/tab are suppressed and Electron draws the UI.

## WebSocket protocol

See [`protocol/schema.json`](protocol/schema.json) and [`protocol/types.js`](protocol/types.js).

- **Push** (mod → overlay): `hello`, `screen`, `window`, `hud`, `island`, `tablist`, `music`, `chat`, …
- **Request/response** (overlay → mod): `OVERLAY_READY`, `QUIT`, `OPEN_*`, `GET_*`, `SET_*`, `NE_*`, …

High-frequency HUD uses dirty-checking; the Minecraft thread only enqueues messages (no blocking network IO).

## NetEase music

Optional: run [api-enhanced](https://github.com/neteasecloudmusicapienhanced/api-enhanced) on `http://127.0.0.1:3000`, then use ClickGui (Right Shift) → 网易云.

## License

AGPL-3.0
