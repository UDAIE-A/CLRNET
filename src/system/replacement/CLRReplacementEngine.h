#pragma once

#include <windows.h>
#include <winternl.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <map>
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace CLRNet {
namespace System {

// Replacement levels and policies
enum class ReplacementLevel {
    None,              // No replacement
    ProcessLevel,      // Replace CLR per-process
    SelectiveSystem,   // Replace CLR for specific app types
    SystemWide         // Replace system-wide (advanced)
};

enum class ReplacementStrategy {
    Conservative,      // Minimal changes, maximum safety
    Balanced,         // Moderate changes, good performance
    Aggressive        // Maximum optimization, higher risk
};

enum class ReplacementStatus {
    NotReplaced,
    PartiallyReplaced,
    FullyReplaced,
    RollbackInProgress,
    Failed
};

// Process injection information
struct ProcessInjectionInfo {
    DWORD processId;
    std::wstring processName;
    std::wstring imagePath;
    HANDLE processHandle;
    bool isManaged;
    bool isReplaced;
    FILETIME injectionTime;
    ReplacementStatus status;
};

// CLR replacement configuration
struct CLRReplacementConfig {
    ReplacementLevel level;
    ReplacementStrategy strategy;
    bool enablePerformanceOptimizations;
    bool enableCompatibilityMode;
    bool enableDetailedLogging;
    std::vector<std::wstring> excludedProcesses;
    std::vector<std::wstring> priorityProcesses;
    DWORD healthCheckInterval;
    DWORD rollbackTimeoutMs;
};

// Forward declarations
class LegacyCLRDetector;
class ProcessInjector;
class ModernCLRInjector;
class CompatibilityShim;
class SystemHealthMonitor;
class RollbackManager;

// Legacy CLR detection and analysis
class LegacyCLRDetector {
private:
    std::map<DWORD, ProcessInjectionInfo> m_managedProcesses;
    CRITICAL_SECTION m_criticalSection;

public:
    LegacyCLRDetector();
    ~LegacyCLRDetector();
    
    // Initialize detector
    HRESULT Initialize();
    
    // Detect legacy CLR usage
    HRESULT ScanForManagedProcesses();
    
    // Check if specific process uses managed code
    bool IsProcessManaged(DWORD processId);
    
    // Get CLR version information
    HRESULT GetCLRVersion(DWORD processId, std::wstring* version);
    
    // Analyze process for replacement compatibility
    HRESULT AnalyzeProcess(DWORD processId, bool* canReplace, 
                          std::vector<std::wstring>* issues);
    
    // Get managed processes list
    std::vector<ProcessInjectionInfo> GetManagedProcesses() const;
    
    // Monitor for new managed processes
    HRESULT StartProcessMonitoring(std::function<void(DWORD)> newProcessCallback);
    
    // Stop monitoring
    void StopProcessMonitoring();

private:
    // Internal detection methods
    bool IsAssemblyLoaded(HANDLE processHandle);
    HRESULT EnumerateLoadedModules(HANDLE processHandle, 
                                  std::vector<std::wstring>* modules);
    bool IsCLRModule(const std::wstring& moduleName);
};

// Process injection engine
class ProcessInjector {
private:
    std::map<DWORD, HANDLE> m_injectedProcesses;
    std::wstring m_injectionDllPath;
    CRITICAL_SECTION m_criticalSection;

public:
    ProcessInjector();
    ~ProcessInjector();
    
    // Initialize injector
    HRESULT Initialize(const std::wstring& injectionDllPath);
    
    // Inject modern CLR into process
    HRESULT InjectIntoProcess(DWORD processId);
    
    // Inject using different methods
    HRESULT InjectUsingDLL(DWORD processId, const std::wstring& dllPath);
    HRESULT InjectUsingShellcode(DWORD processId, const void* shellcode, SIZE_T size);
    HRESULT InjectUsingManualMapping(DWORD processId, const std::wstring& dllPath);
    
    // Remove injection from process
    HRESULT RemoveFromProcess(DWORD processId);
    
    // Check injection status
    bool IsProcessInjected(DWORD processId) const;
    
    // Get injection information
    HRESULT GetInjectionInfo(DWORD processId, ProcessInjectionInfo* info);

private:
    // Internal injection methods
    HRESULT CreateRemoteThread(HANDLE processHandle, LPVOID remoteFunction, 
                              LPVOID parameter);
    HRESULT WriteToProcessMemory(HANDLE processHandle, LPVOID baseAddress,
                                const void* buffer, SIZE_T size);
    HRESULT AllocateProcessMemory(HANDLE processHandle, SIZE_T size, 
                                 LPVOID* allocatedAddress);
};

// Modern CLR injector
class ModernCLRInjector {
private:
    std::wstring m_modernCLRPath;
    std::map<DWORD, std::wstring> m_injectedCLRPaths;
    
public:
    ModernCLRInjector();
    ~ModernCLRInjector();
    
    // Initialize with modern CLR path
    HRESULT Initialize(const std::wstring& modernCLRPath);
    
    // Replace CLR in target process
    HRESULT ReplaceCLRInProcess(DWORD processId);
    
    // Configure CLR replacement options
    HRESULT ConfigureReplacement(DWORD processId, const CLRReplacementConfig& config);
    
    // Verify replacement success
    bool VerifyReplacement(DWORD processId);
    
    // Get replacement status
    ReplacementStatus GetReplacementStatus(DWORD processId);

private:
    // Internal replacement methods
    HRESULT LoadModernCLR(HANDLE processHandle);
    HRESULT RedirectCLRCalls(HANDLE processHandle);
    HRESULT InitializeModernRuntime(HANDLE processHandle);
};

// Compatibility shim manager
class CompatibilityShim {
private:
    struct APIShim {
        std::wstring originalAPI;
        std::wstring replacementAPI;
        FARPROC originalFunction;
        FARPROC replacementFunction;
    };
    
    std::vector<APIShim> m_apiShims;
    
public:
    CompatibilityShim();
    ~CompatibilityShim();
    
    // Initialize compatibility shims
    HRESULT Initialize();
    
    // Install API shim
    HRESULT InstallAPIShim(const std::wstring& originalAPI,
                          const std::wstring& replacementAPI);
    
    // Remove API shim
    HRESULT RemoveAPIShim(const std::wstring& originalAPI);
    
    // Apply shims to process
    HRESULT ApplyShimsToProcess(DWORD processId);
    
    // Remove shims from process
    HRESULT RemoveShimsFromProcess(DWORD processId);

private:
    // Internal shimming methods
    HRESULT HookFunction(FARPROC originalFunction, FARPROC replacementFunction);
    HRESULT UnhookFunction(FARPROC originalFunction);
};

// Main CLR replacement engine
class CLRReplacementEngine {
private:
    // Component managers
    std::unique_ptr<LegacyCLRDetector> m_legacyDetector;
    std::unique_ptr<ProcessInjector> m_processInjector;
    std::unique_ptr<ModernCLRInjector> m_modernInjector;
    std::unique_ptr<CompatibilityShim> m_compatibilityShim;
    
    // Configuration and state
    CLRReplacementConfig m_config;
    ReplacementLevel m_currentLevel;
    bool m_initialized;
    
    // Process tracking
    std::map<DWORD, ProcessInjectionInfo> m_replacedProcesses;
    CRITICAL_SECTION m_criticalSection;
    
    // Monitoring thread
    HANDLE m_monitoringThread;
    bool m_monitoringActive;

public:
    CLRReplacementEngine();
    ~CLRReplacementEngine();
    
    // Initialize replacement engine
    HRESULT Initialize(ReplacementLevel level);
    
    // Configure replacement options
    HRESULT Configure(const CLRReplacementConfig& config);
    
    // Process-level CLR replacement
    HRESULT ReplaceProcessCLR(DWORD processId);
    HRESULT ReplaceProcessCLR(const std::wstring& processName);
    
    // Batch process replacement
    HRESULT ReplaceMultipleProcesses(const std::vector<DWORD>& processIds);
    
    // System-level replacement (advanced)
    HRESULT ReplaceSystemCLR(ReplacementStrategy strategy);
    
    // Rollback operations
    HRESULT RollbackProcess(DWORD processId);
    HRESULT RollbackAllProcesses();
    HRESULT RollbackSystem();
    
    // Status and monitoring
    HRESULT GetReplacementStatus(std::map<DWORD, ReplacementStatus>* status);
    HRESULT GetReplacedProcesses(std::vector<ProcessInjectionInfo>* processes);
    
    // Health validation
    bool ValidateSystemHealth();
    HRESULT GetHealthMetrics(std::map<std::wstring, double>* metrics);
    
    // Configuration management
    CLRReplacementConfig GetConfiguration() const;
    HRESULT UpdateConfiguration(const CLRReplacementConfig& config);
    
    // Emergency operations
    HRESULT EmergencyStop();
    HRESULT EmergencyRollback();

private:
    // Internal operations
    HRESULT InitializeComponents();
    HRESULT StartMonitoring();
    void StopMonitoring();
    
    // Monitoring thread function
    static DWORD WINAPI MonitoringThreadProc(LPVOID parameter);
    void MonitoringLoop();
    
    // Process management
    HRESULT HandleNewProcess(DWORD processId);
    HRESULT HandleProcessTermination(DWORD processId);
    
    // Safety checks
    bool CanReplaceProcess(DWORD processId);
    bool IsSystemCriticalProcess(DWORD processId);
    
    // Rollback helpers
    HRESULT PrepareRollback(DWORD processId);
    HRESULT ExecuteRollback(DWORD processId);
    HRESULT VerifyRollback(DWORD processId);
};

// CLR replacement factory
class CLRReplacementFactory {
public:
    // Create replacement engine with configuration
    static CLRReplacementEngine* CreateEngine(ReplacementLevel level);
    
    // Create with custom configuration
    static CLRReplacementEngine* CreateEngine(const CLRReplacementConfig& config);
    
    // Destroy engine instance
    static void DestroyEngine(CLRReplacementEngine* engine);
    
    // Create default configurations
    static CLRReplacementConfig CreateSafeConfiguration();
    static CLRReplacementConfig CreateBalancedConfiguration();
    static CLRReplacementConfig CreateAggressiveConfiguration();
    
private:
    CLRReplacementFactory() = default;
};

} // namespace System
} // namespace CLRNet