#pragma once

#include "star_windowing/event/MouseMovement.hpp"

#include <star_common/EventBus.hpp>
#include <star_common/Handle.hpp>
#include <star_common/HandleTypeRegistry.hpp>

namespace star::windowing
{
template <typename T> class HandleMouseMovementPolicy
{
  public:
    HandleMouseMovementPolicy(T &me) : me(me)
    {
    }

    void init(common::EventBus &eventBus)
    {
        registerListener(eventBus);
    }

  private:
    T &me;
    Handle m_callbackRegistration;

    void registerListener(common::EventBus &eventBus)
    {
        eventBus.subscribe(
            common::HandleTypeRegistry::instance().registerType(event::GetMouseMovementEventTypeName),
            common::SubscriberCallbackInfo{std::bind(&HandleMouseMovementPolicy<T>::eventCallback, this,
                                                     std::placeholders::_1, std::placeholders::_2),
                                           std::bind(&HandleMouseMovementPolicy<T>::getHandleForEventBus, this),
                                           std::bind(&HandleMouseMovementPolicy<T>::notificationFromEventBusOfDeletion,
                                                     this, std::placeholders::_1)});
    }

    void eventCallback(const common::IEvent &e, bool &keepAlive)
    {
        const auto &event = static_cast<const event::MouseMovement &>(e);

        me.onMouseMovement(event.getXPos(), event.getYPos());

        keepAlive = true;
    }

    Handle *getHandleForEventBus()
    {
        return &m_callbackRegistration;
    }

    void notificationFromEventBusOfDeletion(const Handle &noLongerNeededHandle)
    {
        if (m_callbackRegistration == noLongerNeededHandle)
        {
            m_callbackRegistration = Handle();
        }
    }
};
} // namespace star::windowing