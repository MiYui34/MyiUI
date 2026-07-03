cmake_minimum_required(VERSION 3.20)
project(myiui-injector LANGUAGES CXX)

set(IMGUI_DIR "${CMAKE_SOURCE_DIR}/overlay/third_party/imgui-1.91.6")
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
    ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp
)

add_executable(myiui-injector WIN32
    src/main.cpp
    src/gui_app.cpp
    src/gui_injector.cpp
    src/inject_core.cpp
    src/logo_assets.cpp
    src/process_scanner.cpp
    ../shared/logo_wic.cpp
    ${IMGUI_SOURCES}
)

target_include_directories(myiui-injector PRIVATE
    src
    ${CMAKE_SOURCE_DIR}/shared
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

target_compile_definitions(myiui-injector PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(myiui-injector PRIVATE psapi dwmapi d3d11 dxgi windowscodecs ole32)

if(MSVC)
    target_compile_options(myiui-injector PRIVATE /utf-8)
endif()

add_custom_command(TARGET myiui-injector POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:myiui-injector>/assets/logos/png"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets/logos/png"
        "$<TARGET_FILE_DIR:myiui-injector>/assets/logos/png"
    COMMENT "Copying logo assets next to injector EXE")
