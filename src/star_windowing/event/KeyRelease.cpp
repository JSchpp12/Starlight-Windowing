#include "star_windowing/event/KeyRelease.hpp"

#include <star_common/HandleTypeRegistry.hpp>

namespace star::windowing::event
{
KeyRelease::KeyRelease(int key, int scancode, int mods)
    : common::IEvent(common::HandleTypeRegistry::instance().registerType(GetKeyReleaseEventTypeName)),
      m_key(std::move(key)), m_scancode(std::move(scancode)), m_mods(std::move(mods))
{
}
} // namespace star::windowing::event