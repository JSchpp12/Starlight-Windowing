#include "star_windowing/Swapchain.hpp"

#include <star_common/HandleTypeRegistry.hpp>
#include <starlight/core/device/managers/Fence.hpp>
#include <starlight/core/device/managers/Semaphore.hpp>
#include <starlight/core/device/system/event/ManagerRequest.hpp>

#include <cassert>
#include <stdexcept>

namespace star::windowing
{
void WaitForFence(core::device::StarDevice &device, vk::Fence &fence)
{
    const vk::Result result = device.getVulkanDevice().waitForFences(1, &fence, VK_TRUE, UINT64_MAX);

    if (result != vk::Result::eSuccess)
    {
        throw std::runtime_error("Failed to wait for fence");
    }
}

void ResetFence(core::device::StarDevice &device, vk::Fence &fence)
{
    const vk::Result result = device.getVulkanDevice().resetFences(1, &fence);
    if (result != vk::Result::eSuccess)
    {
        throw std::runtime_error("Failed to reset fences");
    }
}

void CreateFences(common::EventBus &eventBus, const size_t &numToCreate, std::vector<star::Handle> &newHandles,
                  std::vector<vk::Fence *> &newFences)
{
    newHandles.resize(numToCreate);
    newFences.resize(numToCreate);

    for (size_t i{0}; i < numToCreate; i++)
    {
        void *r = nullptr;
        eventBus.emit(core::device::system::event::ManagerRequest(
            common::HandleTypeRegistry::instance().getTypeGuaranteedExist(core::device::manager::GetFenceEventName),
            core::device::manager::FenceRequest{true}, newHandles[i], &r));
        if (r == nullptr)
        {
            throw std::runtime_error("Manager did not provide fence");
        }
        newFences[i] = &static_cast<core::device::manager::FenceRecord *>(r)->fence;
    }
}

void CreateSemaphores(common::EventBus &eventBus, const size_t &numToCreate, std::vector<Handle> &newHandles,
                      std::vector<vk::Semaphore *> &newSemaphores)
{
    newHandles.resize(numToCreate);
    newSemaphores.resize(numToCreate);

    for (size_t i{0}; i < numToCreate; i++)
    {
        void *r = nullptr;
        eventBus.emit(core::device::system::event::ManagerRequest{
            common::HandleTypeRegistry::instance().getTypeGuaranteedExist(
                core::device::manager::GetSemaphoreEventTypeName),
            core::device::manager::SemaphoreRequest{false}, newHandles[i], &r});

        if (r == nullptr)
        {
            throw std::runtime_error("Manager did not provide semaphore");
        }
        newSemaphores[i] = &static_cast<core::device::manager::SemaphoreRecord *>(r)->semaphore;
    }
}

SwapChain::SwapChain(WindowingContext *winContext) : m_winContext(winContext)
{
}

void SwapChain::prepRender(core::device::StarDevice &device, common::EventBus &eventBus,
                           common::FrameTracker &deviceFrameTracker)
{
    m_swapChain = createSwapchain(device, deviceFrameTracker);
    m_imagesInFlight.resize(deviceFrameTracker.getSetup().getNumUniqueTargetFramesForFinalization());

    CreateFences(eventBus, deviceFrameTracker.getSetup().getNumFramesInFlight(), m_inFlightFences, m_inFlightFencesRaw);

    for (size_t i{0}; i < m_inFlightFencesRaw.size(); i++)
    {
        m_fenceStorage.manualInsert(m_inFlightFences[i], *m_inFlightFencesRaw[i]);
    }

    CreateSemaphores(eventBus, deviceFrameTracker.getSetup().getNumUniqueTargetFramesForFinalization(),
                     m_imageAcquireSemaphores, m_imageAcquireSemaphoresRaw);
}

void SwapChain::cleanupRender(core::device::StarDevice &device)
{
    if (m_swapChain != VK_NULL_HANDLE)
    {
        device.getVulkanDevice().destroySwapchainKHR(m_swapChain);
        m_swapChain = VK_NULL_HANDLE;
    }
}

vk::ResultValue<uint32_t> SwapChain::acquireNextSwapChainImage(core::device::StarDevice &device,
                                                               const common::FrameTracker &frameTracker) noexcept
{
    assert(m_swapChain && "Swapchain must be created before use");
    const size_t &frameIndex = frameTracker.getCurrent().getFrameInFlightIndex();

    // wait for fences before acquire
    waitForPreviousFrameInFlightToBeDone(device, frameTracker.getCurrent().getFrameInFlightIndex());

    auto result =
        device.getVulkanDevice().acquireNextImageKHR(m_swapChain, UINT64_MAX, *m_imageAcquireSemaphoresRaw[frameIndex]);

    waitForPreviousFrameToBeDoneWithSwapChainImage(device, result.value);

    {
        const size_t acqImage = static_cast<size_t>(result.value);
        // mark as in use
        m_imagesInFlight[acqImage] = m_inFlightFences[frameIndex];

        vk::Fence &fence = m_fenceStorage.get(m_imagesInFlight[acqImage]);
        ResetFence(device, fence);

        m_winContext->syncInfo.imageAvailableFence = &fence;
        m_winContext->syncInfo.swapChainAcquireSemaphore = m_imageAcquireSemaphoresRaw[frameIndex];
    }

    return result;
}

uint8_t SwapChain::getNumImagesGuaranteedInSwapchain(core::device::StarDevice &device) const
{
    assert(m_winContext != nullptr);

    const auto caps = device.getPhysicalDevice().getSurfaceCapabilities2KHR(m_winContext->surface.getVulkanSurface());

    return chooseNumOfImages(caps);
}

vk::SwapchainKHR SwapChain::createSwapchain(core::device::StarDevice &device, common::FrameTracker &deviceFrameTracker)
{
    vk::Extent2D resolution{};
    vk::SurfaceFormatKHR format{};
    uint8_t numImages{0};
    vk::PresentModeKHR presentMode{};
    vk::SurfaceTransformFlagBitsKHR transform{};
    bool doesSupportTransfer{false};
    gatherSwapchainDependencies(device, resolution, format, presentMode, transform, numImages, doesSupportTransfer);

    std::vector<uint32_t> queueFamilyIndices = device.getQueueOwnershipTracker().getAllQueueFamilyIndices();

    vk::SwapchainCreateInfoKHR createInfo =
        vk::SwapchainCreateInfoKHR()
            .setSurface(m_winContext->surface.getVulkanSurface())
            .setMinImageCount(numImages)
            .setImageFormat(format.format)
            .setImageColorSpace(format.colorSpace)
            .setImageExtent(resolution)
            .setImageArrayLayers(1)
            .setImageUsage(doesSupportTransfer
                               ? (vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc)
                               : vk::ImageUsageFlagBits::eColorAttachment)
            .setImageSharingMode(queueFamilyIndices.size() > 1 ? vk::SharingMode::eConcurrent
                                                               : vk::SharingMode::eExclusive)
            .setQueueFamilyIndexCount(queueFamilyIndices.size() > 1 ? (uint32_t)queueFamilyIndices.size() : 0)
            .setPQueueFamilyIndices(queueFamilyIndices.size() > 1 ? queueFamilyIndices.data() : nullptr)
            .setPreTransform(transform)
            .setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
            .setPresentMode(presentMode)
            .setClipped(VK_TRUE)
            .setOldSwapchain(VK_NULL_HANDLE);

    return device.getVulkanDevice().createSwapchainKHR(createInfo);
}

void SwapChain::waitForPreviousFrameInFlightToBeDone(core::device::StarDevice &device, const uint32_t &imageIndex)
{
    WaitForFence(device, *m_inFlightFencesRaw[imageIndex]);
}

void SwapChain::waitForPreviousFrameToBeDoneWithSwapChainImage(core::device::StarDevice &device,
                                                               const uint32_t &imageIndex)
{
    const size_t index = (size_t)imageIndex;
    if (m_imagesInFlight[index].isInitialized())
    {
        WaitForFence(device, m_fenceStorage.get(m_imagesInFlight[index]));
    }
}

void SwapChain::gatherSwapchainDependencies(core::device::StarDevice &device, vk::Extent2D &selectedResolution,
                                            vk::SurfaceFormatKHR &selectedSurfaceFormat,
                                            vk::PresentModeKHR &selectedPresentMode,
                                            vk::SurfaceTransformFlagBitsKHR &selectedTransform,
                                            uint8_t &selectedNumImages, bool &doesSupportTransfer) const
{
    assert(m_winContext != nullptr);
    const vk::SurfaceCapabilities2KHR caps =
        device.getPhysicalDevice().getSurfaceCapabilities2KHR(m_winContext->surface.getVulkanSurface());
    const auto swapSupport = device.getSwapchainSupport(m_winContext->surface.getVulkanSurface());

    selectedResolution = chooseSwapChainExtent(caps);
    selectedSurfaceFormat = chooseSurfaceFormat(swapSupport);
    selectedPresentMode = choosePresentationMode(swapSupport);
    selectedNumImages = chooseNumOfImages(caps);
    selectedTransform = caps.surfaceCapabilities.currentTransform;

    if (caps.surfaceCapabilities.supportedUsageFlags & vk::ImageUsageFlagBits::eTransferSrc)
    {
        doesSupportTransfer = true;
    }
    else
    {
        doesSupportTransfer = false;
    }
}

vk::Extent2D SwapChain::chooseSwapChainExtent(const vk::SurfaceCapabilities2KHR &caps) const
{
    assert(m_winContext != nullptr);

    auto selectedResolution = m_winContext->window.getWindowFramebufferSize();
    selectedResolution.width = std::clamp(selectedResolution.width, caps.surfaceCapabilities.minImageExtent.width,
                                          caps.surfaceCapabilities.maxImageExtent.width);
    selectedResolution.height = std::clamp(selectedResolution.height, caps.surfaceCapabilities.minImageExtent.height,
                                           caps.surfaceCapabilities.maxImageExtent.height);
    return selectedResolution;
}

vk::SurfaceFormatKHR SwapChain::chooseSurfaceFormat(const core::SwapChainSupportDetails &supportDetails) const
{
    for (const auto &availableFormat : supportDetails.formats)
    {
        // check if a format allows 8 bits for R,G,B, and alpha channel
        // use SRGB color space

        if (availableFormat.format == vk::Format::eB8G8R8A8Srgb &&
            availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
        {
            return availableFormat;
        }
    }

    if (supportDetails.formats.empty())
    {
        throw std::runtime_error("Failed to get any available formats for swapchain images");
    }

    // if nothing matches what we are looking for, just take what is available
    return supportDetails.formats.front();
}

uint8_t SwapChain::chooseNumOfImages(const vk::SurfaceCapabilities2KHR &caps) const
{
    const auto min = (uint8_t)caps.surfaceCapabilities.minImageCount;
    // const auto max = (uint8_t)caps.surfaceCapabilities.maxImageCount;

    uint8_t result = min + 1;
    return result;
}

vk::PresentModeKHR SwapChain::choosePresentationMode(const core::SwapChainSupportDetails &supportDetails) const
{
    /*
     * There are a number of swap modes that are in vulkan
     * 1. VK_PRESENT_MODE_IMMEDIATE_KHR: images submitted by application are sent to the screen right away -- can cause
     * tearing
     * 2. VK_PRESENT_MODE_FIFO_RELAXED_KHR: images are placed in a queue and images are sent to the display in time with
     * display refresh (VSYNC like). If queue is full, application has to wait "Vertical blank" -> time when the display
     * is refreshed
     * 3. VK_PRESENT_MODE_FIFO_RELAXED_KHR: same as above. Except if the application is late, and the queue is empty:
     * the next image submitted is sent to display right away instead of waiting for next blank.
     * 4. VK_PRESENT_MODE_MAILBOX_KHR: similar to #2 option. Instead of blocking applicaiton when the queue is full, the
     * images in the queue are replaced with newer images. This mode can be used to render frames as fast as possible
     * while still avoiding tearing. Kind of like "tripple buffering". Does not mean that framerate is unlocked however.
     *   Author of tutorial statement: this mode [4] is a good tradeoff if energy use is not a concern. On mobile
     * devices it might be better to go with [2]
     */

    for (const auto &availablePresentMode : supportDetails.presentModes)
    {
        if (availablePresentMode == vk::PresentModeKHR::eMailbox)
        {
            return availablePresentMode;
        }
    }

    for (const auto &availablePresentMode : supportDetails.presentModes)
    {
        if (availablePresentMode == vk::PresentModeKHR::eImmediate)
        {
            return availablePresentMode;
        }
    }

    // only VK_PRESENT_MODE_FIFO_KHR is guaranteed to be available
    return vk::PresentModeKHR::eFifo;
}
} // namespace star::windowing