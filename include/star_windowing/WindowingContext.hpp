#pragma once

#include "star_windowing/RenderingSurface.hpp"
#include "star_windowing/StarWindow.hpp"

#include <vector>
namespace star::windowing
{
struct WindowingContext
{
    struct CurrentFrameSyncInfo
    {
        vk::Semaphore *swapChainAcquireSemaphore = nullptr;
        vk::Fence *imageAvailableFence = nullptr;
    };

    RenderingSurface surface;
    StarWindow window;
    CurrentFrameSyncInfo syncInfo;
};
} // namespace star::windowing