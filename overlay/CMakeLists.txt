cmake_minimum_required(VERSION 3.20)
project(myiui-overlay LANGUAGES CXX C)

set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui-1.91.6)
set(MINHOOK_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/minhook-1.3.3)

set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

set(MINHOOK_SOURCES
    ${MINHOOK_DIR}/src/buffer.c
    ${MINHOOK_DIR}/src/hook.c
    ${MINHOOK_DIR}/src/trampoline.c
    ${MINHOOK_DIR}/src/HDE/hde64.c
)

add_library(myiui-overlay SHARED
    src/dllmain.cpp
    src/hooks.cpp
    src/overlay_runtime.cpp
    src/jvm/jvm_log.cpp
    src/jvm/jvm_context.cpp
    src/jvm/jni_func.cpp
    src/jvm/jvm_spike.cpp
    src/inject/game_hook.cpp
    src/preload/preloader.cpp
    src/preload/preloader_class.h
    src/bridge/native_state.cpp
    src/bridge/native_bridge.cpp
    src/bridge/native_query.cpp
    src/bridge/overlay_command_queue.cpp
    src/config/config_loader.cpp
    src/config/user_settings.cpp
    src/ipc/pipe_client.cpp
    src/ipc/shm_reader.cpp
    src/ui/menu_app.cpp
    src/ui/menu_background.cpp
    src/ui/profile_avatar.cpp
    src/ui/profile_card.cpp
    src/ui/menu_data.cpp
    src/ui/ui_manager.cpp
    src/ui/menu_icons.cpp
    src/ui/media_library.cpp
    src/ui/glass_panel.cpp
    src/ui/fonts.cpp
    src/ui/screen_transition.cpp
    src/ui/widgets/glass_button.cpp
    src/ui/widgets/parallax_card.cpp
    src/ui/widgets/screen_shell.cpp
    src/ui/widgets/nav_list.cpp
    src/ui/widgets/toast.cpp
    src/ui/screens/home_screen.cpp
    src/ui/screens/singleplayer_screen.cpp
    src/ui/screens/create_world_screen.cpp
    src/ui/screens/multiplayer_screen.cpp
    src/ui/screens/add_server_screen.cpp
    src/ui/async_pipe.cpp
    src/ui/screens/options_hub_screen.cpp
    src/ui/screens/options_detail_screen.cpp
    src/ui/intro/intro_particles.cpp
    src/ui/intro/intro_screen.cpp
    src/ui/logo_assets.cpp
    src/ui/island/spring_animator.cpp
    src/ui/clickgui/clickgui.cpp
    src/ui/hud/hud_glass.cpp
    src/ui/hud/hud_renderer.cpp
    src/ui/hud/immersive_lyrics.cpp
    src/ui/hud/layout_editor.cpp
    src/ui/hud/now_playing.cpp
    src/ui/hud/now_playing_layout.cpp
    src/ui/island/island_renderer.cpp
    src/ui/music/cover_loader.cpp
    src/ui/music/music_panel.cpp
    src/ui/theme/material_you.cpp
    src/ui/theme/theme_runtime.cpp
    src/render/frame_runtime.cpp
    src/render/gl_state_guard.cpp
    src/render/gl_loader.cpp
    ../shared/logo_wic.cpp
    ${IMGUI_SOURCES}
    ${MINHOOK_SOURCES}
)

target_include_directories(myiui-overlay PRIVATE
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${MINHOOK_DIR}/include
    ${MINHOOK_DIR}/src
    ${MINHOOK_DIR}/src/HDE
    ${CMAKE_SOURCE_DIR}/shared
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/jni/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/jni/include/win32
    src
)

target_compile_definitions(myiui-overlay PRIVATE
    UNICODE _UNICODE WIN32_LEAN_AND_MEAN
)

if(MSVC)
    target_compile_options(myiui-overlay PRIVATE /utf-8)
endif()

target_link_libraries(myiui-overlay PRIVATE opengl32 gdi32 user32 ws2_32 shell32 comdlg32 ole32 shlwapi windowscodecs winhttp)

set_target_properties(myiui-overlay PROPERTIES OUTPUT_NAME "myiui-overlay")

add_custom_command(TARGET myiui-overlay POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:myiui-overlay>/assets/logos/png"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets/logos/png"
        "$<TARGET_FILE_DIR:myiui-overlay>/assets/logos/png"
    COMMENT "Copying logo assets next to overlay DLL")
