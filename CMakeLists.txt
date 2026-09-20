cmake_minimum_required(VERSION 3.16)

project(glfw2sdl
        VERSION 1.0.0
        DESCRIPTION "GLFW 3.5 compatibility layer implemented on top of SDL3"
        LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(GLFW_SDL_BUILD_STATIC "Also build a static glfw library" OFF)
option(GLFW_SDL_SMOKE_TEST "Build the SDL-backed GLFW smoke test" ON)

find_package(SDL3 CONFIG QUIET)

if(TARGET SDL3::SDL3)
    set(SDL3_FOUND TRUE)
else()
    find_package(PkgConfig QUIET)
    pkg_check_modules(SDL3 IMPORTED_TARGET sdl3)
endif()

if(NOT SDL3_FOUND)
    message(FATAL_ERROR
        "SDL3 was not found. Install it (e.g. libsdl3-dev) or set "
        "SDL3_DIR to its CMake config directory.")
endif()

set(GLFW_SDL_SOURCES
    src/context.c
    src/error.c
    src/events.c
    src/init.c
    src/input.c
    src/keys.c
    src/log.c
    src/monitor.c
    src/native.c
    src/vulkan.c
    src/window.c)

set(GLFW_SDL_PUBLIC_HEADERS include/GLFW/glfw3.h include/GLFW/glfw3native.h)

add_library(glfw SHARED ${GLFW_SDL_SOURCES})
add_library(glfw::glfw ALIAS glfw)

# Make it compatible with projects expecting real GLFW
set_target_properties(glfw PROPERTIES
    OUTPUT_NAME glfw
    VERSION     3.5.0
    SOVERSION   3.5
    C_STANDARD  11)

target_include_directories(glfw PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

if(TARGET SDL3::SDL3)
    target_link_libraries(glfw PRIVATE SDL3::SDL3)
else()
    target_link_libraries(glfw PRIVATE PkgConfig::SDL3)
endif()

# monitor.c uses pow().
if(NOT WIN32)
    target_link_libraries(glfw PRIVATE m)
endif()

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(glfw PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

if(WIN32)
    # DLL export of the GLFWAPI functions (the headers do not use dllexport).
    target_compile_definitions(glfw PRIVATE GLFW_DLL)
    set_target_properties(glfw PROPERTIES PREFIX "")
endif()

if(GLFW_SDL_BUILD_STATIC)
    add_library(glfw_static STATIC ${GLFW_SDL_SOURCES})
    set_target_properties(glfw_static PROPERTIES OUTPUT_NAME glfw)
    target_include_directories(glfw_static PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)
    if(TARGET SDL3::SDL3)
        target_link_libraries(glfw_static PRIVATE SDL3::SDL3)
    else()
        target_link_libraries(glfw_static PRIVATE PkgConfig::SDL3)
    endif()
    if(NOT WIN32)
        target_link_libraries(glfw_static PRIVATE m)
    endif()
endif()

if(GLFW_SDL_SMOKE_TEST)
    add_executable(glfw_smoke_test tests/smoke_test.c)
    target_link_libraries(glfw_smoke_test PRIVATE glfw)
    target_include_directories(glfw_smoke_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
endif()

include(GNUInstallDirs)

install(TARGETS glfw
        EXPORT glfw2sdl-targets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(GLFW_SDL_BUILD_STATIC)
    install(TARGETS glfw_static
            EXPORT glfw2sdl-targets
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()

install(FILES ${GLFW_SDL_PUBLIC_HEADERS}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/GLFW)

install(EXPORT glfw2sdl-targets
        NAMESPACE glfw2sdl::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/glfw2sdl)
