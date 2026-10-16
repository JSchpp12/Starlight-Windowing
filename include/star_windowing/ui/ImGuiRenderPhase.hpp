#pragma once

#include <starlight/core/renderer/RenderPhase.hpp>
#include <starlight/core/renderer/RenderingTargetInfo.hpp>
#include <starlight/wrappers/graphics/StarCommandBuffer.hpp>
#include <starlight/wrappers/graphics/StarQueue.hpp>

#include <vulkan/vulkan.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace star::core::device
{
class DeviceContext;
}

namespace star::StarTextures
{
class Texture;
} // namespace star::StarTextures

namespace star::windowing
{
/// @brief Bundles the resources that cycle with each presented frame (one entry per acquired swapchain image)
struct PerFifRenderResource
{
    vk::Image image{};
    vk::ImageView imageView{};
    vk::Semaphore renderDoneSemaphore{};
};

/// @brief Presentation-only overlay render phase backed by Dear ImGui.
class ImGuiRenderPhase : public star::core::renderer::RenderPhase
{
  public:
    using FrameCallback = std::function<void()>;
    /// Reports ImGui's post-frame capture state to the owner (the gui
    /// controller service). Invoked once per frame from frameUpdate().
    using CaptureStateCallback = std::function<void(bool keyboard, bool mouse)>;

    /// @brief Builds a fully wired ImGui overlay phase.
    class Builder
    {
      public:
        explicit Builder(star::core::device::DeviceContext &context);

        Builder &setRenderingTargetInfo(star::core::renderer::RenderingTargetInfo renderingTargetInfo);
        Builder &setSwapchainTextures(std::vector<const star::StarTextures::Texture *> textures);
        Builder &setPresentQueue(StarQueue *presentQueue);
        Builder &setEnabled(bool enabled);
        Builder &setFrameCallbacks(std::vector<FrameCallback> callbacks);
        Builder &setCaptureStateCallback(CaptureStateCallback callback);

        std::unique_ptr<ImGuiRenderPhase> buildUnique();

      private:
        star::core::device::DeviceContext &m_context;
        star::core::renderer::RenderingTargetInfo m_renderingTargetInfo;
        std::vector<const star::StarTextures::Texture *> m_swapchainTextures;
        StarQueue *m_presentQueue{nullptr};
        bool m_enabled = true;
        std::vector<FrameCallback> m_frameCallbacks;
        CaptureStateCallback m_captureStateCallback;
    };

    explicit ImGuiRenderPhase(star::core::renderer::RenderingTargetInfo renderingTargetInfo);
    virtual ~ImGuiRenderPhase() = default;

    ImGuiRenderPhase(const ImGuiRenderPhase &) = delete;
    ImGuiRenderPhase &operator=(const ImGuiRenderPhase &) = delete;
    ImGuiRenderPhase(ImGuiRenderPhase &&) = delete;
    ImGuiRenderPhase &operator=(ImGuiRenderPhase &&) = delete;

    virtual void frameUpdate(star::common::IDeviceContext &context) override;
    virtual void recordCommandBuffer(star::StarCommandBuffer &commandBuffer, const star::common::FrameTracker &ft,
                                     const uint64_t &frameIndex) override;
    virtual star::core::renderer::RenderingTargetInfo getRenderTargetInfo() const override;
    void addFrameCallback(FrameCallback callback);
    void clearFrameCallbacks();
    void setCaptureStateCallback(CaptureStateCallback callback);
    void setEnabled(bool enabled) noexcept
    {
        m_enabled = enabled;
    }
    bool isEnabled() const noexcept
    {
        return m_enabled;
    }

  protected:
    vk::Semaphore submitBuffer(star::StarCommandBuffer &buffer, const star::common::FrameTracker &frameTracker,
                               std::vector<vk::Semaphore> *previousCommandBufferSemaphores,
                               std::vector<vk::Semaphore> &dataSemaphores,
                               std::vector<vk::PipelineStageFlags> &dataWaitPoints,
                               std::vector<std::optional<uint64_t>> &previousSignaledValues);

    void waitForSubmitFence(const int &frameInFlightIndex);
    CaptureStateCallback m_captureStateCallback;
    star::core::renderer::RenderingTargetInfo m_renderingTargetInfo;
    std::vector<PerFifRenderResource> m_perFifResources;
    std::vector<FrameCallback> m_frameCallbacks;
    // one fence per frame in flight so recording never races a pending submit
    std::vector<vk::Fence> m_submitFences;
    // scratch wait-infos for the submission override; reserved once so
    // submitBuffer never allocates per frame
    std::vector<vk::SemaphoreSubmitInfo> m_waitInfos;

    // queue used for the overlay submission + device for fence waits (set by the builder)
    StarQueue *m_presentQueue = nullptr;
    vk::Device m_device{VK_NULL_HANDLE};
    vk::Extent2D m_extent{};
    bool m_enabled = true;
};
} // namespace star::windowing
