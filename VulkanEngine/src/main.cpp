#include <iostream>
#include <cstdio>

#define GLFW_INCLUDE_NONE  // prevents glfw from pulling openGL headers.
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


    if (!glfwInit())
    {
        std::cout << "unable to initialize GLFW";
        return -1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    const char* title = "Vulkan Engine";
    GLFWwindow* vulkan_engine_main_window = glfwCreateWindow(800, 600, title, nullptr, nullptr);
    if (!vulkan_engine_main_window)
    {
        std::cout << "create window returned null pointer. aborting.";
        glfwTerminate();
        return -1;
    }    
    
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
    }

    // Cleaning resources
    glfwDestroyWindow(vulkan_engine_main_window);
    glfwTerminate();

    std::cout << "Vulkan Engine Application terminated" << "\n";
    return 0;
}