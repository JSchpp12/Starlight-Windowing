#include "star_windowing/SwapChainRenderPhase.hpp"

#include "star_windowing/WindowingContext.hpp"

#include <starlight/command/command_order/GetPassInfo.hpp>
#include <starlight/command/command_order/TriggerPass.hpp>
#include <starlight/core/Exceptions.hpp>
#include <starlight/core/helper/queue/QueueHelpers.hpp>

#include <GLFW/glfw3.h>

#include <array>
#include <cassert>
#include <functional>
#include <vector>

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

void SwapChainRenderPhase::frameUpdate(star::common::IDeviceContext &context)
{
    auto &c = static_cast<star::core::device::DeviceContext &>(context);
    const size_t ii = static_cast<size_t>(c.frameTracker().getCurrent().getFrameInFlightIndex());

    // Properly submit this renderer through the command-order service each frame
    // (mirrors HeadlessRenderPhase::frameUpdate). Without this TriggerPass the
    // pass's signaled semaphore is never set, leaving an invalid semaphore that
    // propagates as a wait into the manager command buffer's submit.
    c.getCmdBus().submit(star::command_order::TriggerPass()
                             .setTimelineSemaphore(m_timelineSemaphores[ii])
                             .setSignalValue(c.frameTracker().getCurrent().getNumTimesFrameProcessed() + 1)
                             .setPass(m_commandBuffer));

    this->star::core::renderer::DefaultRenderPhase::frameUpdate(context);
}

void SwapChainRenderPhase::recordCommandBuffer(star::StarCommandBuffer &commandBuffer,
                                               const star::common::FrameTracker &frameTracker,
                                               const uint64_t &frameIndex)
{
    commandBuffer.begin(frameTracker.getCurrent().getFrameInFlightIndex());

    StarTextures::Texture *image = m_renderingContext.recordDependentImage.get(
        m_renderToImages[frameTracker.getCurrent().getFinalTargetImageIndex()]);

    ApplyRenderBarriersPrep(commandBuffer, frameTracker, *image);
    this->DefaultRenderPhase::recordCommands(commandBuffer.buffer(frameTracker.getCurrent().getFrameInFlightIndex()),
                                             frameTracker, frameIndex);

    ApplyRenderBarriersPost(commandBuffer, frameTracker, *image);
    commandBuffer.buffer(frameTracker.getCurrent().getFrameInFlightIndex()).end();
}

std::optional<star::core::device::manager::ManagerCommandBuffer::BufferSubmissionOverride> SwapChainRenderPhase::
    getSubmissionOverride()
{
    star::core::device::manager::ManagerCommandBuffer::BufferSubmissionOverride overrideFn =
        std::bind(&SwapChainRenderPhase::submitBuffer, this, std::placeholders::_1, std::placeholders::_2,
                  std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6);
    return overrideFn;
}

vk::RenderingAttachmentInfo SwapChainRenderPhase::prepareDynamicRenderingInfoColorAttachment(
    const star::common::FrameTracker &frameTracker)
{
    const size_t index = static_cast<size_t>(frameTracker.getCurrent().getFinalTargetImageIndex());

    vk::RenderingAttachmentInfoKHR colorAttachmentInfo{};
    colorAttachmentInfo.imageView =
        m_renderingContext.recordDependentImage.get(m_renderToImages[index])->getImageView();
    colorAttachmentInfo.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    colorAttachmentInfo.loadOp = vk::AttachmentLoadOp::eClear;
    colorAttachmentInfo.storeOp = vk::AttachmentStoreOp::eStore;
    colorAttachmentInfo.clearValue = vk::ClearValue{vk::ClearValue{std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}}};

    return colorAttachmentInfo;
}

vk::Semaphore SwapChainRenderPhase::submitBuffer(star::StarCommandBuffer &buffer,
                                                 const star::common::FrameTracker &frameTracker,
                                                 std::vector<vk::Semaphore> *previousCommandBufferSemaphores,
                                                 std::vector<vk::Semaphore> &dataSemaphores,
                                                 std::vector<vk::PipelineStageFlags> &dataWaitPoints,
                                                 std::vector<std::optional<uint64_t>> &previousSignaledValues)
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

        // update neighbor wait information
        if (cmd.getReply().get().edges != nullptr)
        {
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

    return rawBinaryRenderDoneSemaphores[presentImageIndex];
}

void SwapChainRenderPhase::recreateSwapChain()
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
    vkDeviceWaitIdle(device->getDevice().getVulkanDevice());
}
} // namespace star::windowing