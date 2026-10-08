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
class WindowingContext;

/// @brief Bundles the resources that cycle with each presented frame (one entry
/// per acquired swapchain image): the image, its view, and the binary
/// render-done semaphore used as the present wait for that image. Grouping them
/// keeps a single lookup instead of indexing several parallel vectors.
struct PerFifRenderResource
{
    vk::Image image{};
    vk::ImageView imageView{};
    vk::Semaphore renderDoneSemaphore{};
};

/// @brief Presentation-only overlay render phase backed by Dear ImGui.
///
/// Unlike the other phases this derives directly from RenderPhase: it owns no
/// StarObjects, no StarRenderGroups and no camera/light FrameData -- the ImGui
/// Vulkan backend owns its own pipeline/descriptor machinery. The phase records
/// an eLoad overlay pass on top of the swapchain image the finalization phase
/// just rendered and leaves the image in PRESENT_SRC, exactly where it found it.
class ImGuiRenderPhase : public star::core::renderer::RenderPhase
{
  public:
    using FrameCallback = std::function<void()>;
    /// Reports ImGui's post-frame capture state to the owner (the gui
    /// controller service). Invoked once per frame from frameUpdate().
    using CaptureStateCallback = std::function<void(bool keyboard, bool mouse)>;

    /// @brief Builds a fully wired ImGui overlay phase.
    ///
    /// Owns every internal resource the phase needs (descriptor pool, per-image
    /// render-done semaphores, submit fences, command-buffer registration) plus
    /// the ImGui context/backend setup, so providers only supply the resolved
    /// swapchain data and configuration.
    class Builder
    {
      public:
        explicit Builder(star::core::device::DeviceContext &context);

        Builder &setWindowingContext(WindowingContext *winContext);
        /// Precomputed rendering target metadata (attachment formats). Stored on
        /// the phase so getRenderTargetInfo() never has to rebuild it.
        Builder &setRenderingTargetInfo(star::core::renderer::RenderingTargetInfo renderingTargetInfo);
        /// Swapchain color textures the overlay draws over. The image, view and
        /// extent are all read from each texture, so callers do not need to
        /// thread a separate extent through.
        Builder &setSwapchainTextures(std::vector<const star::StarTextures::Texture *> textures);
        Builder &setPresentQueue(StarQueue *presentQueue);
        Builder &setIniFilename(std::string iniFilename);
        Builder &setEnabled(bool enabled);
        Builder &setStyleScale(float styleScale);
        Builder &setFrameCallbacks(std::vector<FrameCallback> callbacks);
        Builder &setCaptureStateCallback(CaptureStateCallback callback);

        std::unique_ptr<ImGuiRenderPhase> buildUnique();

      private:
        star::core::device::DeviceContext &m_context;
        WindowingContext *m_winContext{nullptr};
        star::core::renderer::RenderingTargetInfo m_renderingTargetInfo;
        std::vector<const star::StarTextures::Texture *> m_swapchainTextures;
        StarQueue *m_presentQueue{nullptr};
        std::string m_iniFilename{"imgui.ini"};
        bool m_enabled = true;
        float m_styleScale = 1.0f;
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
    virtual void cleanupRender(star::common::IDeviceContext &context) override;
    virtual star::core::renderer::RenderingTargetInfo getRenderTargetInfo() const override;

    /// Emit UI between ImGui::NewFrame() and ImGui::Render(). Invoked every
    /// frame while the overlay is enabled.
    void addFrameCallback(FrameCallback callback);
    void clearFrameCallbacks();

    /// Install the sink which receives ImGui's WantCaptureKeyboard/WantCaptureMouse
    /// flags after each frame. This is how the gui controller service keeps the
    /// single authoritative capture state.
    void setCaptureStateCallback(CaptureStateCallback callback);

    /// When disabled the phase still records its barriers and an empty draw so
    /// the present-wait semaphore chain keeps the same shape, but application
    /// callbacks are skipped.
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

    // Members are ordered largest to smallest to keep padding between them to a
    // minimum.
    CaptureStateCallback m_captureStateCallback;
    star::core::renderer::RenderingTargetInfo m_renderingTargetInfo;
    std::string m_iniFilename{"imgui.ini"};
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
    vk::DescriptorPool m_descriptorPool{};
    vk::Extent2D m_extent{};
    bool m_enabled = true;
};
} // namespace star::windowing
