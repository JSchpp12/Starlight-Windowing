#include "star_windowing/InteractivityBus.hpp"

#include <star_windowing/event/MouseMovement.hpp>

#include <cassert>

namespace star::windowing
{
common::EventBus *InteractivityBus::m_deviceEventBus = nullptr;

void InteractivityBus::Init(star::common::EventBus *deviceEventBus, WindowingContext *winContext)
{
    assert(deviceEventBus != nullptr && winContext != nullptr);

    m_deviceEventBus = deviceEventBus;

    glfwSetCursorPosCallback(winContext->window.getGLFWWindow(), InteractivityBus::GlfwCallbackMouseMovement);
}

void InteractivityBus::GlfwCallbackMouseMovement(GLFWwindow *window, double xpos, double ypos)
{
    assert(m_deviceEventBus != nullptr);

    m_deviceEventBus->emit(event::MouseMovement{xpos, ypos});
}

} // namespace star::windowing