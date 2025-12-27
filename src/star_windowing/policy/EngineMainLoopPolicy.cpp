#include "star_windowing/policy/EngineMainLoopPolicy.hpp"

namespace star::windowing
{

void EngineMainLoopPolicy::frameUpdate()
{
    glfwPollEvents();
}
} // namespace star::windowing