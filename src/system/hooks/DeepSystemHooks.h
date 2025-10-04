#pragma once

#include <windows.h>
#include <vector>
#include <memory>
#include <functional>

namespace CLRNet {
namespace System {

// Forward declarations for CLR system hooks
struct HookInstallationInfo;
class KernelIntegrationManager;
class MemoryManagerOverride;
class JITInterceptionEngine;

// Kernel-level hook types
enum class HookType : UINT32 {
    None = 0,
    ProcessCreation = 0x01,
    ImageLoad = 0x02,
    ThreadCreation = 0x04,
    MemoryAllocation = 0x08,
    SystemCall = 0x10,
    JITCompilation = 0x20,
    AssemblyLoad = 0x40,
    All = 0xFF
};

// Hook installation flags
enum class HookInstallFlags : UINT32 {
    None = 0,
    BypassSecurity = 0x01,
    SystemWideHook = 0x02,
    KernelModeHook = 0x04,
    PassiveMode = 0x08,
    HighPriority = 0x10
};

// Memory operation types
enum class MemoryOperationType : UINT32 {
    Allocation,
    Deallocation,
    Protection,
    Mapping,
    GCCollection
};

// JIT compilation phases
enum class JITPhase : UINT32 {
    PreJIT,
    Compilation,
    PostJIT,
    Optimization,
    CodeGeneration
};

// System hook callback signatures
typedef NTSTATUS(*ProcessCreationCallback)(HANDLE processId, HANDLE threadId, BOOLEAN create);
typedef NTSTATUS(*ImageLoadCallback)(PUNICODE_STRING imageName, HANDLE processId, PIMAGE_INFO imageInfo);
typedef NTSTATUS(*ThreadCallback)(HANDLE processId, HANDLE threadId, BOOLEAN create);
typedef NTSTATUS(*MemoryCallback)(HANDLE processId, PVOID baseAddress, SIZE_T size, 
                                 MemoryOperationType operation, ULONG protection);
typedef HRESULT(*JITCallback)(HANDLE processId, PVOID methodHandle, JITPhase phase, 
                             PVOID codeAddress, SIZE_T codeSize);

// Hook installation information
struct HookInstallationInfo {
    HookType type;
    HookInstallFlags flags;
    PVOID callbackAddress;
    PVOID originalFunction;
    HANDLE hookHandle;
    FILETIME installTime;
    bool isActive;
    DWORD targetProcessId; // 0 for system-wide
    std::wstring description;
};

// System call interception information
struct SystemCallInfo {
    ULONG systemCallNumber;
    PVOID originalHandler;
    PVOID interceptHandler;
    bool isIntercepted;
    ULONG callCount;
    FILETIME lastCall;
};

// Memory management override capabilities
class MemoryManagerOverride {
public:
    MemoryManagerOverride();
    ~MemoryManagerOverride();

    // Initialize memory management override
    HRESULT Initialize();
    void Cleanup();

    // Memory allocation interception
    HRESULT InstallAllocationHooks(DWORD processId = 0);
    HRESULT RemoveAllocationHooks(DWORD processId = 0);

    // Custom memory allocator
    PVOID AllocateMemory(SIZE_T size, ULONG protection = PAGE_READWRITE);
    HRESULT FreeMemory(PVOID address);
    HRESULT ProtectMemory(PVOID address, SIZE_T size, ULONG newProtection, PULONG oldProtection);

    // Garbage collection override
    HRESULT OverrideGarbageCollector(DWORD processId);
    HRESULT RestoreGarbageCollector(DWORD processId);

    // Memory tracking and statistics
    SIZE_T GetAllocatedMemory() const;
    ULONG GetAllocationCount() const;
    std::vector<MEMORY_BASIC_INFORMATION> GetMemoryRegions(DWORD processId) const;

private:
    CRITICAL_SECTION m_criticalSection;
    std::map<PVOID, SIZE_T> m_allocations;
    std::map<DWORD, std::vector<HookInstallationInfo>> m_processHooks;
    SIZE_T m_totalAllocated;
    ULONG m_allocationCount;

    // Internal allocation tracking
    void TrackAllocation(PVOID address, SIZE_T size);
    void UntrackAllocation(PVOID address);
    
    // Hook management
    HRESULT InstallMemoryHook(DWORD processId, PVOID targetFunction, PVOID hookFunction);
    HRESULT RemoveMemoryHook(DWORD processId, PVOID targetFunction);
};

// JIT compilation interception engine
class JITInterceptionEngine {
public:
    JITInterceptionEngine();
    ~JITInterceptionEngine();

    // Initialize JIT interception
    HRESULT Initialize();
    void Cleanup();

    // JIT hook installation
    HRESULT InstallJITHooks(DWORD processId = 0);
    HRESULT RemoveJITHooks(DWORD processId = 0);

    // JIT compilation callbacks
    HRESULT RegisterJITCallback(JITPhase phase, JITCallback callback);
    HRESULT UnregisterJITCallback(JITPhase phase);

    // Code generation override
    HRESULT OverrideCodeGeneration(DWORD processId, PVOID methodHandle, 
                                  PVOID customCodeBuffer, SIZE_T codeSize);
    
