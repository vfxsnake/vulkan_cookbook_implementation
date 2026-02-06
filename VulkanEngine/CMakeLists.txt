cmake_minimum_required(VERSION 3.19)

# project name, languages and std version required
project(VulkanEngine CXX C)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_executable(vulkan_engine src/main.cpp)