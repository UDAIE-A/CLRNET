#include "BytecodeCache.h"
#include "VirtualMachine.h"

#include <bcrypt.h>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

#pragma comment(lib, "bcrypt.lib")

namespace CLRNet {
namespace Phase1 {
namespace VM {

namespace {

std::wstring GetExecutableDirectory() {
    wchar_t buffer[MAX_PATH];
    DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (length == 0 || length == MAX_PATH) {
        return L"";
    }

    std::wstring path(buffer, length);
    size_t separator = path.find_last_of(L"\\/");
    if (separator == std::wstring::npos) {
        return L"";
    }

    return path.substr(0, separator);
}

} // namespace

BytecodeCache::BytecodeCache()
    : m_initialized(false) {
    InitializeCriticalSection(&m_lock);
}

BytecodeCache::~BytecodeCache() {
    Shutdown();
    DeleteCriticalSection(&m_lock);
}

bool BytecodeCache::Initialize() {
    if (m_initialized) {
        return true;
    }

    if (!EnsureCacheDirectory()) {
        return false;
    }

    m_initialized = true;
    return true;
}

void BytecodeCache::Shutdown() {
    EnterCriticalSection(&m_lock);
    m_cache.clear();
    m_initialized = false;
    LeaveCriticalSection(&m_lock);
}

std::shared_ptr<VmProgram> BytecodeCache::Get(const std::string& key) {
    EnterCriticalSection(&m_lock);

    auto it = m_cache.find(key);
    if (it != m_cache.end()) {
        if (auto program = it->second.lock()) {
            LeaveCriticalSection(&m_lock);
            return program;
        }
    }

    std::wstring path = ComputeCachePath(key);
    VmProgram program;
    if (LoadFromDisk(path, program)) {
        auto shared = std::make_shared<VmProgram>(std::move(program));
        m_cache[key] = shared;
        LeaveCriticalSection(&m_lock);
        return shared;
    }

    LeaveCriticalSection(&m_lock);
    return nullptr;
}

void BytecodeCache::Put(const std::string& key, const VmProgram& program) {
    EnterCriticalSection(&m_lock);

    auto shared = std::make_shared<VmProgram>(program);
    m_cache[key] = shared;

    std::wstring path = ComputeCachePath(key);
    SaveToDisk(path, program);

    LeaveCriticalSection(&m_lock);
}

void BytecodeCache::Flush() {
    EnterCriticalSection(&m_lock);
    m_cache.clear();
    LeaveCriticalSection(&m_lock);
}

bool BytecodeCache::EnsureCacheDirectory() {
    if (!m_cacheDirectory.empty()) {
        return true;
    }

    std::wstring base = GetExecutableDirectory();
    if (base.empty()) {
        return false;
    }

    std::filesystem::path cachePath(base);
    cachePath /= L"LocalCache";
    cachePath /= L"VmBytecode";

    std::error_code ec;
    std::filesystem::create_directories(cachePath, ec);
    if (ec) {
        return false;
    }

    m_cacheDirectory = cachePath.wstring();
    return true;
}

std::wstring BytecodeCache::ComputeCachePath(const std::string& key) const {
    std::filesystem::path path(m_cacheDirectory);
    path /= std::filesystem::path(key + ".vmc");
    return path.wstring();
}

bool BytecodeCache::LoadFromDisk(const std::wstring& path, VmProgram& program) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream.is_open()) {
        return false;
    }

    uint32_t instructionCount = 0;
    uint32_t callSiteCount = 0;

    stream.read(reinterpret_cast<char*>(&program.localCount), sizeof(program.localCount));
    stream.read(reinterpret_cast<char*>(&program.argumentCount), sizeof(program.argumentCount));
    stream.read(reinterpret_cast<char*>(&instructionCount), sizeof(instructionCount));
    stream.read(reinterpret_cast<char*>(&callSiteCount), sizeof(callSiteCount));

    if (!stream.good()) {
        return false;
    }

    program.instructions.resize(instructionCount);
    program.callSites.resize(callSiteCount);

    stream.read(reinterpret_cast<char*>(program.instructions.data()),
                instructionCount * sizeof(VmInstruction));
    stream.read(reinterpret_cast<char*>(program.callSites.data()),
                callSiteCount * sizeof(VmCallSite));

    if (!stream.good()) {
        return false;
    }

    program.cacheKey = std::string(path.begin(), path.end());
    return true;
}

void BytecodeCache::SaveToDisk(const std::wstring& path, const VmProgram& program) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream.is_open()) {
        return;
    }

    uint32_t instructionCount = static_cast<uint32_t>(program.instructions.size());
    uint32_t callSiteCount = static_cast<uint32_t>(program.callSites.size());

    stream.write(reinterpret_cast<const char*>(&program.localCount), sizeof(program.localCount));
    stream.write(reinterpret_cast<const char*>(&program.argumentCount), sizeof(program.argumentCount));
    stream.write(reinterpret_cast<const char*>(&instructionCount), sizeof(instructionCount));
    stream.write(reinterpret_cast<const char*>(&callSiteCount), sizeof(callSiteCount));

    stream.write(reinterpret_cast<const char*>(program.instructions.data()),
                 instructionCount * sizeof(VmInstruction));
    stream.write(reinterpret_cast<const char*>(program.callSites.data()),
                 callSiteCount * sizeof(VmCallSite));
}

std::string ComputeSha1(const void* data, size_t size) {
    if (!data || size == 0) {
        return std::string();
    }

    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;

    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA1_ALGORITHM, nullptr, 0) != 0) {
        return std::string();
    }

    DWORD hashObjectSize = 0;
    DWORD result = 0;

    if (BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&hashObjectSize),
                          sizeof(hashObjectSize), &result, 0) != 0) {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return std::string();
    }

    std::vector<uint8_t> hashObject(hashObjectSize);
    std::vector<uint8_t> hashValue(20);

    if (BCryptCreateHash(algorithm, &hash, hashObject.data(), static_cast<ULONG>(hashObject.size()),
                         nullptr, 0, 0) != 0) {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return std::string();
    }

    if (BCryptHashData(hash, reinterpret_cast<const PUCHAR>(data), static_cast<ULONG>(size), 0) != 0) {
        BCryptDestroyHash(hash);
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return std::string();
    }

    if (BCryptFinishHash(hash, hashValue.data(), static_cast<ULONG>(hashValue.size()), 0) != 0) {
        BCryptDestroyHash(hash);
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return std::string();
    }

    std::ostringstream hex;
    hex << std::hex << std::setfill('0');
    for (uint8_t byte : hashValue) {
        hex << std::setw(2) << static_cast<int>(byte);
    }

    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm, 0);

    return hex.str();
}

} // namespace VM
} // namespace Phase1
} // namespace CLRNet
