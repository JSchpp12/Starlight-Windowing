#pragma once

#include <star_windowing/GuiDefinition.hpp>

#include <starlight/core/renderer/IRenderPhaseProvider.hpp>

#include <vulkan/vulkan.hpp>

#include <functional>
#include <memory>
#include <vector>

namespace star::core::renderer
{
class RenderPhase;
}

namespace star::windowing
{
class WindowingContext;

/// @brief Builds the presentation-only ImGui overlay phase.
///
/// The provider is created by the gui controller service once the main
/// graphics renderer has been registered, and reuses that renderer's swapchain
/// image views. The service queues it on the scene during prepRender, so it is
/// built after the finalization phase it depends on.
class ImGuiRenderPhaseProvider : public star::core::renderer::IRenderPhaseProvider
{
  public:
    ImGuiRenderPhaseProvider(WindowingContext *winContext, vk::SwapchainKHR swapchain,
                             const star::core::renderer::RenderPhase *finalizationPhase,
                             GuiDefinition config = {});
    virtual ~ImGuiRenderPhaseProvider() = default;

    ImGuiRenderPhaseProvider(const ImGuiRenderPhaseProvider &) = delete;
    ImGuiRenderPhaseProvider &operator=(const ImGuiRenderPhaseProvider &) = delete;
    ImGuiRenderPhaseProvider(ImGuiRenderPhaseProvider &&) = default;
    ImGuiRenderPhaseProvider &operator=(ImGuiRenderPhaseProvider &&) = default;

    /// Queue a UI-building callback to be installed on the built phase.
    void addFrameCallback(std::function<void()> callback);

    /// Install the capture-state sink forwarded to the built phase.
    void setCaptureStateCallback(std::function<void(bool, bool)> callback);

    virtual std::unique_ptr<star::core::renderer::RenderPhase> build(
        star::core::device::DeviceContext &context, star::core::renderer::RenderPhaseRegistry &phases) override;

  private:
    WindowingContext *m_winContext = nullptr;
    vk::SwapchainKHR m_swapChain{};
    const star::core::renderer::RenderPhase *m_finalizationPhase = nullptr;
    GuiDefinition m_config{};
    std::vector<std::function<void()>> m_frameCallbacks;
    std::function<void(bool, bool)> m_captureStateCallback;
};
} // namespace star::windowing
