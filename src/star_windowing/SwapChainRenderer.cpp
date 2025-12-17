#include "star_windowing/SwapChainRenderer.hpp"

#include <starlight/common/ConfigFile.hpp>
#include <starlight/core/device/managers/Semaphore.hpp>
#include <starlight/core/device/system/event/ManagerRequest.hpp>

#include <star_common/HandleTypeRegistry.hpp>

#include <GLFW/glfw3.h>

star::windowing::SwapChainRenderer::SwapChainRenderer(WindowingContext *winContext,
                                                      core::device::DeviceContext &context,
                                                      const uint8_t &numFramesInFlight,
                                                      std::vector<std::shared_ptr<StarObject>> objects,
                                                      std::shared_ptr<std::vector<Light>> lights,
                                                      std::shared_ptr<StarCamera> camera)
    : DefaultRenderer(context, numFramesInFlight, std::move(lights), std::move(camera), std::move(objects)),
      m_winContext(winContext), numFramesInFlight(numFramesInFlight), device(&context), swapChain(VK_NULL_HANDLE)
{
    createSwapChain(context);
}

star::windowing::SwapChainRenderer::SwapChainRenderer(
    WindowingContext *winContext, core::device::DeviceContext &context, const uint8_t &numFramesInFlight,
    std::vector<std::shared_ptr<StarObject>> objects,
    std::shared_ptr<ManagerController::RenderResource::Buffer> lightData,
    std::shared_ptr<ManagerController::RenderResource::Buffer> lightListData,
    std::shared_ptr<ManagerController::RenderResource::Buffer> cameraData)
    : DefaultRenderer(context, numFramesInFlight, std::move(objects), std::move(lightData), std::move(lightListData),
                      std::move(cameraData)),
      m_winContext(winContext), numFramesInFlight(numFramesInFlight), device(&context), swapChain(VK_NULL_HANDLE)
{
    createSwapChain(context);
}

star::windowing::SwapChainRenderer::SwapChainRenderer(SwapChainRenderer &&other)
    : DefaultRenderer(std::move(other)), m_winContext(other.m_winContext), device(other.device),
      numFramesInFlight(std::move(other.numFramesInFlight)), swapChain(std::move(other.swapChain)),
      m_presentationSharedDeps(std::move(other.m_presentationSharedDeps)),
      m_presentationCommands(std::move(other.m_presentationCommands))
{
    m_presentationCommands.init(&m_presentationSharedDeps, &swapChain);
}

star::windowing::SwapChainRenderer &star::windowing::SwapChainRenderer::operator=(SwapChainRenderer &&other)
{
    if (this != &other)
    {
        DefaultRenderer::operator=(std::move(other));

        m_winContext = other.m_winContext;
        device = other.device;
        swapChain = other.swapChain;
        m_presentationSharedDeps = std::move(other.m_presentationSharedDeps);
        m_presentationCommands = std::move(other.m_presentationCommands);

        m_presentationCommands.init(&m_presentationSharedDeps, &swapChain);
    }

    return *this;
}

void star::windowing::SwapChainRenderer::prepRender(common::IDeviceContext &context, const uint8_t &numFramesInFlight)
{
    DefaultRenderer::prepRender(context, numFramesInFlight);

    auto &c = static_cast<core::device::DeviceContext &>(context);
    const size_t numSwapChainImages = c.getDevice().getVulkanDevice().getSwapchainImagesKHR(this->swapChain).size();

    this->imageAcquireSemaphores = CreateSemaphores(c, numFramesInFlight, false);
    this->imageAvailableSemaphores = CreateSemaphores(c, numSwapChainImages, false);

    this->createFences(c);
    this->createFenceImageTracking();

    m_presentationCommands.init(&m_presentationSharedDeps, &swapChain);

    m_presentationCommands.prepRender(c);
}

void star::windowing::SwapChainRenderer::cleanupRender(common::IDeviceContext &context)
{
    DefaultRenderer::cleanupRender(context);

    auto &c = static_cast<core::device::DeviceContext &>(context);
    cleanupSwapChain(c);
}

