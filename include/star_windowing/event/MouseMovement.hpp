#pragma once

#include <star_common/IEvent.hpp>
#include <string_view>

namespace star::windowing::event
{
constexpr std::string_view GetMouseMovementEventTypeName = "star::windowing::MouseMovement";
class MouseMovement : public common::IEvent
{
  public:
    MouseMovement(double xpos, double ypos);
    virtual ~MouseMovement() = default;

    double &getXPos()
    {
        return m_xpos;
    }
    const double &getXPos() const
    {
        return m_xpos;
    }
    double &getYPos()
    {
        return m_ypos;
    }
    const double &getYPos() const
    {
        return m_ypos;
    }

  private:
    double m_xpos;
    double m_ypos;
};
} // namespace star::windowing::event