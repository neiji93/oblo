#pragma once

#include <oblo/core/types.hpp>
#include <vulkan/vulkan.h>

namespace oblo::vk
{
    class allocator;
    class resource_manager;
    class single_queue_engine;

    struct sandbox_init_context
    {
        single_queue_engine* engine;
        allocator* allocator;
        resource_manager* resourceManager;
        VkFormat swapchainFormat;
        u32 width;
        u32 height;
    };

    struct sandbox_shutdown_context
    {
        single_queue_engine* engine;
        allocator* allocator;
        resource_manager* resourceManager;
    };

    struct sandbox_render_context
    {
        single_queue_engine* engine;
        allocator* allocator;
        resource_manager* resourceManager;
        VkCommandBuffer commandBuffer;
        VkImage swapchainImage;
        VkImageView swapchainImageView;
        VkFormat swapchainFormat;
        u32 width;
        u32 height;
        u64 frameIndex;
    };

    struct sandbox_update_imgui_context
    {
        single_queue_engine* engine;
        allocator* allocator;
        resource_manager* resourceManager;
    };
}