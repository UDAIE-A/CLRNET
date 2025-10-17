#include "OverlayConfig.h"

#include <algorithm>
#include <cwctype>
#include <cwchar>
#include <fstream>
#include <cstdio>
#include <locale>
#include <cstdlib>
#include <vector>

#include <windows.h>

namespace CLRNet {
namespace Phase1 {

namespace {

std::wstring::size_type FindLastSeparator(const std::wstring& value) {
    auto posBackslash = value.find_last_of(L'\\');
    auto posSlash = value.find_last_of(L'/');
    if (posBackslash == std::wstring::npos) return posSlash;
    if (posSlash == std::wstring::npos) return posBackslash;
    return std::max(posBackslash, posSlash);
}

} // namespace

void OverlayConfig::Merge(const OverlayConfig& other) {
    if (other.enabled) {
        enabled = true;
    }

    for (const auto& path : other.searchPaths) {
        std::wstring normalized = path;
        std::replace(normalized.begin(), normalized.end(), L'/', L'\\');

        auto duplicate = std::find_if(searchPaths.begin(), searchPaths.end(), [&](const std::wstring& existing) {
            return _wcsicmp(existing.c_str(), normalized.c_str()) == 0;
        });

        if (duplicate == searchPaths.end()) {
            searchPaths.push_back(normalized);
        }
    }

    for (const auto& entry : other.typeForwardMap) {
        // Prefer explicitly configured values, but allow new entries.
        if (typeForwardMap.find(entry.first) == typeForwardMap.end()) {
            typeForwardMap.insert(entry);
        }
    }
}

OverlayConfig OverlayConfigLoader::Load() {
    OverlayConfig result;
    OverlayConfig envConfig = LoadFromEnvironment();
    OverlayConfig packageConfig = LoadFromPackage();

    result.Merge(packageConfig);
    result.Merge(envConfig);

    return result;
}

OverlayConfig OverlayConfigLoader::LoadFromEnvironment() {
    OverlayConfig config;

    std::wstring enabledValue = EnvironmentString(L"CLRNET_OVERLAY_ENABLE");
    if (!enabledValue.empty()) {
        std::wstring lowered = enabledValue;
        std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](wchar_t ch) {
            return static_cast<wchar_t>(std::towlower(ch));
        });
        config.enabled = (lowered == L"1" || lowered == L"true" || lowered == L"yes");
    }

    std::wstring pathList = EnvironmentString(L"CLRNET_OVERLAY_PATHS");
    if (!pathList.empty()) {
        size_t start = 0;
        while (start < pathList.length()) {
            size_t end = pathList.find(L';', start);
            if (end == std::wstring::npos) {
                end = pathList.length();
            }
            std::wstring token = Trim(pathList.substr(start, end - start));
            if (!token.empty() && DirectoryExists(token)) {
                config.enabled = true;
                AppendUniquePath(config.searchPaths, NormalizeSeparators(token));
            }
            start = end + 1;
        }
    }

    std::wstring manifestPath = EnvironmentString(L"CLRNET_OVERLAY_MANIFEST");
    if (!manifestPath.empty() && FileExists(manifestPath)) {
        config.Merge(LoadFromManifest(manifestPath));
    }

    return config;
}

OverlayConfig OverlayConfigLoader::LoadFromPackage() {
    std::wstring executableDir = GetExecutableDirectory();
    return LoadFromRoot(executableDir);
}

OverlayConfig OverlayConfigLoader::LoadFromRoot(const std::wstring& rootPath) {
    OverlayConfig config;

    if (rootPath.empty()) {
        return config;
    }

    std::vector<std::wstring> candidateRoots = {
        JoinPath(rootPath, L"CLRNetOverlay"),
        JoinPath(rootPath, L"CLRNet.Facades"),
        rootPath
    };

    bool discoveredDirectory = false;

    for (const auto& candidate : candidateRoots) {
        if (DirectoryExists(candidate)) {
            AppendUniquePath(config.searchPaths, NormalizeSeparators(candidate));
            discoveredDirectory = true;

            std::wstring manifest = JoinPath(candidate, L"type-forward-map.txt");
            if (FileExists(manifest)) {
                config.Merge(LoadFromManifest(manifest));
            }

            std::wstring facades = JoinPath(candidate, L"facades");
            if (DirectoryExists(facades)) {
                AppendUniquePath(config.searchPaths, NormalizeSeparators(facades));
            }
        }
    }

    if (discoveredDirectory) {
        config.enabled = true;
    }

    return config;
}

