#pragma once

#include <star_common/IEvent.hpp>
namespace star::windowing::event
{
constexpr std::string_view GetMouseButtonEventTypeName = "star::win::MouseButton";
class MouseButton : public common::IEvent
{
  public:
    MouseButton(int button, int action, int mods);
    virtual ~MouseButton() = default;

    int &getButton()
    {
        return m_button;
    }
    const int &getButton() const
    {
        return m_button;
    }
    int &getAction()
    {
        return m_action;
    }
    const int &getAction() const
    {
        return m_action;
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
    int m_button;
    int m_action;
    int m_mods;
};
} // namespace star::windowing::event