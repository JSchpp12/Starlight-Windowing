#include "star_windowing/maker/MakeGui.hpp"

#include "star_windowing/context/WindowingContext.hpp"
#include "star_windowing/factory/GuiFactory.hpp"

#include <starlight/core/Exceptions.hpp>

namespace star::windowing::maker
{
MakeGui::MakeGui(star::core::device::StarDevice &device,
                 star::core::device::manager::GraphicsContainer &graphicsManagers, star::common::EventBus &eventBus,
                 WindowingContext &winContext)
    : m_device(device), m_graphicsManagers(graphicsManagers), m_eventBus(eventBus), m_winContext(winContext)
{
}

MakeGui &MakeGui::setSwapchain(vk::SwapchainKHR swapchain)
{
    m_swapchain = swapchain;
    return *this;
}

MakeGui &MakeGui::setFinalizationPhase(const star::core::renderer::RenderPhase *finalizationPhase)
{
    m_finalizationPhase = finalizationPhase;
    return *this;
}

MakeGui &MakeGui::setConfig(const GuiDefinition &config)
{
    m_config = config;
    return *this;
}

void MakeGui::make()
{
    if (m_swapchain == VK_NULL_HANDLE)
        STAR_THROW("Gui swapchain must be set before make() is called");

    // The ini path and the backend-ready flag live in the context: ImGui keeps the ini pointer for its context lifetime
    factory::MakeGui(m_device, m_graphicsManagers, m_eventBus, m_winContext, m_swapchain, m_finalizationPhase, m_config,
                     m_winContext.imGuiIniFile, m_winContext.imGuiVulkanInitialized);
}
} // namespace star::windowing::maker
