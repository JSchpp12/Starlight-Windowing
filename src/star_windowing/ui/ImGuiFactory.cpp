#include "star_windowing/ui/ImGuiFactory.hpp"

#include <starlight/common/ConfigFile.hpp>
#include <starlight/common/helpers/FileHelpers.hpp>

#include <filesystem>

namespace star::windowing
{
std::string ResolveIniFile(const std::string &iniFilename)
{
    if (iniFilename.empty())
        return {};

    const std::filesystem::path path(iniFilename);
    if (path.is_absolute())
        return path.string();

    const std::filesystem::path temporaryDirectory{star::ConfigFile::getSetting(star::Config_Settings::tmp_directory)};
    star::file_helpers::CreateDirectoryIfDoesNotExist(temporaryDirectory);

    return (temporaryDirectory / path).string();
}
} // namespace star::windowing
