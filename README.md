# MyiUI

Minecraft Fabric **1.21 – 1.21.11** 进程外注入方案（单包自适应）：**Injector + core.dll (overlay) + agent.jar**，v2 使用 **JVMTI + JNI** 直连，保留 ImGui UI。

## 结构

- `injector/` — GUI 注入器（进程扫描 + `LoadLibraryW` 注入 DLL）；CLI：`--cli [PID]`
- `overlay/` — core 运行时：`lwjgl` JVMTI 入口、Preloader、JNI Bridge、ImGui 主菜单/HUD/Island/Chat
- `agent/` — ASM Transformer、JavaCV 背景、`NativeBridge` 状态推送、`GameDataBridge` 查询/动作
- `design/v1/` — UI 设计定稿（tokens/layout/motion/preview）
- `config/menu/` — 运行时配置
- `shared/` — 仅 logo WIC 工具（状态结构已迁至 `overlay/src/bridge/ui_state_types.h`）

## 架构 (v2)

```
myiui-injector.exe  →  LoadLibraryW(myiui-overlay.dll)
  → hook lwjgl JNI (Render thread) → defineClass(Preloader) → URLClassLoader(agent.jar) → Main.main
  → hook wglSwapBuffers/glfwSwapBuffers → ImGui 渲染
  → JNI: Java push 状态 / C++ query&action 调 GameDataBridge
```

**不再需要**：本机 JDK、`jdk.attach`、TCP 47891、SHM mmap。

## 构建

一键（增量构建，不删 build 目录）：

```powershell
.\build.bat
```

或分步：

```powershell
cd agent
gradle jar          # 同时生成 overlay/src/preload/preloader_class.h（纯 Java，无 PS1 依赖）

cd ..
cmake -S . -B build -A x64
cmake --build build --config Release
```

**环境要求**：JDK 21+、Gradle 8+、CMake 3.20+、MSVC x64（VS 2022 或 Build Tools）

产物：
- `agent/build/libs/myiui-agent-1.0.0.jar`
- `build/injector/Release/myiui-injector.exe`
- `build/overlay/Release/myiui-overlay.dll`

## 使用

1. 构建 agent + native（见上）
2. 启动 Minecraft Fabric **1.21 – 1.21.11** 任一版本
3. 运行 GUI 注入器（会自动设置 `MYIUI_ROOT` 并写入 `project_root.txt`）：

```powershell
.\build\injector\Release\myiui-injector.exe
```

或 CLI：`.\build\injector\Release\myiui-injector.exe --cli [PID]`

4. 注入后主菜单显示 MyiUI UI；顶栏 **Manager** 可选择 MP4/图片背景
5. 调试日志：`%LOCALAPPDATA%\MyiUI\agent.log`（Agent）、`spike.log`（注入/Overlay）

设计预览：浏览器打开 `design/injector/v1/preview/index.html`

## 设计

所有 UI 视觉以 `design/v1/` 为准。技术说明见 `design/v1/TECHNICAL.md`。

## 网易云音乐播放器

游戏内网易云播放器依赖 [api-enhanced](https://github.com/neteasecloudmusicapienhanced/api-enhanced) 自建 API 服务。

### 部署 API 服务

**方式一：独立 exe（最简单，推荐）**

1. 从 [GitHub Releases](https://github.com/NeteaseCloudMusicApiEnhanced/api-enhanced/releases/download/v4.36.1/ncm-api-win-x64.exe) 下载 `ncm-api-win-x64.exe`（约 59MB）
2. 放到以下任一位置：
   - 项目根目录（`d:\download\MyiUI\`）
   - `%LOCALAPPDATA%\MyiUI\`
   - `%LOCALAPPDATA%\MyiUI\api-enhanced\`
3. 完成。Agent 注入时会自动检测并启动该 exe，无需 Node.js、无需克隆仓库

如果 exe 不存在，Agent 还会自动从 GitHub 下载（首次约 59MB）。

**方式二：Docker 一键部署**

```powershell
docker pull moefurina/ncm-api:latest
docker run -d -p 3000:3000 --name ncm-api moefurina/ncm-api:latest
```

**方式三：Node.js 运行（需克隆仓库）**

```powershell
git clone https://github.com/neteasecloudmusicapienhanced/api-enhanced.git
cd api-enhanced
pnpm i
```

Agent 会自动在后台 `node app.js` 启动服务。

**方式四：公共在线实例**

修改 `config/menu/netease.json` 的 `base_url` 指向公共实例（有安全风险，建议用小号）：

```json
{ "base_url": "https://ncme.zhenxin.me" }
```

### 配置

编辑 `config/menu/netease.json`：

```json
{
  "base_url": "http://localhost:3000",
  "quality": "exhigh",
  "enable_flac": false,
  "volume": 80
}
```

- `quality`：`standard` / `exhigh` / `lossless`（lossless 需 api-enhanced 启用 `ENABLE_FLAC`）
- 登录态 cookie 明文保存在 `%LOCALAPPDATA%\MyiUI\netease\cookie.txt`，请勿在公共设备使用

## License

MyiUI 采用 [GNU Affero General Public License v3.0](LICENSE)（AGPL-3.0）授权。

仓库内第三方组件保留各自原有许可证，例如 `overlay/third_party/imgui`（MIT）、`overlay/third_party/minhook`（BSD）。

## Credits

- [SoarClient-fork](https://github.com/Eatgrapes/SoarClient-fork) (MIT) — ModernHotBar 居中布局、信息 HUD 与音乐波形交互思路参考
