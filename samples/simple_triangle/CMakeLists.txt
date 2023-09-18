if (EMSCRIPTEN)
  set(platform_entry_src "main_web.cc")
else ()
  set(platform_entry_src "main_native.cc")
endif ()

add_executable(
    iggpu_simple_triangle_sample
    "simple_triangle_app.h" "simple_triangle_app.cc" ${platform_entry_src})
set_property(TARGET iggpu_simple_triangle_sample PROPERTY CXX_STANDARD 20)
target_link_libraries(iggpu_simple_triangle_sample PRIVATE iggpu)

if (EMSCRIPTEN)
  target_link_options(iggpu_simple_triangle_sample PUBLIC "SHELL: --bind -s WASM=1 -s USE_GLFW=3 -s USE_WEBGPU=1")
  set_target_properties(iggpu_simple_triangle_sample PROPERTIES SUFFIX ".html")

  if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Attaching debug symbols to sample app")
    target_compile_options(iggpu_simple_triangle_sample PUBLIC -g -O0)
    target_link_options(iggpu_simple_triangle_sample PUBLIC "SHELL: -g -O0")
  endif ()
endif ()
