#pragma once

#include <starlight/core/device/DeviceContext.hpp>
#include <starlight/core/device/managers/ManagerCommandBuffer.hpp>
#include <starlight/wrappers/graphics/StarCommandBuffer.hpp>

namespace star::windowing
{
class PresentationCommands
{
  public:
    struct RecordDependencies
    {
        vk::Semaphore mainGraphicsDoneSemaphore;
        uint32_t acquiredSwapChainImageIndex;
    };
    PresentationCommands() = default;

    PresentationCommands(const PresentationCommands &) = delete;
    PresentationCommands &operator=(const PresentationCommands &) = delete;
    PresentationCommands(PresentationCommands &&other)
        : m_listener(std::move(other.m_listener)), m_recordDeps(other.m_recordDeps), m_swapchain(other.m_swapchain),
          m_deviceEventBus(std::move(other.m_deviceEventBus))
    {
        if (m_deviceEventBus != nullptr)
        {
            registerListener(*m_deviceEventBus);
        }
    }
    PresentationCommands &operator=(PresentationCommands &&other)
    {
        if (this != &other)
        {
            m_listener = std::move(other.m_listener);
            m_recordDeps = other.m_recordDeps;
            m_swapchain = other.m_swapchain;
            m_deviceEventBus = other.m_deviceEventBus;

            if (m_deviceEventBus != nullptr)
            {
                registerListener(*m_deviceEventBus);
            }
        }

        return *this;
    }
    ~PresentationCommands() = default;

    void init(RecordDependencies *recordDeps, vk::SwapchainKHR *swapchain);

    void prepRender(core::device::DeviceContext &context);

  private:
    Handle m_listener;
    RecordDependencies *m_recordDeps = nullptr;
    vk::SwapchainKHR *m_swapchain = nullptr;
    common::EventBus *m_deviceEventBus = nullptr;

    void eventCallback(const star::common::IEvent &e, bool &keepAlive);

    Handle *getHandleForUpdate(){
      return &m_listener;
    }

    void notificationFromEventBusHandleDelete(const Handle &noLongerNeededSubscriberHandle);

    void registerListener(common::EventBus &context);

    void submitPresentation(core::device::StarDevice &device);
};
} // namespace star::windowing