#pragma once

// Overlay configuration helpers for app-local facade assemblies.
// The overlay system allows CLRNET to probe an application's package
// for CLRNet.Facade.* assemblies before falling back to the stock
// Windows Phone 8.1 base class library.

#ifndef CLRNET_OVERLAY_CONFIG_H
#define CLRNET_OVERLAY_CONFIG_H

#include <string>
#include <unordered_map>
#include <vector>

namespace CLRNet {
namespace Phase1 {

struct OverlayConfig {
    bool enabled = false;
    std::vector<std::wstring> searchPaths;
    std::unordered_map<std::string, std::string> typeForwardMap;

    void Merge(const OverlayConfig& other);
};

class OverlayConfigLoader {
public:
    static OverlayConfig Load();

private:
    static OverlayConfig LoadFromEnvironment();
    static OverlayConfig LoadFromPackage();
    static OverlayConfig LoadFromRoot(const std::wstring& rootPath);
    static OverlayConfig LoadFromManifest(const std::wstring& manifestPath);

    static std::wstring GetExecutableDirectory();
    static bool DirectoryExists(const std::wstring& path);
    static bool FileExists(const std::wstring& path);
    static std::wstring JoinPath(const std::wstring& base, const std::wstring& relative);
    static std::wstring Trim(const std::wstring& value);
    static std::wstring NormalizeSeparators(const std::wstring& value);
    static void AppendUniquePath(std::vector<std::wstring>& paths, const std::wstring& candidate);
    static std::wstring EnvironmentString(const wchar_t* name);
};

std::wstring Utf8ToWide(const std::string& value);
std::string WideToUtf8(const std::wstring& value);

} // namespace Phase1
} // namespace CLRNet

#endif // CLRNET_OVERLAY_CONFIG_H

