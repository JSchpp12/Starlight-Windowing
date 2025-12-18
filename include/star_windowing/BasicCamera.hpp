#pragma once

#include "StarCamera.hpp"
#include "Time.hpp"

#include <star_common/EventBus.hpp>
#include <star_windowing/policy/HandleMouseMovementPolicy.hpp>

#include <glm/glm.hpp>
#include <iostream>

namespace star::windowing
{
/// <summary>
/// Camera with default controls
/// </summary>
class BasicCamera : public StarCamera, private HandleMouseMovementPolicy<BasicCamera>
{
  public:
    BasicCamera() : HandleMouseMovementPolicy<BasicCamera>(*this)
    {
    }
    BasicCamera(const uint32_t &width, const uint32_t &height);
    BasicCamera(const uint32_t &width, const uint32_t &height, const float &horizontalFieldOfView,
                const float &nearClippingPlaneDistance, const float &farClippingPlaneDistance,
                const float &movementSpeed, const float &sensitivity);

    virtual ~BasicCamera();
    void init(common::EventBus &eventBus);

    virtual void frameUpdate(core::device::DeviceContext &context, const uint8_t &frameInFlightIndex) override;
    // /// <summary>
    // /// Key callback for camera object. Implements default controls for the camera.
    // /// </summary>
    // /// <param name="key"></param>
    // /// <param name="scancode"></param>
    // /// <param name="action"></param>
    // /// <param name="mods"></param>
    // virtual void onKeyPress(int key, int scancode, int mods) override;

    // virtual void onKeyRelease(int key, int scancode, int mods) override;

    // /// <summary>
    // /// Mouse callback for camera objects. Implements default controls for the camera.
    // /// </summary>
    // /// <param name="xpos"></param>
    // /// <param name="ypos"></param>
    void onMouseMovement(double xpos, double ypos);

    // /// <summary>
    // /// Mouse button callback for camera object.
    // /// </summary>
    // /// <param name="button"></param>
    // /// <param name="action"></param>
    // /// <param name="mods"></param>
    // virtual void onMouseButtonAction(int button, int action, int mods) override;

    // /// <summary>
    // /// Update camera locations as needed
    // /// </summary>
    // virtual void onWorldUpdate(const uint32_t &frameInFlightIndex) override;

    // void onScroll(double xoffset, double yoffset) override {};

    float getPitch() const
    {
        return this->pitch;
    }

    float getYaw() const
    {
        return this->yaw;
    }

    void setSensitivity(const float &newSensitivity)
    {
        sensitivity = newSensitivity;
    }

    void setMovementSpeed(const float &newSpeed)
    {
        movementSpeed = newSpeed;
    }

  private:
    Time time = Time();

    float movementSpeed = 1000.0f;
    float sensitivity = 0.1f;
    bool m_init = false;

    // previous mouse coordinates from GLFW
    float prevX, prevY, xMovement, yMovement;

    // control information for camera
    float pitch = -0.f, yaw = -90.0f;

    bool click = false;

    Handle m_mouseMovementCallback;

    void onMouseMovement();
};
} // namespace star::windowing