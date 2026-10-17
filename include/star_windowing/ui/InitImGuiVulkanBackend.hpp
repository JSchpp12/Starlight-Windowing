#pragma once

#include <starlight/core/Exceptions.hpp>
#include <starlight/core/device/StarDevice.hpp>
#include <starlight/core/device/managers/GraphicsContainer.hpp>
#include <starlight/core/helper/queue/QueueHelpers.hpp>
#include <starlight/wrappers/graphics/StarQueue.hpp>

#include <star_common/EventBus.hpp>

#include <imgui_impl_vulkan.h>

#include <vulkan/vulkan.hpp>

#include <algorithm>
#include <cassert>
#include <cstdint>

namespace star::windowing
{
/// @brief One-shot initializer for the ImGui Vulkan backend. It is invoked by the engine's DescriptorPoolReady event
/// through the same waiter DescriptorRecipe uses, so the backend's descriptor sets are created once the engine signals
/// that its descriptor pools are allocated.
struct InitImGuiVulkanBackend
{
    star::core::device::StarDevice *device{nullptr};
    star::core::device::manager::GraphicsContainer *graphicsManagers{nullptr};
    star::common::EventBus *eventBus{nullptr};
    vk::Instance instance{};
    vk::Format swapchainFormat{};
    uint32_t imageCount{0};
    vk::DescriptorPool descriptorPool{};
    /// Set once the backend is live so the owner knows whether shutdown is safe.
    bool *vulkanInitialized{nullptr};

    int operator()()
    {
        assert(device != nullptr && graphicsManagers != nullptr && eventBus != nullptr);

        auto *presentQueue = star::core::helper::GetEngineDefaultQueue(*eventBus, graphicsManagers->queueManager,
                                                                       star::Queue_Type::Tpresent);
        if (presentQueue == nullptr)
            STAR_THROW("ImGui overlay could not acquire the engine presentation queue");

        auto &dev = *device;
        VkFormat format = static_cast<VkFormat>(swapchainFormat);
        VkPipelineRenderingCreateInfoKHR pipelineRenderingInfo{};
        pipelineRenderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
        pipelineRenderingInfo.colorAttachmentCount = 1;
        pipelineRenderingInfo.pColorAttachmentFormats = &format;

        ImGui_ImplVulkan_InitInfo initInfo{};
        initInfo.ApiVersion = dev.getPhysicalDevice().getProperties().apiVersion;
        initInfo.Instance = static_cast<VkInstance>(instance);
        initInfo.PhysicalDevice = static_cast<VkPhysicalDevice>(dev.getPhysicalDevice());
        initInfo.Device = static_cast<VkDevice>(dev.getVulkanDevice());
        initInfo.QueueFamily = presentQueue->getParentQueueFamilyIndex();
        initInfo.Queue = static_cast<VkQueue>(presentQueue->getVulkanQueue());
        initInfo.DescriptorPool = static_cast<VkDescriptorPool>(descriptorPool);
        initInfo.DescriptorPoolSize = 0;
        initInfo.MinImageCount = std::max<uint32_t>(imageCount, 2);
        initInfo.ImageCount = imageCount;
        initInfo.UseDynamicRendering = true;
        initInfo.PipelineInfoMain.PipelineRenderingCreateInfo = pipelineRenderingInfo;

        if (!ImGui_ImplVulkan_Init(&initInfo))
            STAR_THROW("Failed to initialize the ImGui Vulkan backend");

        if (vulkanInitialized != nullptr)
            *vulkanInitialized = true;

        return 0;
    }
};
} // namespace star::windowing
