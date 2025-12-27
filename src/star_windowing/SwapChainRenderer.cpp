#include "star_windowing/SwapChainRenderer.hpp"

#include <star_common/HandleTypeRegistry.hpp>
#include <starlight/common/ConfigFile.hpp>
#include <starlight/core/device/managers/Semaphore.hpp>
#include <starlight/core/device/system/event/ManagerRequest.hpp>

#include <GLFW/glfw3.h>

star::windowing::SwapChainRenderer::SwapChainRenderer(WindowingContext *winContext, vk::SwapchainKHR swapChain,
                                                      core::device::DeviceContext &context,
                                                      const uint8_t &numFramesInFlight,
                                                      std::vector<std::shared_ptr<StarObject>> objects,
                                                      std::shared_ptr<std::vector<Light>> lights,
                                                      std::shared_ptr<StarCamera> camera)
    : DefaultRenderer(context, numFramesInFlight, std::move(lights), std::move(camera), std::move(objects)),
      m_winContext(winContext), m_swapChain(std::move(swapChain)), numFramesInFlight(numFramesInFlight),
      device(&context)
{
}

star::windowing::SwapChainRenderer::SwapChainRenderer(
    WindowingContext *winContext, vk::SwapchainKHR swapChain, core::device::DeviceContext &context,
    const uint8_t &numFramesInFlight, std::vector<std::shared_ptr<StarObject>> objects,
    std::shared_ptr<ManagerController::RenderResource::Buffer> lightData,
    std::shared_ptr<ManagerController::RenderResource::Buffer> lightListData,
    std::shared_ptr<ManagerController::RenderResource::Buffer> cameraData)
    : DefaultRenderer(context, numFramesInFlight, std::move(objects), std::move(lightData), std::move(lightListData),
                      std::move(cameraData)),
      m_winContext(winContext), m_swapChain(std::move(swapChain)), numFramesInFlight(numFramesInFlight),
      device(&context)
{
}

star::windowing::SwapChainRenderer::SwapChainRenderer(SwapChainRenderer &&other)
    : DefaultRenderer(std::move(other)), m_winContext(other.m_winContext), m_swapChain(std::move(other.m_swapChain)),
      device(other.device), numFramesInFlight(std::move(other.numFramesInFlight)),
      m_presentationSharedDeps(std::move(other.m_presentationSharedDeps)),
      m_presentationCommands(std::move(other.m_presentationCommands))
{
    m_presentationCommands.init(&m_presentationSharedDeps, &m_swapChain);
}

star::windowing::SwapChainRenderer &star::windowing::SwapChainRenderer::operator=(SwapChainRenderer &&other)
{
    if (this != &other)
    {
        DefaultRenderer::operator=(std::move(other));

        m_winContext = other.m_winContext;
        device = other.device;
        m_swapChain = other.m_swapChain;
        m_presentationSharedDeps = std::move(other.m_presentationSharedDeps);
        m_presentationCommands = std::move(other.m_presentationCommands);

        m_presentationCommands.init(&m_presentationSharedDeps, &m_swapChain);
    }

    return *this;
}

void star::windowing::SwapChainRenderer::prepRender(common::IDeviceContext &context)
{
    DefaultRenderer::prepRender(context);

    auto &c = static_cast<core::device::DeviceContext &>(context);
    const size_t numSwapChainImages = c.getDevice().getVulkanDevice().getSwapchainImagesKHR(m_swapChain).size();

    this->imageAvailableSemaphores =
        CreateSemaphores(c, c.getFrameTracker().getSetup().getNumUniqueTargetFramesForFinalization(), false);

    // this->createFences(c);
    // this->createFenceImageTracking();

    m_presentationCommands.init(&m_presentationSharedDeps, &m_swapChain);

    m_presentationCommands.prepRender(c);
}

void star::windowing::SwapChainRenderer::cleanupRender(common::IDeviceContext &context)
{
    DefaultRenderer::cleanupRender(context);
}

