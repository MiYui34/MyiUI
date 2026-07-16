# MyiUI Fabric Mod

Stonecutter multi-version Fabric client mod (data source + vanilla UI suppressor).

## Supported Minecraft versions

| Node | Minecraft | Fabric API |
|------|-----------|------------|
| `1.21` | 1.21 | 0.102.0+1.21 |
| `1.21.1` | 1.21.1 | 0.116.10+1.21.1 |
| `1.21.2` | 1.21.2 | 0.106.0+1.21.2 |
| `1.21.3` | 1.21.3 | 0.114.0+1.21.3 |
| `1.21.4` | 1.21.4 | 0.119.2+1.21.4 |
| `1.21.5` | 1.21.5 | 0.119.4+1.21.5 |
| `1.21.6` | 1.21.6 | 0.128.2+1.21.6 |
| `1.21.7` | 1.21.7 | 0.129.0+1.21.7 |
| `1.21.8` | 1.21.8 | 0.133.0+1.21.8 |
| `1.21.9` | 1.21.9 | 0.134.0+1.21.9 |
| `1.21.10` | 1.21.10 | 0.136.0+1.21.10 |
| `1.21.11` | 1.21.11 | 0.141.3+1.21.11 |
| `26.1` | 26.1 | 0.145.0+26.1 |
| `26.1.1` | 26.1.1 | 0.145.4+26.1.1 |
| `26.1.2` | 26.1.2 | 0.154.2+26.1.2 |
| `26.2` | 26.2 | 0.154.2+26.2 |

## Build

```powershell
# Single version
.\gradlew.bat :1.21.6:buildAndCollect
.\gradlew.bat :26.1.2:buildAndCollect

# All supported versions
.\gradlew.bat buildAndCollect
```

Switch active IDE sources:

```powershell
.\gradlew.bat "Set active project to 1.21.6"
```

Jars land in `build/libs/<mod.version>/` as `myiui-2.0.0+<mc>.jar`.

See root [README.md](../README.md) for Electron overlay usage.
