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
```
npm install
mkdir build
cd build
cmake ..
```
