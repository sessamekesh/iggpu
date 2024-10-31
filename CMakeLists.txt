cmake_minimum_required(VERSION 3.14)

include(FetchContent)

project(
  "iggpu"
  VERSION 0.1
  DESCRIPTION "Utility library for cross-compiling apps using GLFW+Dawn to native and WASM targets"
  LANGUAGES CXX
)

set(IGGPU_ENABLE_DEFAULT_LOGGING "ON" CACHE BOOL "Enable default logging implementation (to printf)")
set(IGGPU_GRAPHICS_DEBUGGING "ON" CACHE BOOL "Turn on Dawn flags to emit debug symbols from shaders")
set(IGGPU_BUILD_SAMPLES "ON" CACHE BOOL "Include IGGPU samples (no extra dependencies)")

add_subdirectory(extern)

set(iggpu_headers
  "include/iggpu/log.h"
  "platform/include/iggpu/app_base.h")

set(iggpu_sources
  "src/log.cc")

if (EMSCRIPTEN)
  set(iggpu_platform_sources
    "platform/web_src/app_base.cc")
else ()
  set(iggpu_platform_sources
    "platform/native_src/app_base.cc")
endif ()

configure_file("iggpu_config.h.in" "${CMAKE_BINARY_DIR}/generated/include/iggpu/iggpu_config.h")

add_library(iggpu STATIC ${iggpu_headers} ${iggpu_sources} ${iggpu_platform_sources})
target_include_directories(iggpu PUBLIC include platform/include "${CMAKE_BINARY_DIR}/generated/include")
target_link_libraries(iggpu PUBLIC glm igasync)
set_property(TARGET iggpu PROPERTY CXX_STANDARD 20)

if (NOT EMSCRIPTEN)
  target_link_libraries(
    iggpu PUBLIC
      glfw dawncpp)
  target_link_libraries(
    iggpu PRIVATE
      dawn_native dawn_proc dawn_common dawn_glfw)
endif ()

if (EMSCRIPTEN)
  add_executable(minimal_example "minimal_example_web.cc")
  set_property(TARGET minimal_example PROPERTY CXX_STANDARD 20)
  target_link_libraries(minimal_example PRIVATE iggpu)
  target_link_options(minimal_example PUBLIC "SHELL: --bind -s WASM=1 -s USE_GLFW=3 -s USE_WEBGPU=1")
  set_target_properties(minimal_example PROPERTIES SUFFIX ".html")

  if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Attaching debug symbols to sample app")
    target_compile_options(minimal_example PUBLIC -g -O0)
    target_link_options(minimal_example PUBLIC "SHELL: -g -O0")
  endif ()
else ()
  add_executable(minimal_example "minimal_example_native.cc")
  set_property(TARGET minimal_example PROPERTY CXX_STANDARD 20)
  target_link_libraries(minimal_example PRIVATE iggpu)
endif ()

if (IGGPU_BUILD_SAMPLES)
  add_subdirectory(samples)
endif()
