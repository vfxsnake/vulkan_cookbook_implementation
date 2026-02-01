###          Section 1-  Project setup
cmake_minimum_required(VERSION 3.19)
project(VulkanEngine)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

###          Section 2-  Find Vulkan
find_package(Vulkan REQUIRED)

###          Section 3- Setup FetchContent
include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

