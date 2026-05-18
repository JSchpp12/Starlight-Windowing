#include "star_windowing/SwapChainRenderer.hpp"

#include <star_common/HandleTypeRegistry.hpp>
#include <starlight/command/command_order/GetPassInfo.hpp>
#include <starlight/core/Exceptions.hpp>
#include <starlight/core/device/managers/Semaphore.hpp>
#include <starlight/core/device/system/event/ManagerRequest.hpp>
#include <starlight/core/helper/command_buffer/CommandBufferHelpers.hpp>
#include <starlight/core/helper/queue/QueueHelpers.hpp>

#include <GLFW/glfw3.h>

namespace star::windowing
{
static void ApplyRenderBarriersPost(const StarCommandBuffer &cb, const star::common::FrameTracker &fTracker,
                                    const StarTextures::Texture &renderToImage) noexcept
{
    vk::ImageMemoryBarrier2 barriers[1]{vk::ImageMemoryBarrier2()
                                            .setOldLayout(vk::ImageLayout::eColorAttachmentOptimal)
                                            .setNewLayout(vk::ImageLayout::ePresentSrcKHR)
                                            .setSubresourceRange(vk::ImageSubresourceRange()
                                                                     .setAspectMask(vk::ImageAspectFlagBits::eColor)
                                                                     .setBaseMipLevel(0)
                                                                     .setLevelCount(1)
                                                                     .setBaseArrayLayer(0)
                                                                     .setLayerCount(1))
                                            .setImage(renderToImage.getVulkanImage())
                                            .setSrcQueueFamilyIndex(vk::QueueFamilyIgnored)
                                            .setDstQueueFamilyIndex(vk::QueueFamilyIgnored)
                                            .setSrcStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
                                            .setSrcAccessMask(vk::AccessFlagBits2::eColorAttachmentWrite)
                                            .setDstStageMask(vk::PipelineStageFlagBits2::eNone)
                                            .setDstAccessMask(vk::AccessFlagBits2::eNone)};
    cb.buffer(fTracker.getCurrent().getFrameInFlightIndex())
        .pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(barriers));
}

static void ApplyRenderBarriersPrep(const StarCommandBuffer &cb, const star::common::FrameTracker &fTracker,
                                    const StarTextures::Texture &renderToImage) noexcept
{
    vk::ImageMemoryBarrier2 barriers[1]{vk::ImageMemoryBarrier2()
                                            .setOldLayout(renderToImage.getImageLayout())
                                            .setNewLayout(vk::ImageLayout::eColorAttachmentOptimal)
                                            .setSubresourceRange(vk::ImageSubresourceRange()
                                                                     .setAspectMask(vk::ImageAspectFlagBits::eColor)
                                                                     .setBaseMipLevel(0)
                                                                     .setLevelCount(1)
                                                                     .setBaseArrayLayer(0)
                                                                     .setLayerCount(1))
                                            .setImage(renderToImage.getVulkanImage())
                                            .setSrcQueueFamilyIndex(vk::QueueFamilyIgnored)
                                            .setDstQueueFamilyIndex(vk::QueueFamilyIgnored)
                                            .setSrcStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
                                            .setSrcAccessMask(vk::AccessFlagBits2::eNone)
                                            .setDstStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
                                            .setDstAccessMask(vk::AccessFlagBits2::eColorAttachmentWrite)};

    cb.buffer(fTracker.getCurrent().getFrameInFlightIndex())
        .pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(barriers));
}
} // namespace star::windowing

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
      m_presentationCommands(std::move(other.m_presentationCommands)),
      m_presentationQueueToUse(std::move(other.m_presentationQueueToUse)), m_cmdBus(other.m_cmdBus)
{
    m_presentationCommands.init(&m_presentationSharedDeps, &m_swapChain, m_presentationQueueToUse);
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
        m_presentationQueueToUse = std::move(other.m_presentationQueueToUse);
        m_cmdBus = other.m_cmdBus;

        m_presentationCommands.init(&m_presentationSharedDeps, &m_swapChain, m_presentationQueueToUse);
    }

    return *this;
}

