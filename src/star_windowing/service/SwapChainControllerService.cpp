#include "star_windowing/service/SwapChainControllerService.hpp"

#include <star_common/HandleTypeRegistry.hpp>
#include <starlight/event/PrepForNextFrame.hpp>

#include <cassert>
#include <functional>

namespace star::windowing
{

SwapChainControllerService::SwapChainControllerService(SwapChainControllerService &&other)
    : ListenForRequestForSwapChainPolicy<SwapChainControllerService>{*this},
      star::policy::ListenForPrepForNextFramePolicy<SwapChainControllerService>{*this}, m_swapChain{std::move(other.m_swapChain)},
      m_listenerHandle{}, m_winContext{std::move(other.m_winContext)},
      m_deviceEventBus{std::move(other.m_deviceEventBus)}, m_device{std::move(other.m_device)}
{
    if (m_deviceEventBus != nullptr)
    {
        other.cleanup(*m_deviceEventBus);
        initListeners(*m_deviceEventBus);
    }
}

SwapChainControllerService &SwapChainControllerService::operator=(SwapChainControllerService &&other)
{
    if (this != &other)
    {
        m_swapChain = std::move(other.m_swapChain);
        m_winContext = std::move(other.m_winContext);
        m_deviceEventBus = std::move(other.m_deviceEventBus);
        m_deviceFrameTracker = std::move(other.m_deviceFrameTracker);
        m_device = std::move(other.m_device);

        if (m_deviceEventBus != nullptr)
        {
            other.cleanup(*m_deviceEventBus);
            initListeners(*m_deviceEventBus);
        }
    }

    return *this;
}

void SwapChainControllerService::cleanup(common::EventBus &eventBus)
{
    ListenForRequestForSwapChainPolicy<SwapChainControllerService>::cleanup(eventBus);
    star::policy::ListenForPrepForNextFramePolicy<SwapChainControllerService>::cleanup(eventBus);
}

void SwapChainControllerService::setInitParameters(star::service::InitParameters &params)
{
    m_deviceEventBus = &params.eventBus;
    m_device = &params.device;
    m_deviceFrameTracker = &params.flightTracker;
}

void SwapChainControllerService::init(const uint8_t &numFramesInFlight)
{
    assert(m_deviceEventBus != nullptr && m_deviceFrameTracker != nullptr);

    m_swapChain = SwapChain(m_winContext);
    m_swapChain.prepRender(*m_device, *m_deviceEventBus, *m_deviceFrameTracker);
    initListeners(*m_deviceEventBus);
}

void SwapChainControllerService::initListeners(common::EventBus &eventBus)
{
    ListenForRequestForSwapChainPolicy<SwapChainControllerService>::init(eventBus);
    star::policy::ListenForPrepForNextFramePolicy<SwapChainControllerService>::init(eventBus);
}

void SwapChainControllerService::shutdown()
{
    // delete the swapchain
    assert(m_device != nullptr);

    m_swapChain.cleanupRender(*m_device);
}

void SwapChainControllerService::prepForNextFrame(common::FrameTracker *frameTracker)
{
    assert(m_device != nullptr && m_deviceFrameTracker != nullptr);
    // increment frame in flight index before handling next render to target image
    frameTracker->getCurrent().setFrameInFlightIndex(incrementNextFrameInFlight(*frameTracker));
    frameTracker->triggerIncrementForCurrentFrame();
    frameTracker->getCurrent().setFinalTargetImageIndex(incrementNextSwapChainImage(*frameTracker));
}

uint8_t SwapChainControllerService::incrementNextSwapChainImage(const common::FrameTracker &frameTracker)
{
    const auto aResult = m_swapChain.acquireNextSwapChainImage(*m_device, frameTracker);

    if (aResult.result == vk::Result::eErrorOutOfDateKHR)
    {
        throw std::runtime_error("Swapchain is out of date and support for recreation is not implemented");
    }
    else if (aResult.result != vk::Result::eSuccess && aResult.result != vk::Result::eSuboptimalKHR)
    {
        throw std::runtime_error("Failed to acquire swapchain image due to suboptimal khr layout");
    }

    return static_cast<uint8_t>(aResult.value);
}

uint8_t SwapChainControllerService::incrementNextFrameInFlight(const common::FrameTracker &frameTracker) const noexcept
{
    const uint8_t &max = frameTracker.getSetup().getNumFramesInFlight();
    const uint8_t &current = frameTracker.getCurrent().getFrameInFlightIndex();

    return (current + 1) % max;
}

} // namespace star::windowing