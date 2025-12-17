#pragma once

#include "star_windowing/SwapChainRenderer.hpp"
#include "star_windowing/policy/WindowingContext.hpp"

#include <star_common/Handle.hpp>
#include <starlight/core/SystemContext.hpp>

namespace star::windowing
{
class EngineMainLoopPolicy
{
  public:
    explicit EngineMainLoopPolicy(WindowingContext &winContext)
        : m_winContext(winContext)
    {
    }

    void init(uint8_t maxNumFramesInFlight); 

    void frameUpdate();
    // swapChainRenderer->submitPresentation(frameInFlightIndex, &allBuffersSubmitted);
    uint8_t getFrameIndexToBeDrawn();

    bool shouldProgramExit() const
    {
        return false;
    }
    uint8_t &getMaxNumOfFramesInFlight()
    {
        return m_maxNumFramesInFlight;
    }
    const uint8_t &getMaxNumOfFramesInFlight() const
    {
        return m_maxNumFramesInFlight;
    }
    uint8_t &getCurrentFrameInFlightIndex()
    {
        return m_currentFrameInFlight;
    }
    const uint8_t &getCurrentFrameInFlightIndex() const
    {
        return m_currentFrameInFlight;
    }

  private:
    WindowingContext &m_winContext;
    uint8_t m_maxNumFramesInFlight = 1;
    uint8_t m_currentFrameInFlight = 0;
};
} // namespace star::windowing