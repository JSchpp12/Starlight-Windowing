#pragma once

#include "star_windowing/GuiDefinition.hpp"

#include <vulkan/vulkan.hpp>

namespace star::common
{
class EventBus;
}
namespace star::core::device
{
class StarDevice;
namespace manager
{
struct GraphicsContainer;
}
} // namespace star::core::device
namespace star::core::renderer
{
class RenderPhase;
}

namespace star::windowing
{
struct WindowingContext;
}

namespace star::windowing::maker
{
/// @brief Builds the ImGui overlay backends for a windowing context.
class MakeGui
{
  public:
    /// @param device Device the overlay renders through.
    /// @param graphicsManagers Graphics managers; the descriptor pool manager owns the overlay's pool.
    /// @param eventBus Engine event bus (deferred backend init + queue lookup).
    /// @param winContext App-held windowing context; must outlive the ImGui context.
    MakeGui(star::core::device::StarDevice &device, star::core::device::manager::GraphicsContainer &graphicsManagers,
            star::common::EventBus &eventBus, WindowingContext &winContext);

    /// @brief Swapchain whose color targets the overlay draws over. Must be set before calling make().
    MakeGui &setSwapchain(vk::SwapchainKHR swapchain);

    /// @brief Finalization phase that renders the scene beneath the overlay. Must be set before calling make().
    MakeGui &setFinalizationPhase(const star::core::renderer::RenderPhase *finalizationPhase);

    /// @brief Overlay configuration (ini filename, style scale).
    MakeGui &setConfig(const GuiDefinition &config);

    /// @brief Creates the ImGui context + GLFW backend, registers the manager-owned pool and defers the Vulkan
    /// backend init until DescriptorPoolReady fires.
    void make();

  private:
    star::core::device::StarDevice &m_device;
    star::core::device::manager::GraphicsContainer &m_graphicsManagers;
    star::common::EventBus &m_eventBus;
    star::windowing::WindowingContext &m_winContext;
    vk::SwapchainKHR m_swapchain{};
    const star::core::renderer::RenderPhase *m_finalizationPhase{nullptr};
    GuiDefinition m_config{};
};
} // namespace star::windowing::maker
