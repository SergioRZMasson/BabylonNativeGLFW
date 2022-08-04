# Babylon Native GLFW

Example on how to create a cross platform application using GLFW and Babylon Native. It creates a simple GLFW window and performs the required setup for Babylon Native to render into that window. The rendering logic can be controlled by the game.js script located under Scripts/game.js.

## Getting started

This repo uses npm to download the babylon.js dependecies, and CMake to create the platform specific project. It is required to run npm install before CMake since it will try to copy the javascript files from the babylon.js packages into the generated solution. 

### Mac
```
npm install
mkdir build
cd build
cmake .. -G "Xcode"
```

### Windows
```
npm install
mkdir build
cd build
cmake .. -G "Visual Studio 16 2019"
```

### Linux

#### Install CMake:

Babylon Native requires a cmake version higher than the one installed using default ```apt-get install cmake```. It is required that a higher cmake version is installed manually. For instructions on how do install cmake manually please follow the official documentation:

[Install CMake](https://cmake.org/install/)

#### Install dependecies:

```
sudo apt-get install libxi-dev libxcursor-dev libxinerama-dev libglfw3-dev libgl1-mesa-dev libcurl4-openssl-dev clang-9 libc++-9-dev libc++abi-9-dev lld-9 ninja-build
```

#### Install V8:

```
sudo apt-get install libv8-dev
```

#### Build:
```
npm install
mkdir build
cd build
cmake -G Ninja -D NAPI_JAVASCRIPT_ENGINE=V8
ninja
```

#### Run:
```
./BabylonNativeExample
```