    // JIT optimization control
    HRESULT EnableOptimizations(DWORD processId, bool enable);
    HRESULT SetOptimizationLevel(DWORD processId, UINT32 level);

    // Compilation statistics
    ULONG GetCompiledMethodCount() const;
    SIZE_T GetGeneratedCodeSize() const;
    std::vector<PVOID> GetCompiledMethods(DWORD processId) const;

private:
    CRITICAL_SECTION m_criticalSection;
    std::map<JITPhase, JITCallback> m_callbacks;
    std::map<DWORD, std::vector<HookInstallationInfo>> m_jitHooks;
    std::map<PVOID, SIZE_T> m_compiledMethods;
    ULONG m_compiledCount;
    SIZE_T m_generatedSize;

    // Internal JIT management
    HRESULT HookJITCompiler(DWORD processId);
    HRESULT InterceptCompilation(PVOID methodHandle, JITPhase phase);
    void TrackCompiledMethod(PVOID methodHandle, SIZE_T codeSize);
};

// Kernel integration manager for deep system hooks
class KernelIntegrationManager {
public:
    KernelIntegrationManager();
    ~KernelIntegrationManager();

    // Initialize kernel integration
    HRESULT Initialize(HookType enabledHooks = HookType::All);
    void Cleanup();

    // Process and thread hooks
    HRESULT InstallProcessHooks(ProcessCreationCallback callback);
    HRESULT InstallThreadHooks(ThreadCallback callback);
    HRESULT InstallImageLoadHooks(ImageLoadCallback callback);

    // System call interception
    HRESULT InstallSystemCallHook(ULONG systemCallNumber, PVOID interceptHandler);
    HRESULT RemoveSystemCallHook(ULONG systemCallNumber);

    // Memory management integration
    HRESULT InstallMemoryHooks(MemoryCallback callback);
    std::unique_ptr<MemoryManagerOverride> GetMemoryManager() const;

    // JIT compilation integration
    HRESULT InstallJITHooks(JITCallback callback);
    std::unique_ptr<JITInterceptionEngine> GetJITEngine() const;

    // Hook management
    HRESULT EnableHook(HookType hookType, HookInstallFlags flags = HookInstallFlags::None);
    HRESULT DisableHook(HookType hookType);
    bool IsHookActive(HookType hookType) const;

    // System information
    std::vector<HookInstallationInfo> GetInstalledHooks() const;
    std::vector<SystemCallInfo> GetInterceptedSystemCalls() const;
    HRESULT GetSystemStatistics(PVOID statisticsBuffer, SIZE_T bufferSize) const;

    // Safety and rollback
    HRESULT CreateHookSnapshot();
    HRESULT RestoreFromSnapshot();
    HRESULT ValidateSystemIntegrity() const;

private:
    CRITICAL_SECTION m_criticalSection;
    HookType m_enabledHooks;
    bool m_initialized;
    
    // Hook management
    std::map<HookType, HookInstallationInfo> m_installedHooks;
    std::map<ULONG, SystemCallInfo> m_interceptedCalls;
    
    // Component managers
    std::unique_ptr<MemoryManagerOverride> m_memoryManager;
    std::unique_ptr<JITInterceptionEngine> m_jitEngine;
    
    // Callback storage
    ProcessCreationCallback m_processCallback;
    ThreadCallback m_threadCallback;
    ImageLoadCallback m_imageCallback;
    MemoryCallback m_memoryCallback;
    JITCallback m_jitCallback;

    // Internal hook management
    HRESULT InstallKernelHook(HookType type, PVOID callbackFunction, HookInstallFlags flags);
    HRESULT RemoveKernelHook(HookType type);
    
    // System call management
    HRESULT SetupSystemCallInterception();
    HRESULT CleanupSystemCallInterception();
    
    // Validation and safety
    bool ValidateHookInstallation(const HookInstallationInfo& hookInfo) const;
    HRESULT CheckSystemStability() const;
};

// Deep system hooks configuration
struct DeepHooksConfig {
    HookType enabledHooks;
    HookInstallFlags installFlags;
    bool enableMemoryOverride;
    bool enableJITInterception;
    bool enableSystemCallHooks;
    bool enableKernelModeHooks;
    UINT32 maxHookCount;
    UINT32 hookTimeoutMs;
    bool enableHookValidation;
    bool enableRollbackOnFailure;
    std::vector<ULONG> excludedSystemCalls;
    std::vector<std::wstring> excludedProcesses;
};

// Factory for creating deep system hooks
class DeepSystemHooksFactory {
public:
    // Create kernel integration manager with configuration
    static std::unique_ptr<KernelIntegrationManager> CreateKernelManager(const DeepHooksConfig& config);
    
    // Create memory manager override
    static std::unique_ptr<MemoryManagerOverride> CreateMemoryManager();
    
    // Create JIT interception engine
    static std::unique_ptr<JITInterceptionEngine> CreateJITEngine();
    
    // Create safe configuration for testing
    static DeepHooksConfig CreateSafeConfiguration();
    
    // Create aggressive configuration for full system control
    static DeepHooksConfig CreateAggressiveConfiguration();
    
    // Validate configuration safety
    static bool ValidateConfiguration(const DeepHooksConfig& config);
};

} // namespace System
} // namespace CLRNet