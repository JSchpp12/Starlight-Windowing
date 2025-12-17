#pragma once

#include "star_windowing/StarWindow.hpp"
#include "star_windowing/RenderingSurface.hpp"

namespace star::windowing
{
struct WindowingContext
{
    RenderingSurface surface;
    StarWindow window; 
};
} // namespace star::windowing