void star::windowing::SwapChainRenderer::frameUpdate(common::IDeviceContext &context)
{
    DefaultRenderer::frameUpdate(context);

    auto &c = static_cast<core::device::DeviceContext &>(context);
    prepareRenderingContext(c);
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

vk::Semaphore star::windowing::SwapChainRenderer::submitBuffer(
    StarCommandBuffer &buffer, const star::common::FrameTracker &frameTracker,
    std::vector<vk::Semaphore> *previousCommandBufferSemaphores, std::vector<vk::Semaphore> dataSemaphores,
    std::vector<vk::PipelineStageFlags> dataWaitPoints, std::vector<std::optional<uint64_t>> previousSignaledValues)
{
    size_t frameIndex = static_cast<size_t>(frameTracker.getCurrent().getFrameInFlightIndex());

    std::vector<vk::Semaphore> waitSemaphores = {*m_winContext->syncInfo.swapChainAcquireSemaphore};
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
        this->imageAvailableSemaphores[frameTracker.getCurrent().getFinalTargetImageIndex()]);
    assert(signalSemaphore != nullptr &&
           "Signal semaphore was not properly added to the rendering context before record");

    vk::SubmitInfo submitInfo{};
    submitInfo.commandBufferCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores.data();
    submitInfo.waitSemaphoreCount = waitSemaphoreCount;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphore;
    submitInfo.pWaitDstStageMask = waitStages.data();
    submitInfo.pCommandBuffers = &buffer.buffer(frameIndex);
    submitInfo.commandBufferCount = 1;

    assert(m_winContext->syncInfo.imageAvailableFence != nullptr);
    const vk::Fence &fence = *m_winContext->syncInfo.imageAvailableFence;
    auto commandResult = std::make_unique<vk::Result>(this->device->getDevice()
                                                          .getDefaultQueue(star::Queue_Type::Tpresent)
                                                          .getVulkanQueue()
                                                          .submit(1, &submitInfo, fence));
    if (*commandResult != vk::Result::eSuccess)
    {
        throw std::runtime_error("Failed to submit command buffer");
    }

    m_presentationSharedDeps.acquiredSwapChainImageIndex = frameTracker.getCurrent().getFinalTargetImageIndex();

    m_renderingContext.recordDependentImage.get(m_renderToImages[frameIndex])
        ->setImageLayout(vk::ImageLayout::ePresentSrcKHR);

    return *signalSemaphore;
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
    for (vk::Image &image : this->device->getDevice().getVulkanDevice().getSwapchainImagesKHR(m_swapChain))
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
    const common::FrameTracker &frameTracker)
{
    size_t index = static_cast<size_t>(frameTracker.getCurrent().getFinalTargetImageIndex());

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
                                                             const common::FrameTracker &frameTracker,
                                                             const uint64_t &frameIndex)
{
    auto barriers = getImageBarriersForThisFrame(frameTracker);
    uint32_t numBarriers;
    common::helper::SafeCast<size_t, uint32_t>(barriers.size(), numBarriers);

    commandBuffer.pipelineBarrier2(
        vk::DependencyInfo().setImageMemoryBarrierCount(numBarriers).setPImageMemoryBarriers(barriers.data()));

    this->DefaultRenderer::recordCommandBuffer(commandBuffer, frameTracker, frameIndex);
}

std::vector<star::Handle> star::windowing::SwapChainRenderer::CreateSemaphores(
    star::core::device::DeviceContext &context, const uint8_t &numToCreate, const bool &isTimeline)
{
    auto semaphores = std::vector<Handle>(numToCreate);

    for (size_t i{0}; i < (size_t)numToCreate; i++)
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
}

void star::windowing::SwapChainRenderer::prepareRenderingContext(core::device::DeviceContext &context)
{
    addSemaphoresToRenderingContext(context);
}

void star::windowing::SwapChainRenderer::addSemaphoresToRenderingContext(core::device::DeviceContext &context)
{
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

std::vector<vk::ImageMemoryBarrier2> star::windowing::SwapChainRenderer::getImageBarriersForThisFrame(
    const common::FrameTracker &frameTracker)
{
    StarTextures::Texture *image = m_renderingContext.recordDependentImage.get(
        m_renderToImages[frameTracker.getCurrent().getFrameInFlightIndex()]);

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
