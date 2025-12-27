#pragma once

#include "star_windowing/SwapChainRenderer.hpp"
#include "star_windowing/WindowingContext.hpp"

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
    
    void frameUpdate();

  private:
    WindowingContext &m_winContext;
};
} // namespace star::windowing