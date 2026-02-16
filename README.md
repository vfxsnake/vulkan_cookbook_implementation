# vulkan_cookbook_implementation

In this repository I will be implementing the content from the book :
Vulkan 3D Graphics Rendering Cookbook - Second Edition

this is basically my take, as I will be working in windows and ubuntu (wsl) for building the project.
the main editor will be vscode.
CMake will be the too for building the project.


## building commands and configuration:

  cd C:\DEV\vulkan_cookbook_implementation\VulkanEngine                                                                                                                                                                                                                     
  mkdir build                                                                                                                                                                                                                                                               
  cd build                                                                                                                                                                                                                                                                  
  cmake .. -G "Visual Studio 17 2022" -A x64                                                                                                                                                                                                                                
  cmake --build . --config Release 

* note for deploying the dependencies form the book, in windows lightweighvk needs to enable long paths with:
 git config --global core.longpaths true 

"Let's continue the Vulkan learning plan. Check CLAUDE.md for current progress." 


TODO:
  Step 1: Reference files to study

  Here are the key files in the book repo to read. I'd suggest going in this order:

  1. Vulkan Context (instance + device + swapchain)
  - 3D-Graphics-Rendering-Cookbook-Second-Edition/deps/src/lightweightvk/lvk/vulkan/VulkanClasses.h — look for class VulkanContext
  (the main wrapper)
  - 3D-Graphics-Rendering-Cookbook-Second-Edition/deps/src/lightweightvk/lvk/vulkan/VulkanClasses.cpp — find createInstance(),
  initContext(), initSwapchain()

  2. Command Buffers + Synchronization
  - Same files — look for class VulkanImmediateCommands, acquire(), submit()

  3. The simple swapchain demo
  - 3D-Graphics-Rendering-Cookbook-Second-Edition/Chapter02/01_Swapchain/src/main.cpp

  4. The hello triangle demo
  - 3D-Graphics-Rendering-Cookbook-Second-Edition/Chapter02/02_HelloTriangle/src/main.cpp

  Start by reading through the Chapter 2 book reference I just showed you (the Chapter02_GettingStarted.md doc you had open). Then
  look at the actual source files above. Focus on understanding the flow, not memorizing every line.

  When you're ready, I'll ask you some comprehension questions to check understanding before we move on. Take your time with the
  reading!