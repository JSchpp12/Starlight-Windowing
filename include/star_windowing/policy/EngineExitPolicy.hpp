#pragma once

#include "star_windowing/WindowingContext.hpp"

namespace star::windowing
{
class EngineExitPolicy
{
  public:
    explicit EngineExitPolicy(WindowingContext &winContext) : m_winContext(winContext)
    {
    }
    bool shouldExit() const
    {
        return m_winContext.window.shouldClose();
    }

  private:
    WindowingContext &m_winContext;
};
} // namespace star::windowing