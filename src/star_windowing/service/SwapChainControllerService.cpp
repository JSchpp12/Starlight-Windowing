#include "star_windowing/service/SwapChainControllerService.hpp"

#include <star_common/HandleTypeRegistry.hpp>
#include <starlight/command/frames/GetFrameTracker.hpp>
#include <starlight/core/Exceptions.hpp>

#include <cassert>

namespace star::windowing
{

SwapChainControllerService::SwapChainControllerService(SwapChainControllerService &&other)
    : ListenForRequestForSwapChainPolicy<SwapChainControllerService>{*this},
      m_frameTracker(std::move(other.m_frameTracker)), m_swapChain{std::move(other.m_swapChain)}, m_listenerHandle{},
      m_listenGetFrameTracker{*this}, m_listenFrameComplete{*this},
      m_listenPrepNextFrame{*this} ,m_winContext{std::move(other.m_winContext)},
      m_eventBus{std::move(other.m_eventBus)}, m_cmdBus{other.m_cmdBus}, m_device{std::move(other.m_device)}
{
    if (m_eventBus != nullptr)
    {
        other.cleanup(*m_eventBus, *m_cmdBus);
        initListeners(*m_eventBus);
        initListeners(*m_cmdBus);
    }
}

SwapChainControllerService &SwapChainControllerService::operator=(SwapChainControllerService &&other)
{
    if (this != &other)
    {
        m_frameTracker = std::move(other.m_frameTracker);
        m_swapChain = std::move(other.m_swapChain);
        m_winContext = std::move(other.m_winContext);
        m_eventBus = std::move(other.m_eventBus);
        m_cmdBus = other.m_cmdBus;
        m_device = std::move(other.m_device);

        if (m_eventBus != nullptr)
        {
            other.cleanup(*m_eventBus, *m_cmdBus);
            initListeners(*m_eventBus);
            initListeners(*m_cmdBus);
        }
    }

    return *this;
}

void SwapChainControllerService::cleanup(common::EventBus &eventBus, core::CommandBus &cmdBus)
{
    cleanupListeners(eventBus);
    cleanupListeners(cmdBus);
}

void SwapChainControllerService::setInitParameters(star::service::InitParameters &params)
{
    m_cmdBus = &params.commandBus;
    m_eventBus = &params.eventBus;
    m_device = &params.device;

    m_frameTracker = star::common::FrameTracker(params.flightTrackerSetup);
}

void SwapChainControllerService::onStartOfNextFrame(const star::event::StartOfNextFrame &event, bool &keepAlive)
{
    m_frameTracker.getCurrent().setFinalTargetImageIndex(incrementNextSwapChainImage(m_frameTracker));

    keepAlive = true;
}

void SwapChainControllerService::init()
{
    assert(m_eventBus != nullptr);

    m_swapChain = SwapChain(m_winContext);
    m_swapChain.prepRender(*m_device, *m_eventBus, m_frameTracker);
    initListeners(*m_eventBus);
    initListeners(*m_cmdBus);
}

void SwapChainControllerService::initListeners(common::EventBus &eventBus)
{
    ListenForRequestForSwapChainPolicy<SwapChainControllerService>::init(eventBus);
    m_listenFrameComplete.init(eventBus);
    m_listenPrepNextFrame.init(eventBus);
}

void SwapChainControllerService::initListeners(star::core::CommandBus &cmdBus)
{
    m_listenGetFrameTracker.init(cmdBus);
}

void SwapChainControllerService::cleanupListeners(common::EventBus &eventBus)
{
    ListenForRequestForSwapChainPolicy<SwapChainControllerService>::cleanup(eventBus);
    m_listenFrameComplete.cleanup(eventBus);
    m_listenPrepNextFrame.cleanup(eventBus);
}

void SwapChainControllerService::cleanupListeners(star::core::CommandBus &cmdBus)
{
    m_listenGetFrameTracker.cleanup(cmdBus);
}

void SwapChainControllerService::onGetFrameTracker(star::frames::GetFrameTracker &cmd)
{
    cmd.getReply().set(&m_frameTracker);
}

void SwapChainControllerService::shutdown()
{
    // delete the swapchain
    assert(m_device != nullptr);

    m_swapChain.cleanupRender(*m_device);
}

void SwapChainControllerService::onFrameComplete(const star::event::FrameComplete &event, bool &keepAlive)
{
    m_frameTracker.triggerIncrementForCurrentFrame();

    m_frameTracker.getCurrent().setFrameInFlightIndex(incrementNextFrameInFlight(m_frameTracker));

    keepAlive = true;
}

uint8_t SwapChainControllerService::incrementNextSwapChainImage(const common::FrameTracker &frameTracker)
{
    const auto aResult = m_swapChain.acquireNextSwapChainImage(*m_device, frameTracker);

    if (aResult.result == vk::Result::eErrorOutOfDateKHR)
    {
        STAR_THROW("Swapchain is out of date and support for recreation is not implemented");
    }
    else if (aResult.result != vk::Result::eSuccess && aResult.result != vk::Result::eSuboptimalKHR)
    {
        STAR_THROW("Failed to acquire swapchain image due to suboptimal khr layout");
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