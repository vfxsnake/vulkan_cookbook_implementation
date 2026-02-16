#include <iostream>
#include <cstdio>
#include <fstream>
#include <vector>
#include <stdexcept>

#include <lvk/LVK.h>
#include <GLFW/glfw3.h>


// shader file utility function
std::string readShaderFile(const char* file_name)
{
    std::ifstream file(
        file_name, 
        std::ios::binary | std::ios::ate   // read as binary | set the pointer at the end of the file
    );

    if (!file.is_open())
    {
        std::cout << "unable to open the file " << file_name << "\n";
        return std::string();
    }

    size_t file_size = file.tellg();
    std::string buffer;
    buffer.resize(file_size);
    file.seekg(0);
    file.read(buffer.data(), file_size);

    return buffer;
}

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
    std::unique_ptr<lvk::IContext> ctx = lvk::createVulkanContextWithSwapchain(
        vulkan_engine_main_window, width, height, {}
    );
    
    // calling shaders
    const char* vertex_shader_path = "../../shaders/triangle.vert";
    std::string vertex_shader = readShaderFile(vertex_shader_path);
    if (vertex_shader.empty())
    {
        throw std::runtime_error("Failed to load the vertex shader." );
    }

    const char* fragment_shader_path = "../../shaders/triangle.frag";
    std::string fragment_shader = readShaderFile(fragment_shader_path);
    if (fragment_shader.empty())
    {
        throw std::runtime_error("Failed to load the fragment shader.");
    }

    // creating stages
    lvk::ShaderStage vertex_stage = lvk::Stage_Vert;
    lvk::ShaderStage fragment_stage = lvk::Stage_Frag;

    lvk::Holder<lvk::ShaderModuleHandle> vertex_handle = ctx->createShaderModule(
        {
            vertex_shader.c_str(), 
            vertex_stage, 
            "vertex shader"
        }, 
        nullptr
    );    
    lvk::Holder<lvk::ShaderModuleHandle> fragment_handle = ctx->createShaderModule(
        {
            fragment_shader.c_str(), 
            fragment_stage, 
            "fragment shader"
        }, 
        nullptr
    );

    // creating the render pipeline
    lvk::Holder<lvk::RenderPipelineHandle> triangle_rendering_pipeline = ctx->createRenderPipeline(
        {
            .smVert = vertex_handle,
            .smFrag = fragment_handle,
            .color = { {.format = ctx->getSwapchainFormat()} },
        }
    );

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
        lvk::ICommandBuffer& render_buffer = ctx->acquireCommandBuffer();

        // draw commands
        render_buffer.cmdBeginRendering(
            {.color = {{.loadOp = lvk::LoadOp_Clear, .clearColor = {0.01f, 0.01f, 0.01f, 1.0f}}}},
            {.color = {{.texture = ctx->getCurrentSwapchainTexture()}}}
        );

        render_buffer.cmdBindRenderPipeline(triangle_rendering_pipeline);
        render_buffer.cmdDraw(3);
        render_buffer.cmdEndRendering();


        ctx->submit(render_buffer, ctx->getCurrentSwapchainTexture());
    }

    // Cleaning resources
    ctx.reset();
    glfwDestroyWindow(vulkan_engine_main_window);
    glfwTerminate();

    std::cout << "Vulkan Engine Application terminated" << "\n";
    return 0;
}