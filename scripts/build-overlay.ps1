# MyiUI overlay build helper (cmake not required in PATH)
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$CMake = "C:\Program Files\CMake\bin\cmake.exe"
if (-not (Test-Path $CMake)) {
  $CMake = "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
}
if (-not (Test-Path $CMake)) {
    throw "未找到 cmake.exe，请安装 CMake 或 Visual Studio Build Tools"
}
$BuildDir = Join-Path $Root "build"
& $CMake -S $Root -B $BuildDir -G "Visual Studio 18 2026" -A x64
& $CMake --build $BuildDir --config Release --target myiui-overlay
Write-Host ""
Write-Host "完成: $BuildDir\overlay\Release\myiui-overlay.dll" -ForegroundColor Green
