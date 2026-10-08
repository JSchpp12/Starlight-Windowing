#pragma once

#include "star_windowing/RenderingSurface.hpp"
#include "star_windowing/StarWindow.hpp"

namespace star::windowing
{
struct WindowingContext
{
    struct CurrentFrameSyncInfo
    {
        vk::Semaphore *swapChainAcquireSemaphore = nullptr;
        vk::Fence *imageAvailableFence = nullptr;
    };

    /// Vulkan instance the surface/device were created from. Cached here so
    /// presentation-coupled phases (e.g. the ImGui overlay) can reach it
    /// without threading the instance through the engine's device context.
    vk::Instance instance{VK_NULL_HANDLE};
    RenderingSurface surface;
    StarWindow window;
    CurrentFrameSyncInfo syncInfo;
};
} // namespace star::windowing