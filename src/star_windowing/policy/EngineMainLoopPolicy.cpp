#include "star_windowing/policy/EngineMainLoopPolicy.hpp"

namespace star::windowing
{
void EngineMainLoopPolicy::init(uint8_t maxNumFramesInFlight)
{
    m_maxNumFramesInFlight = maxNumFramesInFlight;
}

void EngineMainLoopPolicy::frameUpdate()
{
    m_currentFrameInFlight = (m_currentFrameInFlight + 1) % m_maxNumFramesInFlight;
    glfwPollEvents();
}
} // namespace star::windowing