#pragma once

#include "StarCamera.hpp"
#include "Time.hpp"

#include <star_common/EventBus.hpp>
#include <star_windowing/policy/HandleKeyPressPolicy.hpp>
#include <star_windowing/policy/HandleKeyReleasePolicy.hpp>
#include <star_windowing/policy/HandleMouseButtonPolicy.hpp>
#include <star_windowing/policy/HandleMouseMovementPolicy.hpp>

#include <glm/glm.hpp>
#include <iostream>

namespace star::windowing
{
/// <summary>
/// Camera with default controls
/// </summary>
class BasicCamera : public StarCamera,
                    private HandleMouseMovementPolicy<BasicCamera>,
                    private HandleMouseButtonPolicy<BasicCamera>,
                    private HandleKeyPressPolicy<BasicCamera>,
                    private HandleKeyReleasePolicy<BasicCamera>
{
  public:
    class Builder
    {
        float m_horzFov{0.0f};
        float m_nearClippingPlane{0.0f};
        float m_farClippingPlane{0.0f};
        float m_movementSpeed{0.0f};
        float m_sensitivity{0.0f};
        uint32_t m_width{0};
        uint32_t m_height{0};

      public:
        Builder() = default;
        Builder &setWidth(uint32_t width)
        {
            m_width = std::move(width);
            return *this;
        }
        Builder &setHeight(uint32_t height)
        {
            m_height = std::move(height);
            return *this;
        }
        Builder &setHorizontalFieldOfView(float value)
        {
            m_horzFov = std::move(value);
            return *this;
        }
        Builder &setNearClippingPlaneDistance(float value)
        {
            m_nearClippingPlane = std::move(value);
            return *this;
        }
        Builder &setFarClippingPlaneDistance(float value)
        {
            m_farClippingPlane = std::move(value);
            return *this;
        }
        Builder &setMovementSpeed(float value)
        {
            m_movementSpeed = std::move(value);
            return *this;
        }
        Builder &setSensitivity(float value)
        {
            m_sensitivity = std::move(value);
            return *this;
        }
        std::unique_ptr<star::StarCamera> buildUnique()
        {
            if (m_horzFov != 0.0f)
            {
                return std::make_unique<BasicCamera>(m_width, m_height, m_horzFov, m_nearClippingPlane,
                                                     m_farClippingPlane, m_movementSpeed, m_sensitivity);
            }
            else
            {
                return std::make_unique<BasicCamera>(m_width, m_height);
            }
        }
        std::shared_ptr<BasicCamera> buildShared()
        {
            if (m_horzFov != 0.0f)
            {
                return std::make_shared<BasicCamera>(m_width, m_height, m_horzFov, m_nearClippingPlane,
                                                     m_farClippingPlane, m_movementSpeed, m_sensitivity);
            }
            else
            {
                return std::make_shared<BasicCamera>(m_width, m_height);
            }
        }
    };
    BasicCamera();
    BasicCamera(const uint32_t &width, const uint32_t &height);
    BasicCamera(const uint32_t &width, const uint32_t &height, const float &horizontalFieldOfView,
                const float &nearClippingPlaneDistance, const float &farClippingPlaneDistance,
                const float &movementSpeed, const float &sensitivity);

    virtual ~BasicCamera();
    void init(common::EventBus &eventBus);

    virtual void frameUpdate(core::device::DeviceContext &context, const uint8_t &frameInFlightIndex) override;

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

  protected:
    void onKeyRelease(const int &key, const int &scancode, const int &mods);

    // /// <summary>
    // /// Key callback for camera object. Implements default controls for the camera.
    // /// </summary>
    // /// <param name="key"></param>
    // /// <param name="scancode"></param>
    // /// <param name="action"></param>
    // /// <param name="mods"></param>
    void onKeyPress(const int &key, const int &scancode, const int &mods);

    // /// <summary>
    // /// Mouse callback for camera objects. Implements default controls for the camera.
    // /// </summary>
    // /// <param name="xpos"></param>
    // /// <param name="ypos"></param>
    void onMouseMovement(const double &xpos, const double &ypos);

    // /// <summary>
    // /// Mouse button callback for camera object.
    // /// </summary>
    // /// <param name="button"></param>
    // /// <param name="action"></param>
    // /// <param name="mods"></param>
    void onMouseButtonAction(const int &button, const int &action, const int &mods);

  private:
    friend class HandleMouseMovementPolicy<BasicCamera>;
    friend class HandleMouseButtonPolicy<BasicCamera>;
    friend class HandleKeyPressPolicy<BasicCamera>;
    friend class HandleKeyReleasePolicy<BasicCamera>;
    Time time = Time();

    float movementSpeed = 1000.0f;
    float sensitivity = 0.1f;
    // previous mouse coordinates from GLFW
    float prevX, prevY, xMovement, yMovement;
    // control information for camera
    float pitch = -0.f, yaw = -90.0f;
    bool moveLeft = false, moveRight = false, moveForward = false, moveBack = false;
    bool m_init = false;
    bool click = false;
};
} // namespace star::windowing