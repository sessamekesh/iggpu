cmake_minimum_required(VERSION 3.14)

include(FetchContent)

project(
  "iggpu"
  VERSION 0.1
  DESCRIPTION "Utility library for cross-compiling apps using GLFW+Dawn to native and WASM targets"
  LANGUAGES CXX
)

add_subdirectory(extern)

add_executable(foobar_driver "main.cc")
set_property(TARGET foobar_driver PROPERTY CXX_STANDARD 20)
