#include <iostream>
#include <cstdio>

#include <lvk/LVK.h>
#include <GLFW/glfw3.h>


int main()
{

    // setting a glfw error callback:
    glfwSetErrorCallback(  // lambda function for the function callback.
        [](int error, const char* description)  
        {
            printf("GLFW error number: %i , description: %s", error, description);
        }
    );

    const char* title = "Vulkan Engine";
    int width = 800;
    int height = 600;
    
    GLFWwindow* vulkan_engine_main_window = lvk::initWindow(title, width, height);
    std::unique_ptr<lvk::IContext> ctx = lvk::createVulkanContextWithSwapchain(vulkan_engine_main_window, width, height, {});
    
    // press escape callback:
    glfwSetKeyCallback(
        vulkan_engine_main_window,
        [](GLFWwindow* vulkan_engine_main_window, int key, int, int action, int)
        {
            if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
            {
                glfwSetWindowShouldClose(vulkan_engine_main_window, GLFW_TRUE);
            }
        }
    );

    while(!glfwWindowShouldClose(vulkan_engine_main_window))
    {
        glfwPollEvents();
        glfwGetFramebufferSize(vulkan_engine_main_window, &width, &height);
        if (!width || !height)
        {
            continue;
        }
        lvk::ICommandBuffer& buf = ctx->acquireCommandBuffer();
        ctx->submit(buf, ctx->getCurrentSwapchainTexture());
    }

    // Cleaning resources
    ctx.reset();
    glfwDestroyWindow(vulkan_engine_main_window);
    glfwTerminate();

    std::cout << "Vulkan Engine Application terminated" << "\n";
    return 0;
}