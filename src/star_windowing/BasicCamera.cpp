#include "star_windowing/BasicCamera.hpp"

#include <star_common/HandleTypeRegistry.hpp>
#include <star_windowing/event/MouseMovement.hpp>

#include <GLFW/glfw3.h>

namespace star::windowing
{
BasicCamera::BasicCamera()
    : HandleMouseMovementPolicy<BasicCamera>(*this), HandleMouseButtonPolicy<BasicCamera>(*this),
      HandleKeyPressPolicy<BasicCamera>(*this), HandleKeyReleasePolicy<BasicCamera>(*this)
{
}

BasicCamera::BasicCamera(const uint32_t &width, const uint32_t &height)
    : star::StarCamera(width, height), HandleMouseMovementPolicy<BasicCamera>(*this),
      HandleMouseButtonPolicy<BasicCamera>(*this), HandleKeyPressPolicy<BasicCamera>(*this),
      HandleKeyReleasePolicy<BasicCamera>(*this)
{
}

BasicCamera::BasicCamera(const uint32_t &width, const uint32_t &height, const float &horizontalFieldOfView,
                         const float &nearClippingPlaneDistance, const float &farClippingPlaneDistance,
                         const float &movementSpeed, const float &sensitivity)
    : star::StarCamera(width, height, horizontalFieldOfView, nearClippingPlaneDistance, farClippingPlaneDistance),
      HandleMouseMovementPolicy<BasicCamera>(*this), HandleMouseButtonPolicy<BasicCamera>(*this),
      HandleKeyPressPolicy<BasicCamera>(*this), HandleKeyReleasePolicy<BasicCamera>(*this),
      movementSpeed(movementSpeed), sensitivity(sensitivity)
{
}

BasicCamera::~BasicCamera()
{
}

void BasicCamera::init(common::EventBus &eventBus)
{
    HandleMouseMovementPolicy<BasicCamera>::init(eventBus);
    HandleMouseButtonPolicy<BasicCamera>::init(eventBus);
    HandleKeyPressPolicy<BasicCamera>::init(eventBus);
    HandleKeyReleasePolicy<BasicCamera>::init(eventBus);
}

void BasicCamera::onMouseMovement(const double &xpos, const double &ypos)
{
    if (this->click)
    {
        // prime camera
        if (!m_init)
        {
            this->prevX = xpos;
            this->prevY = ypos;
            m_init = true;
        }

        this->xMovement = xpos - this->prevX;
        this->yMovement = ypos - this->prevY;
        this->prevX = xpos;
        this->prevY = ypos;
    }
}

void BasicCamera::frameUpdate(core::device::DeviceContext &context, const uint8_t &frameInFlightIndex)
{

    if ((double)time.timeElapsedLastFrameSeconds() > 1)
    {
        time.updateLastFrameTime();
    }

    if (moveLeft || moveRight || moveForward || moveBack)
    {
        float moveAmt = this->movementSpeed * time.timeElapsedLastFrameSeconds();

        glm::vec3 cameraLookDir = this->getForwardVector();

        if (moveLeft)
        {
            this->moveRelative(glm::cross(glm::vec3(this->getForwardVector()), -glm::vec3(this->getUpVector())),
                               moveAmt);
        }
        if (moveRight)
        {
            this->moveRelative(glm::cross(glm::vec3(this->getForwardVector()), glm::vec3(this->getUpVector())),
                               moveAmt);
        }
        if (moveForward)
        {
            this->moveRelative(this->getForwardVector(), moveAmt);
        }
        if (moveBack)
        {
            this->moveRelative(-this->getForwardVector(), moveAmt);
        }

        time.updateLastFrameTime();
    }

    if (m_init)
    {
        this->xMovement *= this->sensitivity;
        this->yMovement *= this->sensitivity;

        this->yaw += this->xMovement;
        this->pitch += this->yMovement;

        // apply restrictions due to const up vector for the camera
        if (this->pitch > 89.0f)
        {
            pitch = 89.0f;
        }
        if (this->pitch < -89.0f)
        {
            pitch = -89.0f;
        }

        glm::vec3 direction{cos(glm::radians(this->yaw)) * cos(glm::radians(this->pitch)),
                            sin(glm::radians(this->pitch)),
                            sin(glm::radians(this->yaw)) * cos(glm::radians(this->pitch))};

        this->setForwardVector(glm::vec4(glm::normalize(direction), 0.0));
    }
}

void BasicCamera::onMouseButtonAction(const int &button, const int &action, const int &mods)
{
    if (action == GLFW_PRESS && button == GLFW_MOUSE_BUTTON_LEFT)
    {
        this->click = true;
    }
    else if (this->click && button == GLFW_MOUSE_BUTTON_LEFT)
    {
        this->click = false;
        m_init = false;
    }
}

void BasicCamera::onKeyPress(const int &key, const int &scancode, const int &mods)
{
    // TODO: improve time var allocation
    // bool moveLeft = KeyStates::state(A);
    // bool moveRight = KeyStates::state(D);
    // bool moveForward = KeyStates::state(W);
    // bool moveBack = KeyStates::state(S);

    if (key == GLFW_KEY_A)
    {
        moveLeft = true;
    }
    if (key == GLFW_KEY_D)
    {
        moveRight = true;
    }
    if (key == GLFW_KEY_W)
    {
        moveForward = true;
    }
    if (key == GLFW_KEY_S)
    {
        moveBack = true;
    }
}

void BasicCamera::onKeyRelease(const int &key, const int &scancode, const int &mods)
{
    if (key == GLFW_KEY_A)
    {
        moveLeft = false;
    }
    else if (key == GLFW_KEY_D)
    {
        moveRight = false;
    }
    else if (key == GLFW_KEY_W)
    {
        moveForward = false;
    }
    else if (key == GLFW_KEY_S)
    {
        moveBack = false;
    }
}

} // namespace star::windowing