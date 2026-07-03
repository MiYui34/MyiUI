@echo off
setlocal
set ROOT=%~dp0
set CMAKE="C:\Program Files\CMake\bin\cmake.exe"
set MYIUI_ROOT=%ROOT%

echo [MyiUI] Building agent...
cd /d "%ROOT%agent"
call gradle jar
if errorlevel 1 exit /b 1

echo [MyiUI] Building native...
cd /d "%ROOT%"
if exist build rd /s /q build
%CMAKE% -S . -B build -A x64
%CMAKE% --build build --config Release
if errorlevel 1 exit /b 1

echo [MyiUI] Build complete.
echo Agent: %ROOT%agent\build\libs\myiui-agent-1.0.0.jar
echo Injector: %ROOT%build\injector\Release\myiui-injector.exe
echo Overlay: %ROOT%build\overlay\Release\myiui-overlay.dll
