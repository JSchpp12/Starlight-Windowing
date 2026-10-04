#pragma once

#include <starlight/core/renderer/DefaultRenderPhaseProvider.hpp>

#include <vulkan/vulkan.hpp>

#include <memory>
#include <vector>

namespace star::windowing
{
class WindowingContext;

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

  private:
    WindowingContext *m_winContext = nullptr;
    vk::SwapchainKHR m_swapChain;

    // Presented/windowed render targets instead of the default offscreen targets.
    // Supplied to DefaultRenderPhase::Builder via the target factory.
    star::core::renderer::RenderTargets createRenderTargets(star::core::device::DeviceContext &context,
                                                            star::core::renderer::RenderingContext &renderingContext);
};
} // namespace star::windowing