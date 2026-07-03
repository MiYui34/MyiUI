@echo off
setlocal
set ROOT=%~dp0
set MYIUI_ROOT=%ROOT%

echo [1/2] Building agent (Gradle)...
cd /d "%ROOT%agent"
call gradle jar
if errorlevel 1 (
    echo [ERROR] Agent build failed.
    exit /b 1
)

echo [2/2] Building native (CMake)...
cd /d "%ROOT%"
cmake -S . -B build -A x64
cmake --build build --config Release
if errorlevel 1 (
    echo [ERROR] Native build failed.
    exit /b 1
)

echo.
echo [OK] Build complete:
echo   Agent:    %ROOT%agent\build\libs\myiui-agent-1.0.0.jar
echo   Injector: %ROOT%build\injector\Release\myiui-injector.exe
echo   Overlay:  %ROOT%build\overlay\Release\myiui-overlay.dll
