cmake_minimum_required(VERSION 3.21)   # $<TARGET_RUNTIME_DLLS> below
project(pde_visualizer VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# Use system SFML if installed; otherwise fetch and build it from source.
find_package(SFML 2.5 COMPONENTS graphics window system QUIET)
if(NOT SFML_FOUND)
    set(SFML_BUILD_AUDIO    OFF CACHE BOOL "" FORCE)
    set(SFML_BUILD_NETWORK  OFF CACHE BOOL "" FORCE)
    set(SFML_BUILD_DOC      OFF CACHE BOOL "" FORCE)
    set(SFML_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(SFML
        GIT_REPOSITORY https://github.com/SFML/SFML.git
        GIT_TAG        2.6.2
        GIT_SHALLOW    ON)
    FetchContent_MakeAvailable(SFML)
endif()

# ImGui 1.89.9 + imgui-sfml 2.6 — newer pairs break the SFML 2.5 bridge
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.89.9)
FetchContent_MakeAvailable(imgui)

FetchContent_Declare(imgui-sfml
    GIT_REPOSITORY https://github.com/SFML/imgui-sfml.git
    GIT_TAG        v2.6)
set(IMGUI_DIR ${imgui_SOURCE_DIR})
set(IMGUI_SFML_FIND_SFML OFF)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
FetchContent_MakeAvailable(imgui-sfml)

file(GLOB_RECURSE SOURCES src/*.cpp)

add_executable(pde_visualizer ${SOURCES})

target_include_directories(pde_visualizer PRIVATE src/ ${imgui_SOURCE_DIR})
target_link_libraries(pde_visualizer PRIVATE
    sfml-graphics sfml-window sfml-system
    ImGui-SFML::ImGui-SFML)

# /utf-8 so the Cyrillic u8 literals in the UI survive MSVC's default code page
if(MSVC)
    target_compile_options(pde_visualizer PRIVATE /utf-8)
endif()

if(WIN32)
    add_custom_command(TARGET pde_visualizer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:pde_visualizer>
            $<TARGET_FILE_DIR:pde_visualizer>
        COMMAND_EXPAND_LISTS)
endif()