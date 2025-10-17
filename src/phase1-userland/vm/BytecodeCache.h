#pragma once

#ifndef CLRNET_VM_BYTECODE_CACHE_H
#define CLRNET_VM_BYTECODE_CACHE_H

#include <windows.h>
#include <memory>
#include <string>
#include <unordered_map>

namespace CLRNet {
namespace Phase1 {
namespace VM {

struct VmProgram;

// Simple in-memory + on-disk cache for compiled VM bytecode
class BytecodeCache {
public:
    BytecodeCache();
    ~BytecodeCache();

    bool Initialize();
    void Shutdown();

    std::shared_ptr<VmProgram> Get(const std::string& key);
    void Put(const std::string& key, const VmProgram& program);
    void Flush();

    std::wstring GetCacheDirectory() const { return m_cacheDirectory; }

private:
    std::wstring m_cacheDirectory;
    std::unordered_map<std::string, std::weak_ptr<VmProgram>> m_cache;
    CRITICAL_SECTION m_lock;
    bool m_initialized;

    bool EnsureCacheDirectory();
    std::wstring ComputeCachePath(const std::string& key) const;
    bool LoadFromDisk(const std::wstring& path, VmProgram& program);
    void SaveToDisk(const std::wstring& path, const VmProgram& program);
};

std::string ComputeSha1(const void* data, size_t size);

} // namespace VM
} // namespace Phase1
} // namespace CLRNet

#endif // CLRNET_VM_BYTECODE_CACHE_H
