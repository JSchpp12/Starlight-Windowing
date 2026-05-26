#pragma once

#include <star_common/IEvent.hpp>
#include <string_view>

namespace star::windowing::event
{
namespace key_release
{
constexpr const char *GetUniqueTypeName()
{
    return "WinEvtKRel";
};
}

class KeyRelease : public common::IEvent
{
  public:
    static constexpr std::string_view GetUniqueTypeName()
    {
        return key_release::GetUniqueTypeName();
    }

    KeyRelease(int key, int scancode, int mods);

    int &getKey()
    {
        return m_key;
    }
    const int &getKey() const
    {
        return m_key;
    }
    int &getScancode()
    {
        return m_scancode;
    }
    const int &getScancode() const
    {
        return m_scancode;
    }
    int &getMods()
    {
        return m_mods;
    }
    const int &getMods() const
    {
        return m_mods;
    }

  private:
    int m_key;
    int m_scancode;
    int m_mods;
};
} // namespace star::windowing::event