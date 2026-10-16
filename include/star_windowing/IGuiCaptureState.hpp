#pragma once

namespace star::windowing
{
/// @brief Single authoritative source for "does the GUI currently own the keyboard / mouse?".
class IGuiCaptureState
{
  public:
    virtual bool wantsKeyboardInput() const noexcept = 0;
    virtual bool wantsMouseInput() const noexcept = 0;
};
} // namespace star::windowing
