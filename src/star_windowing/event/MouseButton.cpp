#include "star_windowing/event/MouseButton.hpp"

#include <star_common/HandleTypeRegistry.hpp>
namespace star::windowing::event
{
MouseButton::MouseButton(int button, int action, int mods)
    : common::IEvent(common::HandleTypeRegistry::instance().registerType(GetMouseButtonEventTypeName)),
      m_button(std::move(button)), m_action(std::move(action)), m_mods(std::move(mods))
{
}
} // namespace star::windowing::event