OverlayConfig OverlayConfigLoader::LoadFromManifest(const std::wstring& manifestPath) {
    OverlayConfig config;

    FILE* file = nullptr;
    if (_wfopen_s(&file, manifestPath.c_str(), L"r, ccs=UTF-8") != 0 || !file) {
        return config;
    }

    wchar_t buffer[1024];
    while (fgetws(buffer, static_cast<int>(sizeof(buffer) / sizeof(buffer[0])), file)) {
        std::wstring line(buffer);
        line = Trim(line);
        if (line.empty()) {
            continue;
        }
        if (line[0] == L'#' || line[0] == L';') {
            continue;
        }

        size_t equalsPos = line.find(L'=');
        if (equalsPos == std::wstring::npos) {
            continue;
        }

        std::wstring typeName = Trim(line.substr(0, equalsPos));
        std::wstring assemblyName = Trim(line.substr(equalsPos + 1));

        if (!typeName.empty() && !assemblyName.empty()) {
            config.enabled = true;
            config.typeForwardMap[WideToUtf8(typeName)] = WideToUtf8(assemblyName);
        }
    }

    fclose(file);
    return config;
}

std::wstring OverlayConfigLoader::GetExecutableDirectory() {
    wchar_t pathBuffer[MAX_PATH];
    DWORD length = GetModuleFileNameW(nullptr, pathBuffer, MAX_PATH);
    if (length == 0 || length == MAX_PATH) {
        return L"";
    }

    std::wstring path(pathBuffer, length);
    auto separator = FindLastSeparator(path);
    if (separator == std::wstring::npos) {
        return L"";
    }

    return path.substr(0, separator);
}

bool OverlayConfigLoader::DirectoryExists(const std::wstring& path) {
    DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY);
}

bool OverlayConfigLoader::FileExists(const std::wstring& path) {
    DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}

std::wstring OverlayConfigLoader::JoinPath(const std::wstring& base, const std::wstring& relative) {
    if (relative.empty()) {
        return NormalizeSeparators(base);
    }

    if (relative.length() >= 2 && (relative[1] == L':' || (relative[0] == L'\\' && relative[1] == L'\\'))) {
        // Already absolute.
        return NormalizeSeparators(relative);
    }

    if (base.empty()) {
        return NormalizeSeparators(relative);
    }

    std::wstring normalizedBase = NormalizeSeparators(base);
    if (!normalizedBase.empty() && normalizedBase.back() != L'\\') {
        normalizedBase += L'\\';
    }

    return normalizedBase + NormalizeSeparators(relative);
}

std::wstring OverlayConfigLoader::Trim(const std::wstring& value) {
    size_t start = 0;
    while (start < value.length() && std::iswspace(value[start])) {
        ++start;
    }

    if (start == value.length()) {
        return L"";
    }

    size_t end = value.length() - 1;
    while (end > start && std::iswspace(value[end])) {
        --end;
    }

    return value.substr(start, end - start + 1);
}

std::wstring OverlayConfigLoader::NormalizeSeparators(const std::wstring& value) {
    std::wstring normalized = value;
    std::replace(normalized.begin(), normalized.end(), L'/', L'\\');
    return normalized;
}

void OverlayConfigLoader::AppendUniquePath(std::vector<std::wstring>& paths, const std::wstring& candidate) {
    std::wstring normalized = NormalizeSeparators(candidate);
    if (normalized.empty()) {
        return;
    }

    auto it = std::find_if(paths.begin(), paths.end(), [&](const std::wstring& existing) {
        return _wcsicmp(existing.c_str(), normalized.c_str()) == 0;
    });

    if (it == paths.end()) {
        paths.push_back(normalized);
    }
}

std::wstring OverlayConfigLoader::EnvironmentString(const wchar_t* name) {
    if (!name) {
        return L"";
    }

    wchar_t* value = _wgetenv(name);
    if (!value) {
        return L"";
    }

    return value;
}

std::wstring Utf8ToWide(const std::string& value) {
    if (value.empty()) {
        return L"";
    }

    int requiredLength = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0);
    if (requiredLength <= 0) {
        return L"";
    }

    std::vector<wchar_t> buffer(static_cast<size_t>(requiredLength));
    int written = MultiByteToWideChar(
        CP_UTF8,
        0,
        value.c_str(),
        static_cast<int>(value.size()),
        buffer.data(),
        requiredLength
    );
    if (written <= 0) {
        return L"";
    }

    return std::wstring(buffer.data(), static_cast<size_t>(written));
}

std::string WideToUtf8(const std::wstring& value) {
    if (value.empty()) {
        return std::string();
    }

    int requiredLength = WideCharToMultiByte(
        CP_UTF8,
        0,
        value.c_str(),
        static_cast<int>(value.size()),
        nullptr,
        0,
        nullptr,
        nullptr
    );
    if (requiredLength <= 0) {
        return std::string();
    }

    std::vector<char> buffer(static_cast<size_t>(requiredLength));
    int written = WideCharToMultiByte(
        CP_UTF8,
        0,
        value.c_str(),
        static_cast<int>(value.size()),
        buffer.data(),
        requiredLength,
        nullptr,
        nullptr
    );
    if (written <= 0) {
        return std::string();
    }

    return std::string(buffer.data(), static_cast<size_t>(written));
}

} // namespace Phase1
} // namespace CLRNet

