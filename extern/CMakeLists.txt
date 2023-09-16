#
# GLM
#
add_subdirectory(glm)

#
# GLFW (only on native builds)
#
if (NOT EMSCRIPTEN)
  add_subdirectory(glfw)
endif ()

#
# igasync (WASM-friendly concurrency library)
#
add_subdirectory(igasync)

#
# Dawn (WebGPU impl with Vulkan/DX12/Metal backends) (only on native builds)
#
if (NOT EMSCRIPTEN)
  find_package(Python REQUIRED COMPONENTS Interpreter)

  if (NOT EXISTS "${PROJECT_SOURCE_DIR}/extern/dawn/.gclient")
    message(STATUS "Configuring DAWN .gclient file...")
    execute_process(
      COMMAND ${CMAKE_COMMAND} -E copy
        "${PROJECT_SOURCE_DIR}/extern/dawn/scripts/standalone.gclient"
        "${PROJECT_SOURCE_DIR}/extern/dawn/.gclient"
      RESULT_VARIABLE gclient_copy_rsl
    )
    if (NOT ${gclient_copy_rsl} EQUAL 0)
      message(FATAL "Error copying gclient config for Dawn library")
    endif ()
  endif ()

  # This is a stupid hack - I know offhand that Dawn includes zlib as a
  #  depencency wrangled by gclient, and that zlib is (probably) the last
  #  one to be wrangled should gclient be interrupted, so checking for the
  #  existence of the zlib folder is a decent-ish check for gclient runs.
  # A stamp file might be a better solution, but I'm lazy
  if (NOT EXISTS "${PROJECT_SOURCE_DIR}/dawn/third_party/vulkan-deps/README.md")
    message(STATUS "Downloading DAWN dependencies (this may take a while!)")
    
    execute_process(
        COMMAND ${Python3_EXECUTABLE}
        "${PROJECT_SOURCE_DIR}/dawn/tools/fetch_dawn_dependencies.py"
        RESULT_VARIABLE rsl_dawn_deps_update
    )
    if (NOT ${rsl_dawn_deps_update} EQUAL 0)
      message(FATAL "Failed to wrangle Dawn dependencies")
    endif ()
  endif ()

  add_subdirectory(dawn)
endif ()
