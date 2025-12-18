#pragma once

#include <star_common/EventBus.hpp>
#include <star_windowing/WindowingContext.hpp>

#include <GLFW/glfw3.h>

namespace star::windowing
{
class InteractivityBus
{
  public:
    static void Init(star::common::EventBus *deviceEventBus, star::windowing::WindowingContext *); 

    static void GlfwCallbackMouseMovement(GLFWwindow *window, double xpos, double ypos); 

    // static void GlfwCallbackMouseButton(GLFWWindow *window)
  private:
    static star::common::EventBus *m_deviceEventBus;
};
} // namespace star::windowing