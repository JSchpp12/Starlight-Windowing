#pragma once

#include "star_windowing/Keys.hpp"

#include <map>

namespace star::windowing
{
class KeyStates
{
  public:
    /// <summary>
    /// Get the current state of a key.
    /// </summary>
    /// <param name="key">Target key</param>
    static bool state(KEY key)
    {
        return states[key];
    }

  protected:
    /// <summary>
    /// Mark key as pressed
    /// </summary>
    /// <param name="key">Target key</param>
    static void press(KEY key)
    {
        states[key] = true;
    }

    /// <summary>
    /// Mark key as released.
    /// </summary>
    /// <param name="key">Target key</param>
    static void release(KEY key)
    {
        states[key] = false;
    }

  private:
    static std::map<KEY, bool> states;
};
} // namespace star::windowing