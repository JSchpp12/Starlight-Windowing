#pragma once

#include <star_common/Handle.hpp>
#include <star_windowing/GuiDefinition.hpp>
#include <star_windowing/IGuiCaptureState.hpp>
#include <star_windowing/command/CreateGui.hpp>

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
namespace star::core::device
{
class StarDevice;
class DeviceContext;
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
class ImGuiRenderPhase;

template <typename T>
using ListenForCreateGui = star::policy::command::ListenFor<T, star::windowing::CreateGui,
                                                            star::windowing::create_gui::GetTypeName, &T::onCreateGui>;

/// @brief Owns the windowed ImGui overlay lifecycle and the single authoritative input-capture decision.
class GuiControllerService : public IGuiCaptureState
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

  private:
    // Members grouped by size, largest first, so the layout stays compact and
    // predictable instead of interleaving pointers with value types.

    // Large value members.
    std::function<void()> m_draw;
    GuiDefinition m_config{};

    // Pointer / handle-sized members.
    WindowingContext *m_winContext = nullptr;
    star::common::EventBus *m_eventBus = nullptr;
    star::core::CommandBus *m_cmdBus = nullptr;
    star::StarScene *m_scene = nullptr;
    star::core::device::StarDevice *m_device = nullptr;
    star::core::device::DeviceContext *m_context = nullptr;
    star::core::device::manager::GraphicsContainer *m_graphicsManagers = nullptr;
    ImGuiRenderPhase *m_renderPhase = nullptr;
    Handle m_guiPhaseHandle;
    ListenForCreateGui<GuiControllerService> m_onCreateGui;

    // Byte-sized flags.
    bool m_wantsKeyboard = false;
    bool m_wantsMouse = false;
    bool m_imguiContextInitialized = false;

    void initListeners();
    void cleanupListeners();

    /// Resolve the built overlay phase through the scene, caching the pointer
    /// so later CreateGui commands can update its callbacks in place.
    ImGuiRenderPhase *resolveRenderPhase();
};
} // namespace star::windowing
