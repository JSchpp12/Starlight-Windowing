#include "star_windowing/event/KeyPress.hpp"

#include <star_common/HandleTypeRegistry.hpp>

namespace star::windowing::event
{
KeyPress::KeyPress(int key, int scancode, int mods)
    : common::IEvent(common::HandleTypeRegistry::instance().registerType(GetKeyPressedEventTypeName)), m_key(key),
      m_scancode(scancode), m_mods(mods)
{
}
} // namespace star::windowing::event