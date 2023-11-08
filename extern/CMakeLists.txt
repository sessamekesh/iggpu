include(FetchContent)

#
# GLM
#
FetchContent_Declare(
    glfw
    GIT_REPOSITORY "https://github.com/g-truc/glm"
    GIT_TAG "47585fde0c49fa77a2bf2fb1d2ead06999fd4b6e"
)
FetchContent_MakeAvailable(glfw)

#
# GLFW (only on native builds)
#
if (NOT EMSCRIPTEN)
  FetchContent_Declare(
    glfw
    GIT_REPOSITORY "https://github.com/glfw/glfw"
    GIT_TAG "3eaf1255b29fdf5c2895856c7be7d7185ef2b241"
  )
  FetchContent_MakeAvailable(glfw)
endif ()

#
# igasync (only on web builds)
#
if (NOT TARGET igasync)
  FetchContent_Declare(
    igasync
    GIT_REPOSITORY "https://github.com/sessamekesh/igasync"
    GIT_TAG "0c1e06bcade82f2b6e294b0e2e89e6d06a1a00d8"
  )
  FetchContent_MakeAvailable(igasync)
endif ()

#
# Dawn (WebGPU impl with Vulkan/DX12/Metal backends) (only on native builds)
#
if (NOT EMSCRIPTEN)
  FetchContent_Declare(
    dawn
    GIT_REPOSITORY "https://dawn.googlesource.com/dawn/"
    GIT_SUBMODULES ""
    GIT_TAG "4b86619504466704374f98795213587874f4b258"
  )
  FetchContent_GetProperties(dawn)
  if (NOT dawn_POPULATED)
    # Fetch the content...
    FetchContent_Populate(dawn)

    # ... Custom scripts...
    if (NOT EXISTS "${dawn_SOURCE_DIR}/.gclient")
      message(STATUS "Configuring DAWN .gclient file...")
      execute_process(
        COMMAND ${CMAKE_COMMAND} -E copy
        "${dawn_SOURCE_DIR}/scripts/standalone.gclient"
        "${dawn_SOURCE_DIR}/.gclient"
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
    if (NOT EXISTS "${dawn_SOURCE_DIR}/third_party/vulkan-deps/README.md")
      message(STATUS "Downloading DAWN dependencies (this may take a while!)")

      execute_process(
        COMMAND "python" "${dawn_SOURCE_DIR}/tools/fetch_dawn_dependencies.py"
        WORKING_DIRECTORY "${dawn_SOURCE_DIR}"
        RESULT_VARIABLE rsl_dawn_deps_update
      )
      if (NOT ${rsl_dawn_deps_update} EQUAL 0)
        message(FATAL "Failed to wrangle Dawn dependencies")
      endif ()
    endif ()

    add_subdirectory(${dawn_SOURCE_DIR} ${dawn_BINARY_DIR})
  endif ()
endif ()
