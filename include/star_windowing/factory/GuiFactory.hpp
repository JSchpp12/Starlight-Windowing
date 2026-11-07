#pragma once

#include <vulkan/vulkan.hpp>

#include <string>

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
struct GuiDefinition;
struct WindowingContext;

/// @brief Stateless helpers: the single point of contact with the ImGui library.
namespace factory
{
/// @brief Create the ImGui context + GLFW backend, register the manager-owned descriptor pool through the windowing
/// context, and defer the Vulkan backend init (runs on DescriptorPoolReady).
/// @details Must be called after InteractivityBus::Init so ImGui chains the star GLFW callbacks instead of overwriting
/// them.
/// @param iniFile Receives the resolved ini path; must outlive the ImGui context.
/// @param vulkanInitialized Set once the deferred Vulkan backend init has run.
void MakeGui(star::core::device::StarDevice &device, star::core::device::manager::GraphicsContainer &graphicsManagers,
             star::common::EventBus &eventBus, WindowingContext &winContext, vk::SwapchainKHR swapchain,
             const star::core::renderer::RenderPhase *finalizationPhase, const GuiDefinition &config,
             std::string &iniFile, bool &vulkanInitialized);

/// @brief Run the platform + renderer backends and start a new frame.
void BeginFrame();
/// @brief Finalize the current frame's draw data.
void EndFrame();
/// @brief Report ImGui's capture state for the frame that was just rendered.
void CaptureState(bool &keyboard, bool &mouse);
/// @brief Record the current frame's draw data into a command buffer.
void RenderDrawData(vk::CommandBuffer commandBuffer);

/// @brief Destroy the backends + context.
/// @details The descriptor pool is manager-owned (registered by WindowingContext) and is released during device
/// teardown, so it is deliberately not destroyed here.
void Shutdown(bool &vulkanInitialized);
} // namespace factory
} // namespace star::windowing
