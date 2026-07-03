# MyiUI Injector UI — Technical v1

## 定稿说明

Open Design daemon 不可用时，本目录为本地定稿；daemon 可用后可通过 MCP 迭代并同步。

## 窗口规格

- 尺寸：520×640（最小 440×520）
- 风格：与 `design/v1/` 主菜单一致的暗色玻璃 + `#78a8ff` 强调色
- 字体：Segoe UI（标题/正文）、Consolas（状态日志）

## 功能映射（参考 InfiniteGUI 注入器）

| UI 控件 | 行为 |
|---------|------|
| 进程列表 | 枚举 `java.exe` / `javaw.exe`，显示 PID、内存、JDK 版本、命令行摘要 |
| JDK 匹配 | 自动识别目标 JVM（21/26），扫描 `JAVA_HOME` / 常见安装目录并 attach |
| 推荐标记 | 命令行含 `minecraft`/`fabric`/`net.minecraft` 或内存最大者优先 |
| 刷新 | 手动重新扫描 |
| 自动刷新 | 2s 定时器，可开关 |
| 注入 MyiUI | attach Agent JAR → 等待 2s → LoadLibrary overlay.dll |
| 状态区 | 追加 `[MyiUI]` 日志行，成功/失败分色 |

## Native 实现

Win32 GUI（`injector/src/gui_app.cpp`），颜色与间距取自 `tokens.json` / `layout.json`。

CLI 保留：`myiui-injector.exe --cli [PID]` 或 `myiui-injector.exe <PID>`。