void star::windowing::SwapChainRenderer::prepRender(common::IDeviceContext &c)
{
    auto &context = static_cast<core::device::DeviceContext &>(c);
    const size_t numSwapChainImages = context.getDevice().getVulkanDevice().getSwapchainImagesKHR(m_swapChain).size();

    this->imageAvailableSemaphores =
        CreateSemaphores(context, context.frameTracker().getSetup().getNumUniqueTargetFramesForFinalization(), true);
    const auto binaryDoneSemaphores = CreateSemaphores(context, numSwapChainImages, false);
    rawBinaryRenderDoneSemaphores.resize(binaryDoneSemaphores.size());
    for (size_t i{0}; i < binaryDoneSemaphores.size(); i++)
    {
        rawBinaryRenderDoneSemaphores[i] =
            context.getGraphicsManagers().semaphoreManager->get(binaryDoneSemaphores[i])->semaphore;
    }

    m_presentationQueueToUse = core::helper::GetEngineDefaultQueue(
        context.getEventBus(), context.getGraphicsManagers().queueManager, star::Queue_Type::Tpresent);

    if (m_presentationQueueToUse == nullptr)
    {
        STAR_THROW("Failed to acquire a presentation queue from engine");
    }

    m_presentationCommands.init(&m_presentationSharedDeps, &m_swapChain, m_presentationQueueToUse);
    m_presentationCommands.prepRender(context);

    m_cmdBus = &context.getCmdBus();

    DefaultRenderer::prepRender(c);
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
        .waitStage = vk::PipelineStageFlagBits::eAllCommands,
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
    std::vector<vk::Semaphore> *previousCommandBufferSemaphores, std::vector<vk::Semaphore> &dataSemaphores,
    std::vector<vk::PipelineStageFlags> &dataWaitPoints, std::vector<std::optional<uint64_t>> &previousSignaledValues)
{
    assert(m_presentationQueueToUse != nullptr);

    const size_t frameIndex = static_cast<size_t>(frameTracker.getCurrent().getFrameInFlightIndex());
    const size_t presentImageIndex = static_cast<size_t>(frameTracker.getCurrent().getFinalTargetImageIndex());

    vk::SemaphoreSubmitInfo waitInfo[10];
    waitInfo[0]
        .setSemaphore(*m_winContext->syncInfo.swapChainAcquireSemaphore)
        .setStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
        .setValue(0);
    uint8_t waitInfoCount{1};
    assert(dataSemaphores.size() == dataWaitPoints.size());
    for (size_t i = 0; i < dataSemaphores.size(); i++)
    {
        const vk::PipelineStageFlags2 stage2 =
            static_cast<vk::PipelineStageFlags2>(static_cast<uint32_t>(dataWaitPoints[i]));

        uint64_t value = 0;
        if (previousSignaledValues[i].has_value())
            value = previousSignaledValues[i].value();

        waitInfo[waitInfoCount].setSemaphore(dataSemaphores[i]).setStageMask(stage2).setValue(value);
        waitInfoCount++;
    }

    const vk::CommandBufferSubmitInfo cbInfo =
        vk::CommandBufferSubmitInfo().setCommandBuffer(buffer.buffer(frameIndex));

    // get my and neighbor info
    vk::Semaphore mySemaphore{VK_NULL_HANDLE};
    uint64_t mySemaphoreSignalValue{0};
    {
        auto cmd = star::command_order::GetPassInfo{m_commandBuffer};
        m_cmdBus->submit(cmd);
        mySemaphore = cmd.getReply().get().signaledSemaphore;
        mySemaphoreSignalValue = cmd.getReply().get().toSignalValue;

        assert(cmd.getReply().get().edges != nullptr &&
               "No neighbor command buffers were registered. At least one is expected");
        assert(cmd.getReply().get().edges->size() + dataWaitPoints.size() < 10 &&
               "Static size container for wait semaphore info only expects a max of 10");
        for (const auto &edge : *cmd.getReply().get().edges)
        {
            if (edge.consumer == m_commandBuffer)
            {
                auto nCmd = star::command_order::GetPassInfo{edge.producer};
                m_cmdBus->submit(nCmd);

                waitInfo[waitInfoCount]
                    .setSemaphore(nCmd.getReply().get().signaledSemaphore)
                    .setValue(nCmd.getReply().get().toSignalValue)
                    .setStageMask(vk::PipelineStageFlagBits2::eAllCommands);

                waitInfoCount++;
            }
        }
    }

    const vk::SemaphoreSubmitInfo signalInfo[2]{vk::SemaphoreSubmitInfo()
                                                    .setSemaphore(mySemaphore)
                                                    .setStageMask(vk::PipelineStageFlagBits2::eAllCommands)
                                                    .setValue(mySemaphoreSignalValue),
                                                vk::SemaphoreSubmitInfo()
                                                    .setSemaphore(rawBinaryRenderDoneSemaphores[presentImageIndex])
                                                    .setValue(0)
                                                    .setStageMask(vk::PipelineStageFlagBits2::eAllCommands)};

    const vk::SubmitInfo2 submitInfo = vk::SubmitInfo2()
                                           .setPWaitSemaphoreInfos(waitInfo)
                                           .setWaitSemaphoreInfoCount(waitInfoCount)
                                           .setCommandBufferInfos(cbInfo)
                                           .setSignalSemaphoreInfos(signalInfo);

    assert(m_winContext->syncInfo.imageAvailableFence != nullptr);
    const vk::Fence &fence = *m_winContext->syncInfo.imageAvailableFence;

    const vk::Result result = m_presentationQueueToUse->getVulkanQueue().submit2(1, &submitInfo, fence);
    if (result != vk::Result::eSuccess)
    {
        STAR_THROW("Failed to submit command buffer");
    }

    m_presentationSharedDeps.acquiredSwapChainImageIndex = frameTracker.getCurrent().getFinalTargetImageIndex();

    m_renderingContext.recordDependentImage.get(m_renderToImages[frameTracker.getCurrent().getFinalTargetImageIndex()])
        ->setImageLayout(vk::ImageLayout::ePresentSrcKHR);

    return rawBinaryRenderDoneSemaphores[presentImageIndex];
}

