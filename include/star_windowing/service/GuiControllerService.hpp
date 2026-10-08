#pragma once

#include <star_common/Handle.hpp>
#include <star_windowing/GuiDefinition.hpp>
#include <star_windowing/IGuiCaptureState.hpp>
#include <star_windowing/command/CreateGui.hpp>
#include <star_windowing/ui/ImGuiRenderPhaseProvider.hpp>

#include <starlight/policy/ListenForRegisterMainGraphicsRendererPolicy.hpp>
#include <starlight/policy/command/ListenFor.hpp>
#include <starlight/service/InitParameters.hpp>

#include <functional>

namespace star
{
class StarScene;
}
namespace star::common
{
class EventBus;
}
namespace star::core
{
class CommandBus;
}

namespace star::windowing
{
class WindowingContext;
class ImGuiRenderPhase;

template <typename T>
using ListenForCreateGui =
    star::policy::command::ListenFor<T, star::windowing::CreateGui, star::windowing::create_gui::GetTypeName,
                                     &T::onCreateGui>;

/// @brief Owns the windowed ImGui overlay lifecycle and the single authoritative
/// input-capture decision.
///
/// The service is created for presenting configurations and:
///  * listens for command::CreateGui from the application (scene + draw lambda),
///  * waits for event::RegisterMainGraphicsRenderer to learn which phase it
///    should overlay,
///  * creates and queues the ImGuiRenderPhaseProvider during prepRender, and
///  * owns the WantCaptureKeyboard/WantCaptureMouse state which InteractivityBus
///    consults before forwarding input events to star.
class GuiControllerService : public IGuiCaptureState,
                             private star::policy::ListenForRegisterMainGraphicsRenderPolicy<GuiControllerService>
{
  public:
    explicit GuiControllerService(WindowingContext &winContext);
    GuiControllerService(const GuiControllerService &) = delete;
    GuiControllerService &operator=(const GuiControllerService &) = delete;
    GuiControllerService(GuiControllerService &&);
    GuiControllerService &operator=(GuiControllerService &&);
    ~GuiControllerService() = default;

    void setInitParameters(star::service::InitParameters &params);
    void init();
    void negotiateWorkers(star::core::WorkerPool &pool, star::job::TaskManager &tm)
    {
        (void)pool;
        (void)tm;
    }
    void shutdown();

    bool wantsKeyboardInput() const noexcept override;
    bool wantsMouseInput() const noexcept override;

    /// Application -> service: attach or update the overlay for the given scene.
    void onCreateGui(star::windowing::CreateGui &cmd);

    /// Engine -> service: the main graphics renderer exists; build the overlay.
    void onRegisterMainGraphics(const star::event::RegisterMainGraphicsRenderer &event, bool &keepAlive);

  private:
    WindowingContext *m_winContext = nullptr;
    star::common::EventBus *m_eventBus = nullptr;
    star::core::CommandBus *m_cmdBus = nullptr;
    star::StarScene *m_scene = nullptr;

    GuiDefinition m_config{};
    std::function<void()> m_draw;
    Handle m_guiPhaseHandle;
    ImGuiRenderPhase *m_renderPhase = nullptr;

    bool m_wantsKeyboard = false;
    bool m_wantsMouse = false;

    ListenForCreateGui<GuiControllerService> m_onCreateGui;

    void initListeners();
    void cleanupListeners();

    /// Resolve the built overlay phase through the scene, caching the pointer
    /// so later CreateGui commands can update its callbacks in place.
    ImGuiRenderPhase *resolveRenderPhase();
};
} // namespace star::windowing
