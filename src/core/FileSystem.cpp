#include "core/FileSystem.h"

#include <windows.h>

namespace FileSystem {

bool pathExists(const std::filesystem::path& path)
{
    if (path.empty()) {
        return false;
    }
    return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool isFile(const std::filesystem::path& path)
{
    if (path.empty()) {
        return false;
    }
    const DWORD attrs = GetFileAttributesW(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES &&
           (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

} // namespace FileSystem