void star::windowing::SwapChainRenderer::frameUpdate(common::IDeviceContext &context, const uint8_t &frameInFlightIndex)
{
    DefaultRenderer::frameUpdate(context, frameInFlightIndex);

    auto &c = static_cast<core::device::DeviceContext &>(context);
    prepareRenderingContext(c);
}

void star::windowing::SwapChainRenderer::submitPresentation(const int &frameIndexToBeDrawn,
                                                            const vk::Semaphore *mainGraphicsDoneSemaphore)
{
    /* Presentation */
    vk::PresentInfoKHR presentInfo{};
    presentInfo.sType = vk::StructureType::ePresentInfoKHR;

    // what to wait for
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = mainGraphicsDoneSemaphore;

    // what swapchains to present images to
    vk::SwapchainKHR swapChains[] = {swapChain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;

    presentInfo.pImageIndices = &m_presentationSharedDeps.acquiredSwapChainImageIndex;

    // can use this to get results from swap chain to check if presentation was successful
    presentInfo.pResults = nullptr; // Optional

    // make call to present image
    auto presentResult =
        this->device->getDevice().getDefaultQueue(star::Queue_Type::Tpresent).getVulkanQueue().presentKHR(presentInfo);

    if (presentResult == vk::Result::eErrorOutOfDateKHR || presentResult == vk::Result::eSuboptimalKHR ||
        frameBufferResized)
    {
        frameBufferResized = false;
        recreateSwapChain();
    }
    else if (presentResult != vk::Result::eSuccess)
    {
        throw std::runtime_error("failed to present swap chain image");
    }
}

star::core::device::manager::ManagerCommandBuffer::Request star::windowing::SwapChainRenderer::getCommandBufferRequest()
{
    return core::device::manager::ManagerCommandBuffer::Request{
        .recordBufferCallback = std::bind(&SwapChainRenderer::recordCommandBuffer, this, std::placeholders::_1,
                                          std::placeholders::_2, std::placeholders::_3),
        .order = Command_Buffer_Order::main_render_pass,
        .orderIndex = Command_Buffer_Order_Index::first,
        .type = Queue_Type::Tgraphics,
        .waitStage = vk::PipelineStageFlagBits::eFragmentShader,
        .willBeSubmittedEachFrame = true,
        .recordOnce = false,
        .beforeBufferSubmissionCallback =
            std::bind(&SwapChainRenderer::prepareForSubmission, this, std::placeholders::_1),
        .overrideBufferSubmissionCallback =
            std::bind(&SwapChainRenderer::submitBuffer, this, std::placeholders::_1, std::placeholders::_2,
                      std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6)};
}

vk::SurfaceFormatKHR star::windowing::SwapChainRenderer::chooseSwapSurfaceFormat(
    const std::vector<vk::SurfaceFormatKHR> &availableFormats) const
{
    for (const auto &availableFormat : availableFormats)
    {
        // check if a format allows 8 bits for R,G,B, and alpha channel
        // use SRGB color space

        if (availableFormat.format == vk::Format::eB8G8R8A8Srgb &&
            availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
        {
            return availableFormat;
        }
    }

    // if nothing matches what we are looking for, just take what is available
    return availableFormats[0];
}

bool star::windowing::SwapChainRenderer::doesSwapChainSupportTransferOperations(
    core::device::DeviceContext &context) const
{
    core::SwapChainSupportDetails swapChainSupport =
        device->getDevice().getSwapchainSupport(m_winContext->surface.getVulkanSurface());

    if (swapChainSupport.capabilities.supportedUsageFlags & vk::ImageUsageFlagBits::eTransferSrc)
        return true;

    return false;
}

vk::Format star::windowing::SwapChainRenderer::getColorAttachmentFormat(star::core::device::DeviceContext &device) const
{
    core::SwapChainSupportDetails swapChainSupport =
        device.getDevice().getSwapchainSupport(m_winContext->surface.getVulkanSurface());

    vk::SurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    return surfaceFormat.format;
}

vk::PresentModeKHR star::windowing::SwapChainRenderer::chooseSwapPresentMode(
    const std::vector<vk::PresentModeKHR> &availablePresentModes)
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

    for (const auto &availablePresentMode : availablePresentModes)
    {
        if (availablePresentMode == vk::PresentModeKHR::eMailbox)
        {
            return availablePresentMode;
        }
    }

    for (const auto &availablePresentMode : availablePresentModes)
    {
        if (availablePresentMode == vk::PresentModeKHR::eImmediate)
        {
            return availablePresentMode;
        }
    }

    // only VK_PRESENT_MODE_FIFO_KHR is guaranteed to be available
    return vk::PresentModeKHR::eFifo;
}

vk::Extent2D star::windowing::SwapChainRenderer::chooseSwapExtent(const vk::SurfaceCapabilitiesKHR &capabilities)
{
    /*
     * "swap extent" -> resolution of the swap chain images (usually the same as window resultion
     * Range of available resolutions are defined in VkSurfaceCapabilitiesKHR
     * Resolution can be changed by setting value in currentExtent to the maximum value of a uint32_t
     *   then: the resolution can be picked by matching window size in minImageExtent and maxImageExtent
     */
    if (capabilities.currentExtent.width != UINT32_MAX)
    {
        return capabilities.currentExtent;
    }
    else
    {
        // vulkan requires that resolution be defined in pixels -- if a high DPI display is used, screen coordinates do
        // not match with pixels

        vk::Extent2D actualExtent = m_winContext->window.getWindowFramebufferSize();

        //(clamp) -- keep the width and height bounded by the permitted resolutions
        actualExtent.width =
            std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height =
            std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return actualExtent;
    }
}

void star::windowing::SwapChainRenderer::prepareForSubmission(const int &frameIndexToBeDrawn)
{
    /* Goals of each call to drawFrame:
     *   get an image from the swap chain
     *   execute command buffer with that image as attachment in the framebuffer
     *   return the image to swapchain for presentation
     * by default each of these steps would be executed asynchronously so need method of synchronizing calls with
     * rendering two ways of doing this:
     *   1. fences
     *       accessed through calls to vkWaitForFences
     *       designed to synchronize application itself with rendering ops
     *   2. semaphores
     *       designed to synchronize opertaions within or across command queues
     * need to sync queue operations of draw and presentation commmands -> using semaphores
     */

    // wait for fence to be ready
    //  3. 'VK_TRUE' -> waiting for all fences
    //  4. timeout

    {
        auto result = this->device->getDevice().getVulkanDevice().waitForFences(
            m_renderingContext.recordDependentFence.get(inFlightFences[frameIndexToBeDrawn]), VK_TRUE, UINT64_MAX);
        if (result != vk::Result::eSuccess)
            throw std::runtime_error("Failed to wait for fences");
    }

    /* Get Image From Swapchain */

    // as is extension we must use vk*KHR naming convention
    // UINT64_MAX -> 3rd argument: used to specify timeout in nanoseconds for image to become available
    /* Suboptimal SwapChain notes */
    // vulkan can return two different flags
    //  1. VK_ERROR_OUT_OF_DATE_KHR: swap chain has become incompatible with the surface and cant be used for rendering.
    //  (Window resize)
    //  2. VK_SUBOPTIMAL_KHR: swap chain can still be used to present to the surface, but the surface properties no
    //  longer match
    {
        auto result = this->device->getDevice().getVulkanDevice().acquireNextImageKHR(
            swapChain, UINT64_MAX,
            m_renderingContext.recordDependentSemaphores.get(this->imageAcquireSemaphores[frameIndexToBeDrawn]));

        if (result.result == vk::Result::eErrorOutOfDateKHR)
        {
            // the swapchain is no longer optimal according to vulkan. Must recreate a more efficient swap chain
            recreateSwapChain();
            return;
        }
        else if (result.result != vk::Result::eSuccess && result.result != vk::Result::eSuboptimalKHR)
        {
            // for VK_SUBOPTIMAL_KHR can also recreate swap chain. However, chose to continue to presentation stage
            throw std::runtime_error("failed to acquire swap chain image");
        }

        m_presentationSharedDeps.acquiredSwapChainImageIndex = result.value;
    }

    // check if a previous frame is using the current image
    if (imagesInFlight[m_presentationSharedDeps.acquiredSwapChainImageIndex].isInitialized())
    {
        const vk::Result result = this->device->getDevice().getVulkanDevice().waitForFences(
            1,
            &m_renderingContext.recordDependentFence.get(
                imagesInFlight[m_presentationSharedDeps.acquiredSwapChainImageIndex]),
            VK_TRUE, UINT64_MAX);
        if (result != vk::Result::eSuccess)
            throw std::runtime_error("Failed to wait for fences");
    }
    // mark image as now being in use by this frame by assigning the fence to it
    imagesInFlight[m_presentationSharedDeps.acquiredSwapChainImageIndex] = inFlightFences[frameIndexToBeDrawn];

    // set fence to unsignaled state
    const vk::Result resetResult = this->device->getDevice().getVulkanDevice().resetFences(
        1, &m_renderingContext.recordDependentFence.get(
               imagesInFlight[m_presentationSharedDeps.acquiredSwapChainImageIndex]));
    if (resetResult != vk::Result::eSuccess)
        throw std::runtime_error("Failed to reset fences");
}

vk::Semaphore star::windowing::SwapChainRenderer::submitBuffer(
    StarCommandBuffer &buffer, const int &frameIndexToBeDrawn,
    std::vector<vk::Semaphore> *previousCommandBufferSemaphores, std::vector<vk::Semaphore> dataSemaphores,
    std::vector<vk::PipelineStageFlags> dataWaitPoints, std::vector<std::optional<uint64_t>> previousSignaledValues)
{
    size_t frameIndex = static_cast<size_t>(frameIndexToBeDrawn);

    std::vector<vk::Semaphore> waitSemaphores = {
        m_renderingContext.recordDependentSemaphores.get(this->imageAcquireSemaphores[frameIndex])};
    std::vector<vk::PipelineStageFlags> waitStages = {vk::PipelineStageFlagBits::eColorAttachmentOutput};

    std::vector<vk::Semaphore> waitTimelines;
    std::vector<uint64_t> waitTimelinesValues;

    if (previousCommandBufferSemaphores != nullptr)
    {
        for (auto &semaphore : *previousCommandBufferSemaphores)
        {
            waitSemaphores.push_back(semaphore);
            waitStages.push_back(vk::PipelineStageFlagBits::eVertexShader);
        }
    }

    assert(dataSemaphores.size() == dataWaitPoints.size());
    for (size_t i = 0; i < dataSemaphores.size(); i++)
    {
        if (previousSignaledValues[i].has_value())
        {
            waitTimelines.push_back(dataSemaphores[i]);
            waitTimelinesValues.push_back(previousSignaledValues[i].value());
        }
        else
        {
            waitSemaphores.push_back(dataSemaphores[i]);
            waitStages.push_back(dataWaitPoints[i]);
        }
    }

    uint32_t waitSemaphoreCount = 0;
    common::helper::SafeCast<size_t, uint32_t>(waitSemaphores.size(), waitSemaphoreCount);

    auto *signalSemaphore = &m_renderingContext.recordDependentSemaphores.get(
        this->imageAvailableSemaphores[m_presentationSharedDeps.acquiredSwapChainImageIndex]);
    vk::SubmitInfo submitInfo{};
    submitInfo.commandBufferCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores.data();
    submitInfo.waitSemaphoreCount = waitSemaphoreCount;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphore;
    submitInfo.pWaitDstStageMask = waitStages.data();
    submitInfo.pCommandBuffers = &buffer.buffer(frameIndex);
    submitInfo.commandBufferCount = 1;

    const auto &fence = m_renderingContext.recordDependentFence.get(inFlightFences[frameIndex]);

    auto commandResult = std::make_unique<vk::Result>(this->device->getDevice()
                                                          .getDefaultQueue(star::Queue_Type::Tpresent)
                                                          .getVulkanQueue()
                                                          .submit(1, &submitInfo, fence));
    if (*commandResult != vk::Result::eSuccess)
    {
        throw std::runtime_error("Failed to submit command buffer");
    }

    m_renderingContext.recordDependentImage.get(m_renderToImages[frameIndex])
        ->setImageLayout(vk::ImageLayout::ePresentSrcKHR);

    auto resultSemaphore = m_renderingContext.recordDependentSemaphores.get(
        this->imageAvailableSemaphores[m_presentationSharedDeps.acquiredSwapChainImageIndex]);

    m_presentationSharedDeps.mainGraphicsDoneSemaphore = resultSemaphore;
    return resultSemaphore;
}

std::vector<star::StarTextures::Texture> star::windowing::SwapChainRenderer::createRenderToImages(
    star::core::device::DeviceContext &device, const uint8_t &numFramesInFlight)
{
    std::vector<StarTextures::Texture> newRenderToImages = std::vector<StarTextures::Texture>();
    const vk::Extent2D winResolution = m_winContext->window.getWindowFramebufferSize(); 
    const vk::Extent3D resolution =
        vk::Extent3D().setWidth(winResolution.width).setHeight(winResolution.height).setDepth(1);

    vk::Format format = getColorAttachmentFormat(device);

    // get images in the newly created swapchain
    for (vk::Image &image : this->device->getDevice().getVulkanDevice().getSwapchainImagesKHR(this->swapChain))
    {
        auto builder =
            star::StarTextures::Texture::Builder(device.getDevice().getVulkanDevice(), image)
                .setSizeInfo(star::StarTextures::Texture::CalculateSize(format, resolution, 1, vk::ImageType::e2D, 1),
                             resolution)
                .setBaseFormat(format)
                .addViewInfo(vk::ImageViewCreateInfo()
                                 .setViewType(vk::ImageViewType::e2D)
                                 .setFormat(format)
                                 .setSubresourceRange(vk::ImageSubresourceRange()
                                                          .setAspectMask(vk::ImageAspectFlagBits::eColor)
                                                          .setBaseArrayLayer(0)
                                                          .setLayerCount(1)
                                                          .setBaseMipLevel(0)
                                                          .setLevelCount(1)));

        newRenderToImages.emplace_back(builder.build());
        newRenderToImages.back().setImageLayout(vk::ImageLayout::ePresentSrcKHR);

        // auto buffer = device->beginSingleTimeCommands();
        // newRenderToImages.back()->transitionLayout(buffer, vk::ImageLayout::ePresentSrcKHR,
        // vk::AccessFlagBits::eNone, vk::AccessFlagBits::eColorAttachmentWrite,
        // vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eColorAttachmentOutput);
        // device->endSingleTimeCommands(buffer);
        auto oneTimeSetup = device.getDevice().beginSingleTimeCommands();

        vk::ImageMemoryBarrier2 barrier{};
        barrier.sType = vk::StructureType::eImageMemoryBarrier2;
        barrier.oldLayout = vk::ImageLayout::eUndefined;
        barrier.newLayout = vk::ImageLayout::ePresentSrcKHR;
        barrier.srcQueueFamilyIndex = vk::QueueFamilyIgnored;
        barrier.dstQueueFamilyIndex = vk::QueueFamilyIgnored;

        barrier.image = newRenderToImages.back().getVulkanImage();
        barrier.srcAccessMask = vk::AccessFlagBits2::eNone;
        barrier.setSrcStageMask(vk::PipelineStageFlagBits2::eTopOfPipe);
        barrier.dstAccessMask = vk::AccessFlagBits2::eNone;
        barrier.setDstStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput);

        barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        barrier.subresourceRange.baseMipLevel = 0; // image does not have any mipmap levels
        barrier.subresourceRange.levelCount = 1;   // image is not an array
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        oneTimeSetup->buffer().pipelineBarrier2(
            vk::DependencyInfo().setPImageMemoryBarriers(&barrier).setImageMemoryBarrierCount(1));

        // oneTimeSetup->buffer().pipelineBarrier(
        //     vk::PipelineStageFlagBits::eTopOfPipe,             // which pipeline stages should
        //                                                        // occurr before barrier
        //     vk::PipelineStageFlagBits::eColorAttachmentOutput, // pipeline stage in
        //                                                        // which operations will
        //                                                        // wait on the barrier
        //     {}, {}, nullptr, barrier);

        device.getDevice().endSingleTimeCommands(std::move(oneTimeSetup));
    }

    return newRenderToImages;
}

