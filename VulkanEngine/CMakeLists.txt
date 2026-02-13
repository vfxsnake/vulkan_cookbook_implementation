cmake_minimum_required(VERSION 3.19)

# project name, languages and std version required
project(VulkanEngine CXX C)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(LVK_WITH_SAMPLES   OFF CACHE BOOL "")
set(LVK_WITH_TRACY     OFF  CACHE BOOL "")
set(LVK_WITH_TRACY_GPU OFF  CACHE BOOL "")

# adding definitions
add_definitions(-DGLM_ENABLE_EXPERIMENTAL)

# adding include directories
include_directories(../3D-Graphics-Rendering-Cookbook-Second-Edition/deps/src/lightweightvk)
include_directories(../3D-Graphics-Rendering-Cookbook-Second-Edition/deps/src/lightweightvk/third-party/deps/src/glm)

# adding subdirectories before executable
add_subdirectory(../3D-Graphics-Rendering-Cookbook-Second-Edition/deps/src/lightweightvk lightweightvk)
add_executable(vulkan_engine src/main.cpp)

# linking libraries sould be after the executable
target_link_libraries(vulkan_engine LVKLibrary)