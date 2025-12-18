#include "star_windowing/event/MouseMovement.hpp"

#include <star_common/HandleTypeRegistry.hpp>

namespace star::windowing::event
{
MouseMovement::MouseMovement(double xpos, double ypos)
    : common::IEvent(common::HandleTypeRegistry::instance().registerType(GetMouseMovementEventTypeName)),
      m_xpos(std::move(xpos)), m_ypos(std::move(ypos))
{
}
} // namespace star::windowing::event