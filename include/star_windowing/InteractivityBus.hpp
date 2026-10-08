#pragma once

#include <star_common/EventBus.hpp>
#include <star_windowing/IGuiCaptureState.hpp>
#include <star_windowing/WindowingContext.hpp>

#include <GLFW/glfw3.h>

namespace star::windowing
{
class InteractivityBus
{
  public:
    static void Init(star::common::EventBus *deviceEventBus, star::windowing::WindowingContext *); 

    static void GlfwCallbackMouseMovement(GLFWwindow *window, double xpos, double ypos); 

    static void GlfwCallbackMouseButton(GLFWwindow *window, int button, int action, int mods); 

    static void GlfwKeyCallback(GLFWwindow *window, int key, int scancode, int action, int mods); 

    /// Install the single capture-state provider (the gui controller service).
    /// While it reports that the GUI owns keyboard/mouse focus, the GLFW
    /// callbacks below stop emitting star input events so UI interaction never
    /// drives the camera or app hotkeys. Passing nullptr disables gating.
    static void SetCaptureStateProvider(const IGuiCaptureState *provider) noexcept
    {
        m_captureState = provider;
    }

  private:
    static star::common::EventBus *m_deviceEventBus;
    static const IGuiCaptureState *m_captureState;
};
} // namespace star::windowing