#pragma once

#include "star_windowing/event/KeyRelease.hpp"

#include <concepts>

#include <starlight/policy/event/ListenFor.hpp>

namespace star::windowing
{
template <typename T>
concept ValidKeyReleaseHandler = requires(T listener, const event::KeyRelease &event, bool &keepAlive) {
    // Check the member exists via pointer — works before class is complete
    static_cast<void (T::*)(const event::KeyRelease &, bool &)>(&T::onKeyRelease);
};

template <typename T>
    requires ValidKeyReleaseHandler<T>
using ListenForKeyRelease =
    star::policy::event::ListenFor<T, star::windowing::event::KeyRelease,
                                   star::windowing::event::key_release::GetUniqueTypeName, &T::onKeyRelease>;
} // namespace star::windowing