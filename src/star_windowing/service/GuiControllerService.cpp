#include "star_windowing/service/GuiControllerService.hpp"

#include "star_windowing/InteractivityBus.hpp"
#include "star_windowing/WindowingContext.hpp"
#include "star_windowing/event/RequestSwapChainFromService.hpp"
#include "star_windowing/ui/GuiFactory.hpp"
#include "star_windowing/ui/ImGuiRenderPhase.hpp"

#include <starlight/core/Exceptions.hpp>
#include <starlight/event/RegisterMainGraphicsRenderer.hpp>
#include <starlight/virtual/StarScene.hpp>

#include <vulkan/vulkan.hpp>

namespace star::windowing
{
GuiControllerService::GuiControllerService(WindowingContext &winContext)
    : ListenForRegisterMainGraphicsRenderPolicy<GuiControllerService>(*this), m_winContext(&winContext),
      m_onCreateGui(*this)
{
}

GuiControllerService::GuiControllerService(GuiControllerService &&other)
    : IGuiCaptureState(std::move(other)), ListenForRegisterMainGraphicsRenderPolicy<GuiControllerService>(*this),
      m_draw(std::move(other.m_draw)), m_config(std::move(other.m_config)),
      m_imguiIniFile(std::move(other.m_imguiIniFile)), m_winContext(other.m_winContext), m_eventBus(other.m_eventBus),
      m_cmdBus(other.m_cmdBus), m_scene(other.m_scene), m_device(other.m_device),
      m_graphicsManagers(other.m_graphicsManagers), m_renderPhase(other.m_renderPhase),
      m_imguiDescriptorPool(other.m_imguiDescriptorPool), m_guiPhaseHandle(std::move(other.m_guiPhaseHandle)),
      m_onCreateGui(*this), m_wantsKeyboard(other.m_wantsKeyboard), m_wantsMouse(other.m_wantsMouse),
      m_imguiContextInitialized(other.m_imguiContextInitialized),
      m_imguiVulkanInitialized(other.m_imguiVulkanInitialized)
{
    if (m_eventBus != nullptr)
    {
        other.cleanupListeners();
        initListeners();
    }
}

GuiControllerService &GuiControllerService::operator=(GuiControllerService &&other)
{
    if (this != &other)
    {
        m_draw = std::move(other.m_draw);
        m_config = std::move(other.m_config);
        m_imguiIniFile = std::move(other.m_imguiIniFile);
        m_winContext = other.m_winContext;
        m_eventBus = other.m_eventBus;
        m_cmdBus = other.m_cmdBus;
        m_scene = other.m_scene;
        m_device = other.m_device;
        m_graphicsManagers = other.m_graphicsManagers;
        m_renderPhase = other.m_renderPhase;
        m_imguiDescriptorPool = other.m_imguiDescriptorPool;
        m_guiPhaseHandle = std::move(other.m_guiPhaseHandle);
        m_wantsKeyboard = other.m_wantsKeyboard;
        m_wantsMouse = other.m_wantsMouse;
        m_imguiContextInitialized = other.m_imguiContextInitialized;
        m_imguiVulkanInitialized = other.m_imguiVulkanInitialized;

        if (m_eventBus != nullptr)
        {
            other.cleanupListeners();
            initListeners();
        }
    }

    return *this;
}

void GuiControllerService::initListeners()
{
    if (m_cmdBus != nullptr)
        m_onCreateGui.init(*m_cmdBus);

    if (m_eventBus != nullptr)
        ListenForRegisterMainGraphicsRenderPolicy<GuiControllerService>::init(*m_eventBus);
}

void GuiControllerService::cleanupListeners()
{
    if (m_cmdBus != nullptr)
        m_onCreateGui.cleanup(*m_cmdBus);

    if (m_eventBus != nullptr)
        ListenForRegisterMainGraphicsRenderPolicy<GuiControllerService>::cleanup(*m_eventBus);
}

void GuiControllerService::setInitParameters(star::service::InitParameters &params)
{
    m_eventBus = &params.eventBus;
    m_cmdBus = &params.commandBus;
    m_device = &params.device;
    m_graphicsManagers = &params.graphicsManagers;
}

void GuiControllerService::init()
{
    initListeners();

    // Become the single capture-state authority consulted by the GLFW callbacks.
    InteractivityBus::SetCaptureStateProvider(this);
}

void GuiControllerService::shutdown()
{
    InteractivityBus::SetCaptureStateProvider(nullptr);
    cleanupListeners();

    if (m_imguiContextInitialized && m_device != nullptr)
    {
        GuiFactory::Shutdown(*m_device, m_imguiDescriptorPool, m_imguiVulkanInitialized);
        m_imguiContextInitialized = false;
    }
}

bool GuiControllerService::wantsKeyboardInput() const noexcept
{
    return m_wantsKeyboard;
}

bool GuiControllerService::wantsMouseInput() const noexcept
{
    return m_wantsMouse;
}

void GuiControllerService::onCreateGui(star::windowing::CreateGui &cmd)
{
    m_scene = &cmd.scene;
    m_draw = std::move(cmd.draw);

    // The overlay may already be built (a later CreateGui replaces the UI); in
    // that case update the existing phase's callbacks in place.
    if (auto *phase = resolveRenderPhase(); phase != nullptr)
    {
        phase->clearFrameCallbacks();
        if (m_draw)
            phase->addFrameCallback(m_draw);
    }
}

void GuiControllerService::onRegisterMainGraphics(const star::event::RegisterMainGraphicsRenderer &event,
                                                  bool &keepAlive)
{
    keepAlive = true;

    // Overlay attaches to the first registered main renderer, and only once.
    if (m_scene == nullptr || m_guiPhaseHandle.isInitialized())
        return;

    vk::SwapchainKHR swapchain{VK_NULL_HANDLE};
    m_eventBus->emit(star::windowing::event::RequestSwapChainFromService{swapchain});
    if (swapchain == VK_NULL_HANDLE)
        STAR_THROW("GuiControllerService failed to acquire the swapchain for the ImGui overlay");

    // Stand up the global ImGui context/backends. Done here (rather than in
    // init()) because it must run after InteractivityBus::Init so ImGui chains
    // the star GLFW callbacks instead of overwriting them.
    GuiFactory::Initialize(*m_device, *m_graphicsManagers, *m_eventBus, *m_winContext, swapchain, event.getRenderer(),
                           m_config, m_imguiIniFile, m_imguiDescriptorPool, m_imguiVulkanInitialized);
    m_imguiContextInitialized = true;

    auto provider = std::make_unique<ImGuiRenderPhaseProvider>(m_winContext, swapchain, event.getRenderer(), m_config);

    // Route ImGui's capture flags back so the service remains the one owner of
    // the capture decision.
    provider->setCaptureStateCallback([this](bool keyboard, bool mouse) noexcept {
        m_wantsKeyboard = keyboard;
        m_wantsMouse = mouse;
    });

    if (m_draw)
        provider->addFrameCallback(m_draw);

    // Queueing during prepRender is supported: the scene pops providers FIFO, so
    // this one is built right after the finalization phase it overlays.
    m_guiPhaseHandle = m_scene->addProvider(std::move(provider));
}

ImGuiRenderPhase *GuiControllerService::resolveRenderPhase()
{
    if (m_renderPhase == nullptr && m_scene != nullptr && m_guiPhaseHandle.isInitialized())
        m_renderPhase = static_cast<ImGuiRenderPhase *>(m_scene->getPhase(m_guiPhaseHandle));

    return m_renderPhase;
}
} // namespace star::windowing
