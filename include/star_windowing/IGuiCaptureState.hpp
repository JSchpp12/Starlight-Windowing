#pragma once

namespace star::windowing
{
/// @brief Single authoritative source for "does the GUI currently own the
/// keyboard / mouse?".
///
/// The ImGui overlay reports its capture state to the gui controller service,
/// which implements this interface. The GLFW callbacks in InteractivityBus
/// consult the registered provider before emitting star input events, so the
/// capture decision lives in exactly one place.
class IGuiCaptureState
{
  public:
    virtual ~IGuiCaptureState() = default;

    virtual bool wantsKeyboardInput() const noexcept = 0;
    virtual bool wantsMouseInput() const noexcept = 0;
};
} // namespace star::windowing
