#pragma once

#include "star_windowing/WindowingContext.hpp"
#include <star_common/EventBus.hpp>
#include <star_common/FrameTracker.hpp>
#include <starlight/core/MappedHandleContainer.hpp>
#include <starlight/core/device/StarDevice.hpp>

namespace star::windowing
{
class SwapChain
{
  public:
    SwapChain() = default;
    SwapChain(const SwapChain &) = delete;
    SwapChain &operator=(const SwapChain &) = delete;
    SwapChain(SwapChain &&) = default;
    SwapChain &operator=(SwapChain &&) = default;
    ~SwapChain() = default;
    explicit SwapChain(WindowingContext *winContext);

    void prepRender(core::device::StarDevice &device, common::EventBus &eventBus,
                    common::FrameTracker &deviceFrameTracker);

    void cleanupRender(core::device::StarDevice &device);

    uint8_t getNumImagesGuaranteedInSwapchain(core::device::StarDevice &device) const;

    vk::ResultValue<uint32_t> acquireNextSwapChainImage(core::device::StarDevice &device,
                                                        const common::FrameTracker &frameTracker) noexcept;

    vk::SwapchainKHR &getVulkanSwapchain()
    {
        return m_swapChain;
    }

  private:
    std::vector<Handle> m_imageAcquireSemaphores, m_inFlightFences, m_imagesInFlight;
    star::core::MappedHandleContainer<vk::Fence> m_fenceStorage =
        star::core::MappedHandleContainer<vk::Fence>{common::special_types::FenceTypeName};
    std::vector<vk::Fence *> m_inFlightFencesRaw;
    std::vector<vk::Semaphore *> m_imageAcquireSemaphoresRaw;
    vk::SwapchainKHR m_swapChain{VK_NULL_HANDLE};
    WindowingContext *m_winContext = nullptr;

    vk::SwapchainKHR createSwapchain(core::device::StarDevice &device, common::FrameTracker &deviceFrameTracker);

    void waitForPreviousFrameInFlightToBeDone(core::device::StarDevice &device, const uint32_t &imageIndex);

    void waitForPreviousFrameToBeDoneWithSwapChainImage(core::device::StarDevice &device, const uint32_t &imageIndex);

    void gatherSwapchainDependencies(core::device::StarDevice &device, vk::Extent2D &selectedResolution,
                                     vk::SurfaceFormatKHR &selectedSurfaceFormat,
                                     vk::PresentModeKHR &selectedPresentMode,
                                     vk::SurfaceTransformFlagBitsKHR &selectedTransform, uint8_t &selectedNumImages,
                                     bool &doesSupportTransfer) const;

    vk::Extent2D chooseSwapChainExtent(const vk::SurfaceCapabilities2KHR &caps) const;

    vk::SurfaceFormatKHR chooseSurfaceFormat(const core::SwapChainSupportDetails &supportDetails) const;

    uint8_t chooseNumOfImages(const vk::SurfaceCapabilities2KHR &caps) const;

    vk::PresentModeKHR choosePresentationMode(const core::SwapChainSupportDetails &supportDetails) const;
};
} // namespace star::windowing