#include "star_windowing/InteractivityBus.hpp"

#include <star_windowing/event/KeyPress.hpp>
#include <star_windowing/event/KeyRelease.hpp>
#include <star_windowing/event/MouseButton.hpp>
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
    glfwSetKeyCallback(winContext->window.getGLFWWindow(), InteractivityBus::GlfwKeyCallback);
    glfwSetMouseButtonCallback(winContext->window.getGLFWWindow(), InteractivityBus::GlfwCallbackMouseButton);
}

void InteractivityBus::GlfwCallbackMouseMovement(GLFWwindow *window, double xpos, double ypos)
{
    assert(m_deviceEventBus != nullptr);

    m_deviceEventBus->emit(event::MouseMovement{xpos, ypos});
}

void InteractivityBus::GlfwCallbackMouseButton(GLFWwindow *window, int button, int action, int mods)
{
    assert(m_deviceEventBus != nullptr);

    m_deviceEventBus->emit(event::MouseButton{button, action, mods});
}

void InteractivityBus::GlfwKeyCallback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    assert(m_deviceEventBus != nullptr);

    if (action == GLFW_PRESS)
    {
        m_deviceEventBus->emit(event::KeyPress{key, scancode, mods});
    }
    else if (action == GLFW_RELEASE)
    {
        m_deviceEventBus->emit(event::KeyRelease{key, scancode, mods});
    }
}
} // namespace star::windowing