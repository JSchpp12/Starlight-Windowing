#pragma once

#include <string>

namespace star::windowing
{
/// @brief Configuration shared by the windowed ImGui overlay and its owner.
struct GuiDefinition
{
    /// ImGui settings filename. Resolved into the engine's temporary directory
    /// by star::windowing::ResolveIniFile; an empty value disables ini
    /// persistence.
    std::string iniFilename{"imgui.ini"};
    bool enabled = true;
    float styleScale = 1.0f;
};
} // namespace star::windowing
