# IGGPU: A utility library for cross-compiling apps using GLFW+Dawn to native and WASM targets

Version 0.1

## Overview

I've used this library in a lot of my personal projects, and a lot of it is stable enough that
I think it makes sense to isolate into a separate library.

I'll be adding in more general features from side projects over time, for now it's very basic.

## Features

* Configures [Google Dawn](https://dawn.googlesource.com/dawn) for native builds
* Simplifies nasty project boilerplate around setting up a WASM WebGPU app

## Potential issues (and how to fix them):

* Undeclared identifier `IDxcCompiler3` in Dawn D3D12 code
	* (Windows 10) Make sure your Windows SDK version is at least 10.0.20348.0
* wgpu::(everything) is undefined:
	* When working IGGPU code, you'll first have to run a build of the dawn dependencies, since some of the headers are generated code.
	* I've provided a `minimal_example` executable target that will build them as a dependency if you're too lazy to build in CLI like I am.

## Building samples:

Native:
```
mkdir out/debug
cd out/debug
cmake ../..
make iggpu_simple_triangle_sample
./samples/simple_triangle/iggpu_simple_triangle_sample
```

Web (more interesting, eh?)
```
mkdir out/web
cd out/web
emcmake cmake ../..
emmake make iggpu_simpe_triangle_sample
node ../../simple_server.js

# Navigate to http://localhost:8000/samples/simple_triangle/iggpu_simple_triangle_sample.html
```