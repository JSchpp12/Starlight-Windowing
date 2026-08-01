#pragma once

#include <starlight/core/renderer/DefaultRenderPhaseProvider.hpp>

#include <vulkan/vulkan.hpp>

#include <memory>
#include <vector>

namespace star::windowing
{
class WindowingContext;

/// Builds a SwapChainRenderPhase. Mirrors DefaultRenderPhaseProvider but targets
/// the presented/windowed path: it stores the windowing context and swapchain
/// handle and overrides createRenderTargets to use RenderTargets::forPresentation.
/// build() runs the swapchain pre-setup (presentation queue + PresentationCommands,
/// per-present-image binary render-done semaphores) before reusing
/// DefaultRenderPhaseProvider::buildCore for the shared base prep. Cold-path setup,
/// so a virtual build() is acceptable here.
class SwapChainRenderPhaseProvider : public star::core::renderer::DefaultRenderPhaseProvider
{
  public:
    SwapChainRenderPhaseProvider(WindowingContext *winContext, vk::SwapchainKHR swapchain,
                                 star::core::device::DeviceContext &context,
                                 std::vector<std::shared_ptr<star::StarObject>> objects,
                                 std::shared_ptr<std::vector<star::Light>> lights,
                                 std::shared_ptr<star::StarCamera> camera);

    SwapChainRenderPhaseProvider(WindowingContext *winContext, vk::SwapchainKHR swapchain,
                                 star::core::device::DeviceContext &context,
                                 std::vector<std::shared_ptr<star::StarObject>> objects,
                                 std::shared_ptr<star::core::renderer::FrameData> frameData);

    virtual ~SwapChainRenderPhaseProvider() = default;

    SwapChainRenderPhaseProvider(const SwapChainRenderPhaseProvider &) = delete;
    SwapChainRenderPhaseProvider &operator=(const SwapChainRenderPhaseProvider &) = delete;
    SwapChainRenderPhaseProvider(SwapChainRenderPhaseProvider &&) = default;
    SwapChainRenderPhaseProvider &operator=(SwapChainRenderPhaseProvider &&) = default;

    virtual std::unique_ptr<star::core::renderer::RenderPhase> build(
        star::core::device::DeviceContext &context, star::core::renderer::RenderPhaseRegistry &phases) override;

  protected:
    // Presented/windowed render targets instead of the default offscreen targets.
    virtual star::core::renderer::RenderTargets createRenderTargets(
        star::core::device::DeviceContext &context, star::core::renderer::RenderingContext &renderingContext) override;

  private:
    WindowingContext *m_winContext = nullptr;
    vk::SwapchainKHR m_swapChain;
};
} // namespace star::windowing