vk::RenderingAttachmentInfo star::windowing::SwapChainRenderer::prepareDynamicRenderingInfoColorAttachment(
    const uint8_t &frameInFlightIndex)
{
    size_t index = static_cast<size_t>(m_presentationSharedDeps.acquiredSwapChainImageIndex);

    vk::RenderingAttachmentInfoKHR colorAttachmentInfo{};
    colorAttachmentInfo.imageView =
        m_renderingContext.recordDependentImage.get(m_renderToImages[index])->getImageView();
    colorAttachmentInfo.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    colorAttachmentInfo.loadOp = vk::AttachmentLoadOp::eClear;
    colorAttachmentInfo.storeOp = vk::AttachmentStoreOp::eStore;
    colorAttachmentInfo.clearValue = vk::ClearValue{vk::ClearValue{std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}}};

    return colorAttachmentInfo;
}

void star::windowing::SwapChainRenderer::recordCommandBuffer(vk::CommandBuffer &commandBuffer,
                                                             const uint8_t &frameInFlightIndex,
                                                             const uint64_t &frameIndex)
{
    auto barriers = getImageBarriersForThisFrame();
    uint32_t numBarriers;
    common::helper::SafeCast<size_t, uint32_t>(barriers.size(), numBarriers);

    commandBuffer.pipelineBarrier2(
        vk::DependencyInfo().setImageMemoryBarrierCount(numBarriers).setPImageMemoryBarriers(barriers.data()));

    this->DefaultRenderer::recordCommandBuffer(commandBuffer, frameInFlightIndex, frameIndex);
}

