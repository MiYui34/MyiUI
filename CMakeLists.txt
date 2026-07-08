cmake_minimum_required(VERSION 3.20)
project(MyiUI LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 静态链接 MSVC 运行库，避免目标电脑缺少 VCRedist 导致注入失败
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

add_subdirectory(injector)
add_subdirectory(overlay)
