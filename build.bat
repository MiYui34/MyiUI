@echo off
setlocal
echo === MyiUI Dual-Star Build ===

echo.
echo [1/2] Building Fabric mod (active Stonecutter node)...
cd /d "%~dp0mod"
call gradlew.bat buildAndCollect --no-daemon
if errorlevel 1 (
  echo Mod build failed.
  exit /b 1
)

echo.
echo [2/2] Installing Electron overlay deps...
cd /d "%~dp0electron"
call npm install
if errorlevel 1 (
  echo Electron npm install failed.
  exit /b 1
)

echo.
echo Done.
echo   Mod jars:   mod\build\libs\
echo   Overlay:    cd electron ^&^& npm start
echo.
echo Tip: use "gradlew :1.21.6:build" / "gradlew :26.1.2:build" or Stonecutter "Set active project" for other MC versions.
echo Supported: 1.21-1.21.11, 26.1, 26.1.1, 26.1.2, 26.2
endlocal
