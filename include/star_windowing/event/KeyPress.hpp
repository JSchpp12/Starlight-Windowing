#pragma once

#include <star_common/IEvent.hpp>

namespace star::windowing::event
{
constexpr std::string_view GetKeyPressedEventTypeName = "star::windowing::event::keypress";
class KeyPress : public common::IEvent
{
    KeyPress() = default;
};
} // namespace star::windowing::event