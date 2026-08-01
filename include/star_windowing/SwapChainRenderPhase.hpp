#pragma once

#include "star_windowing/PresentationCommands.hpp"

#include <starlight/core/renderer/DefaultRenderPhase.hpp>

#include <vulkan/vulkan.hpp>

#include <optional>
#include <vector>

namespace star::core
{
class CommandBus;
}

namespace star::windowing
{
class SwapChainRenderPhaseProvider;
class WindowingContext;

/// Runtime half of the presented (swapchain) renderer. Built once by a
/// SwapChainRenderPhaseProvider; extends DefaultRenderPhase with the per-frame
/// presentation submit override, a final-target-image-indexed color attachment,
/// and swapchain lifecycle (recreate). One-shot setup -- presentation queue and
/// PresentationCommands, the per-present-image binary render-done semaphores,
/// and render-target creation via RenderTargets::forPresentation -- lives on
/// the provider.
class SwapChainRenderPhase : public star::core::renderer::DefaultRenderPhase
{
  public:
    SwapChainRenderPhase() = default;
    virtual ~SwapChainRenderPhase() = default;

    SwapChainRenderPhase(const SwapChainRenderPhase &) = delete;
    SwapChainRenderPhase &operator=(const SwapChainRenderPhase &) = delete;
    SwapChainRenderPhase(SwapChainRenderPhase &&) = delete;
    SwapChainRenderPhase &operator=(SwapChainRenderPhase &&) = delete;

    virtual void frameUpdate(star::common::IDeviceContext &context) override;
    virtual void recordCommandBuffer(star::StarCommandBuffer &commandBuffer,
                                     const star::common::FrameTracker &frameTracker,
                                     const uint64_t &frameIndex) override;

    virtual vk::RenderingAttachmentInfo prepareDynamicRenderingInfoColorAttachment(
        const star::common::FrameTracker &frameTracker) override;

  protected:
    friend class SwapChainRenderPhaseProvider;

    virtual std::optional<star::core::device::manager::ManagerCommandBuffer::BufferSubmissionOverride>
    getSubmissionOverride() override;

    vk::SwapchainKHR m_swapChain;
    PresentationCommands::RecordDependencies m_presentationSharedDeps;
    PresentationCommands m_presentationCommands;
    StarQueue *m_presentationQueueToUse = nullptr;
    WindowingContext *m_winContext = nullptr;
    star::core::device::DeviceContext *device = nullptr;
    const star::core::CommandBus *m_cmdBus = nullptr;

    // tracker for which frame is being processed of the available permitted frames
    uint8_t previousFrame = 0;

    // explicit declaration of resize, used if driver does not trigger VK_ERROR_OUT_OF_DATE
    bool frameBufferResized = false;

    // sync obj storage
    std::vector<vk::Semaphore> rawBinaryRenderDoneSemaphores;
    std::vector<star::Handle> m_timelineSemaphores;

    vk::Semaphore submitBuffer(star::StarCommandBuffer &buffer, const star::common::FrameTracker &frameTracker,
                               std::vector<vk::Semaphore> *previousCommandBufferSemaphores,
                               std::vector<vk::Semaphore> &dataSemaphores,
                               std::vector<vk::PipelineStageFlags> &dataWaitPoints,
                               std::vector<std::optional<uint64_t>> &previousSignaledValues);

    /// If the swapchain is no longer compatible, it must be recreated.
    virtual void recreateSwapChain();
};
} // namespace star::windowing