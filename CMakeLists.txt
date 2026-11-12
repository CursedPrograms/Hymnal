cmake_minimum_required(VERSION 3.20)
project(Hymnal VERSION 1.0.0 LANGUAGES C CXX)
# Hymnal: whole songs, with vocals, from a style and your lyrics; covers, remixes, stems, key and tempo. Part of Atelier.
# The engine is acestep.cpp's ace-server (ACE-Step 1.5 in C++ on ggml, MIT), run by Hymnal; build it with build.bat.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

include(FetchContent)
if(EXISTS ${CMAKE_SOURCE_DIR}/external/imgui-1.90.4/imgui.h)
    set(FETCHCONTENT_SOURCE_DIR_IMGUI ${CMAKE_SOURCE_DIR}/external/imgui-1.90.4)
endif()
FetchContent_Declare(imgui URL https://github.com/ocornut/imgui/archive/refs/tags/v1.90.4.tar.gz)
FetchContent_MakeAvailable(imgui)
if(WIN32 AND NOT SDL2_DIR AND EXISTS ${CMAKE_SOURCE_DIR}/external/SDL2-2.30.9/cmake)
    set(SDL2_DIR ${CMAKE_SOURCE_DIR}/external/SDL2-2.30.9/cmake)
endif()
find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)
add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp ${imgui_SOURCE_DIR}/imgui_draw.cpp ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_link_libraries(imgui PUBLIC SDL2::SDL2 OpenGL::GL)

set(SOURCES src/gui.cpp src/splash.cpp src/audio.cpp src/http.cpp src/engine.cpp)
if(WIN32)
    list(APPEND SOURCES src/app.rc)
endif()
add_executable(Hymnal WIN32 ${SOURCES})
target_include_directories(Hymnal PRIVATE src)
target_link_libraries(Hymnal PRIVATE imgui)
target_compile_definitions(Hymnal PRIVATE HYMNAL_VERSION="${PROJECT_VERSION}")
if(TARGET SDL2::SDL2main)
    target_link_libraries(Hymnal PRIVATE SDL2::SDL2main)
endif()
if(MSVC)
    target_compile_options(Hymnal PRIVATE /W3 /utf-8 /O2 /D_CRT_SECURE_NO_WARNINGS)
endif()
add_custom_command(TARGET Hymnal POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:Hymnal>/assets)
if(WIN32)
    add_custom_command(TARGET Hymnal POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:SDL2::SDL2> $<TARGET_FILE_DIR:Hymnal>)
endif()