std::vector<star::Handle> star::windowing::SwapChainRenderer::CreateSemaphores(
    star::core::device::DeviceContext &context, const uint8_t &numToCreate, const bool &isTimeline)
{
    auto semaphores = std::vector<Handle>(numToCreate);

    vk::SemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = vk::StructureType::eSemaphoreCreateInfo;

    for (int i = 0; i < numToCreate; i++)
    {
        context.getEventBus().emit(core::device::system::event::ManagerRequest(
            common::HandleTypeRegistry::instance().getType(core::device::manager::GetSemaphoreEventTypeName).value(),
            core::device::manager::SemaphoreRequest{isTimeline}, semaphores[i]));

        if (!semaphores[i].isInitialized())
        {
            throw std::runtime_error("failed to create semaphores for a frame");
        }
    }

    return semaphores;
}

void star::windowing::SwapChainRenderer::createFences(core::device::DeviceContext &context)
{
    // note: fence creation can be rolled into semaphore creation. Seperated for understanding
    inFlightFences.resize(this->numFramesInFlight);

    for (size_t i = 0; i < this->numFramesInFlight; i++)
    {
        context.getEventBus().emit(core::device::system::event::ManagerRequest(
            common::HandleTypeRegistry::instance().getTypeGuaranteedExist(core::device::manager::GetFenceEventName),
            core::device::manager::FenceRequest{true}, inFlightFences[i]));
        if (!this->inFlightFences[i])
        {
            throw std::runtime_error("failed to create fence object for a frame");
        }
    }
}