std::vector<star::StarTextures::Texture> star::windowing::SwapChainRenderer::createRenderToImages(
    star::core::device::DeviceContext &device, const uint8_t &numFramesInFlight)
{
    assert(m_presentationQueueToUse != nullptr);

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

        auto oneTimeSetup = core::helper::BeginSingleTimeCommands(device.getDevice(), device.getEventBus(),
                                                                  device.getManagerCommandBuffer().m_manager,
                                                                  star::Queue_Type::Tpresent);

        vk::ImageMemoryBarrier2 barrier[1]{vk::ImageMemoryBarrier2()
                                               .setOldLayout(vk::ImageLayout::eUndefined)
                                               .setNewLayout(vk::ImageLayout::ePresentSrcKHR)
                                               .setSrcQueueFamilyIndex(vk::QueueFamilyIgnored)
                                               .setDstQueueFamilyIndex(vk::QueueFamilyIgnored)
                                               .setImage(newRenderToImages.back().getVulkanImage())
                                               .setSrcAccessMask(vk::AccessFlagBits2::eNone)
                                               .setSrcStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
                                               .setDstAccessMask(vk::AccessFlagBits2::eColorAttachmentWrite)
                                               .setDstStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
                                               .setSubresourceRange(vk::ImageSubresourceRange()
                                                                        .setAspectMask(vk::ImageAspectFlagBits::eColor)
                                                                        .setBaseMipLevel(0)
                                                                        .setLevelCount(1)
                                                                        .setBaseArrayLayer(0)
                                                                        .setLayerCount(1))};

        oneTimeSetup.buffer().pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(barrier));

        core::helper::EndSingleTimeCommands(*m_presentationQueueToUse, std::move(oneTimeSetup));
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

void star::windowing::SwapChainRenderer::recordCommandBuffer(star::StarCommandBuffer &commandBuffer,
                                                             const common::FrameTracker &frameTracker,
                                                             const uint64_t &frameIndex)
{
    commandBuffer.begin(frameTracker.getCurrent().getFrameInFlightIndex());

    StarTextures::Texture *image = m_renderingContext.recordDependentImage.get(
        m_renderToImages[frameTracker.getCurrent().getFinalTargetImageIndex()]);

    ApplyRenderBarriersPrep(commandBuffer, frameTracker, *image);

    this->DefaultRenderer::recordCommands(commandBuffer.buffer(frameTracker.getCurrent().getFrameInFlightIndex()),
                                          frameTracker, frameIndex);

    ApplyRenderBarriersPost(commandBuffer, frameTracker, *image);
    commandBuffer.buffer(frameTracker.getCurrent().getFrameInFlightIndex()).end();
}

std::vector<star::Handle> star::windowing::SwapChainRenderer::CreateSemaphores(
    star::core::device::DeviceContext &context, const uint8_t &numToCreate, const bool &isTimeline)
{
    auto semaphores = std::vector<Handle>(numToCreate);

    for (size_t i{0}; i < (size_t)numToCreate; i++)
    {
        {
            auto request = isTimeline ? core::device::manager::SemaphoreRequest(uint64_t{0})
                                      : core::device::manager::SemaphoreRequest();

            context.getEventBus().emit(core::device::system::event::ManagerRequest(
                common::HandleTypeRegistry::instance()
                    .getType(core::device::manager::GetSemaphoreEventTypeName)
                    .value(),
                std::move(request), semaphores[i]));
        }

        if (!semaphores[i].isInitialized())
        {
            STAR_THROW("failed to create semaphores for a frame");
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