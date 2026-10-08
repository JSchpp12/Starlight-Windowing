#pragma once

#include <string>

namespace star::windowing
{
/// @brief Free helpers for locating ImGui runtime files.
///
/// ImGui owns a settings/persistence file (`imgui.ini`) which records window
/// positions, sizes, collapsed state, table columns, etc. between runs. These
/// helpers keep that file out of the working/media directories and inside the
/// engine's temporary directory.

/// Resolve the ImGui ini filename into the engine's temporary directory
/// (`Config_Settings::tmp_directory`). An empty filename disables the ini file
/// entirely (returns an empty string); an absolute path is honored as-is.
std::string ResolveIniFile(const std::string &iniFilename);
} // namespace star::windowing