void star::windowing::SwapChainRenderer::createFenceImageTracking()
{
    // note: just like createFences() this too can be wrapped into semaphore creation. Seperated for understanding.

    // need to ensure the frame that is going to be drawn to, is the one linked to the expected fence.
    // If, for any reason, vulkan returns an image out of order, we will be able to handle that with this link
    imagesInFlight.resize(m_renderToImages.size());

    // initially, no frame is using any image so this is going to be created without an explicit link
}

void star::windowing::SwapChainRenderer::createSwapChain(core::device::DeviceContext &context)
{
    // TODO: current implementation requires halting to all rendering when recreating swapchain. Can place old swap
    // chain in oldSwapChain field
    //   in order to prevent this and allow rendering to continue

    assert(m_winContext != nullptr); 
    auto swapChainSupport = context.getDevice().getSwapchainSupport(m_winContext->surface.getVulkanSurface());
    vk::SurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    vk::PresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    vk::Extent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

    // how many images should be in the swap chain
    // in order to avoid extra waiting for driver overhead, author of tutorial recommends +1 of the minimum
    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;

    // with this additional +1 make sure not to go over maximum permitted
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
    {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    assert(m_winContext != nullptr);
    vk::SwapchainCreateInfoKHR createInfo{};
    createInfo.sType = vk::StructureType::eSwapchainCreateInfoKHR;
    createInfo.surface = m_winContext->surface.getVulkanSurface();

    // specify image information for the surface
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1; // 1 unless using 3D display
    if (doesSwapChainSupportTransferOperations(context))
    {
        createInfo.imageUsage =
            vk::ImageUsageFlagBits::eColorAttachment |
            vk::ImageUsageFlagBits::eTransferSrc; // how are these images going to be used? Color attachment since we
                                                  // are rendering to them (can change for postprocessing effects)
    }
    else
    {
        createInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
    }

    std::vector<uint32_t> queueFamilyIndicies =
        this->device->getDevice().getQueueOwnershipTracker().getAllQueueFamilyIndices();

    if (queueFamilyIndicies.size() > 1)
    {
        /*need to handle how images will be transferred between different queues
         * so we need to draw images on the graphics queue and then submitting them to the presentation queue
         * Two ways of handling this:
         * 1. VK_SHARING_MODE_EXCLUSIVE: an image is owned by one queue family at a time and can be transferred between
         * groups
         * 2. VK_SHARING_MODE_CONCURRENT: images can be used across queue families without explicit ownership
         */
        createInfo.imageSharingMode = vk::SharingMode::eConcurrent;
        createInfo.queueFamilyIndexCount = queueFamilyIndicies.size();
        createInfo.pQueueFamilyIndices = queueFamilyIndicies.data();
    }
    else
    {
        // same family is used for graphics and presenting
        createInfo.imageSharingMode = vk::SharingMode::eExclusive;
        createInfo.queueFamilyIndexCount = 0;     // optional
        createInfo.pQueueFamilyIndices = nullptr; // optional
    }

    // can specify certain transforms if they are supported (like 90 degree clockwise rotation)
    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;

    // what present mode is going to be used
    createInfo.presentMode = presentMode;
    // if clipped is set to true, we dont care about color of pixels that arent in sight -- best performance to enable
    // this
    createInfo.clipped = VK_TRUE;

    // for now, only assume we are making one swapchain
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    this->swapChain = this->device->getDevice().getVulkanDevice().createSwapchainKHR(createInfo);
}

void star::windowing::SwapChainRenderer::recreateSwapChain()
{
    assert(m_winContext != nullptr);

    int width = 0, height = 0;
    // check for window minimization and wait for window size to no longer be 0
    glfwGetFramebufferSize(m_winContext->window.getGLFWWindow(), &width, &height);
    while (width == 0 || height == 0)
    {
        glfwGetFramebufferSize(m_winContext->window.getGLFWWindow(), &width, &height);
        glfwWaitEvents();
    }
    // wait for device to finish any current actions
    vkDeviceWaitIdle(this->device->getDevice().getVulkanDevice());

    cleanupSwapChain(*device);

    // create swap chain itself
    createSwapChain(*device);
}

void star::windowing::SwapChainRenderer::cleanupSwapChain(core::device::DeviceContext &context)
{
    // for (auto &image : this->renderToImages)
    // {
    //     image.cleanupRender(context.getDevice().getVulkanDevice());
    // }
    // for (auto &image : this->renderToDepthImages)
    // {
    //     if (image)
    //     {
    //         image->cleanupRender(context.getDevice().getVulkanDevice());
    //         image.release();
    //     }
    // }

    context.getDevice().getVulkanDevice().destroySwapchainKHR(this->swapChain);
}

void star::windowing::SwapChainRenderer::prepareRenderingContext(core::device::DeviceContext &context)
{
    addSemaphoresToRenderingContext(context);
    addFencesToRenderingContext(context);
}

void star::windowing::SwapChainRenderer::addSemaphoresToRenderingContext(core::device::DeviceContext &context)
{
    for (const auto &semaphore : this->imageAcquireSemaphores)
    {
        m_renderingContext.recordDependentSemaphores.manualInsert(
            semaphore, context.getSemaphoreManager().get(semaphore)->semaphore);
    }

    for (const auto &semaphore : this->graphicsDoneSemaphoresExternalUse)
    {
        m_renderingContext.recordDependentSemaphores.manualInsert(
            semaphore, context.getSemaphoreManager().get(semaphore)->semaphore);
    }

    for (const auto &semaphore : this->imageAvailableSemaphores)
    {
        m_renderingContext.recordDependentSemaphores.manualInsert(
            semaphore, context.getSemaphoreManager().get(semaphore)->semaphore);
    }
}

void star::windowing::SwapChainRenderer::addFencesToRenderingContext(core::device::DeviceContext &context)
{
    for (const auto &fence : inFlightFences)
    {
        m_renderingContext.recordDependentFence.manualInsert(fence, context.getFenceManager().get(fence)->fence);
    }

    for (const auto &fence : imagesInFlight)
    {
        if (fence.isInitialized() && !m_renderingContext.recordDependentFence.contains(fence))
        {
            m_renderingContext.recordDependentFence.manualInsert(fence, context.getFenceManager().get(fence)->fence);
        }
    }
}

std::vector<vk::ImageMemoryBarrier2> star::windowing::SwapChainRenderer::getImageBarriersForThisFrame()
{
    StarTextures::Texture *image = m_renderingContext.recordDependentImage.get(
        m_renderToImages[m_presentationSharedDeps.acquiredSwapChainImageIndex]);

    auto barriers = std::vector<vk::ImageMemoryBarrier2>{
        vk::ImageMemoryBarrier2()
            .setOldLayout(image->getImageLayout())
            .setNewLayout(vk::ImageLayout::eColorAttachmentOptimal)
            .setSubresourceRange(vk::ImageSubresourceRange()
                                     .setAspectMask(vk::ImageAspectFlagBits::eColor)
                                     .setBaseMipLevel(0)
                                     .setLevelCount(1)
                                     .setBaseArrayLayer(0)
                                     .setLayerCount(1))
            .setImage(image->getVulkanImage())
            .setSrcQueueFamilyIndex(vk::QueueFamilyIgnored)
            .setDstQueueFamilyIndex(vk::QueueFamilyIgnored)
            .setSrcStageMask(vk::PipelineStageFlagBits2::eTopOfPipe)
            .setSrcAccessMask(vk::AccessFlagBits2::eNone)
            .setDstStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
            .setDstAccessMask(vk::AccessFlagBits2::eColorAttachmentWrite),
        vk::ImageMemoryBarrier2()
            .setOldLayout(vk::ImageLayout::eColorAttachmentOptimal)
            .setNewLayout(vk::ImageLayout::ePresentSrcKHR)
            .setSubresourceRange(vk::ImageSubresourceRange()
                                     .setAspectMask(vk::ImageAspectFlagBits::eColor)
                                     .setBaseMipLevel(0)
                                     .setLevelCount(1)
                                     .setBaseArrayLayer(0)
                                     .setLayerCount(1))
            .setImage(image->getVulkanImage())
            .setSrcQueueFamilyIndex(vk::QueueFamilyIgnored)
            .setDstQueueFamilyIndex(vk::QueueFamilyIgnored)
            .setSrcStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
            .setSrcAccessMask(vk::AccessFlagBits2::eColorAttachmentWrite)
            .setDstStageMask(vk::PipelineStageFlagBits2::eBottomOfPipe)
            .setDstAccessMask(vk::AccessFlagBits2::eNone)};

    return barriers;
}