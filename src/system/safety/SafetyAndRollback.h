#pragma once

#include <windows.h>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <chrono>

namespace CLRNet {
namespace System {

// Forward declarations
class SystemMonitor;
class RollbackManager;
class SafetyValidator;
class HealthChecker;

// Safety check types
enum class SafetyCheckType : UINT32 {
    None = 0,
    ProcessIntegrity = 0x01,
    MemoryIntegrity = 0x02,
    SystemStability = 0x04,
    PerformanceImpact = 0x08,
    SecurityConstraints = 0x10,
    DependencyValidation = 0x20,
    ResourceAvailability = 0x40,
    All = 0xFF
};

// System health status
enum class HealthStatus : UINT32 {
    Unknown,
    Healthy,
    Warning,
    Critical,
    Failed
};

// Rollback trigger conditions
enum class RollbackTrigger : UINT32 {
    None = 0,
    SystemInstability = 0x01,
    PerformanceThreshold = 0x02,
    SecurityViolation = 0x04,
    MemoryPressure = 0x08,
    ProcessCrash = 0x10,
    UserRequested = 0x20,
    TimeoutExpired = 0x40,
    DependencyFailure = 0x80,
    All = 0xFF
};

// System snapshot information
struct SystemSnapshot {
    FILETIME snapshotTime;
    std::map<DWORD, HANDLE> processHandles;
    std::map<PVOID, SIZE_T> memoryRegions;
    std::vector<std::wstring> loadedModules;
    std::map<std::wstring, std::wstring> registryKeys;
    std::vector<std::wstring> fileModifications;
    SYSTEM_INFO systemInfo;
    MEMORYSTATUSEX memoryStatus;
    ULONGLONG diskSpace;
    UINT32 snapshotId;
    std::wstring description;
};

// Health monitoring metrics
struct HealthMetrics {
    FILETIME measurementTime;
    DWORD processId;
    
    // Performance metrics
    UINT64 cpuUsagePercent;
    UINT64 memoryUsageBytes;
    UINT64 diskUsageBytes;
    UINT32 threadCount;
    UINT32 handleCount;
    
    // System metrics
    UINT64 systemCpuPercent;
    UINT64 systemMemoryPercent;
    UINT64 systemDiskPercent;
    UINT32 systemProcessCount;
    
    // Stability metrics
    UINT32 crashCount;
    UINT32 exceptionCount;
    UINT32 memoryLeakBytes;
    UINT32 resourceLeaks;
    
    // Custom metrics
    std::map<std::wstring, UINT64> customMetrics;
};

// Safety validation result
struct SafetyValidationResult {
    SafetyCheckType checkType;
    HealthStatus status;
    std::wstring description;
    std::vector<std::wstring> warnings;
    std::vector<std::wstring> errors;
    bool canProceed;
    UINT32 riskLevel; // 0-100
    std::chrono::milliseconds validationTime;
};

// Rollback operation information
struct RollbackOperation {
    UINT32 operationId;
    FILETIME startTime;
    FILETIME completionTime;
    RollbackTrigger trigger;
    UINT32 sourceSnapshotId;
    DWORD targetProcessId;
    std::vector<std::wstring> steps;
    std::vector<std::wstring> errors;
    bool completed;
    bool successful;
    std::wstring description;
};

// System monitoring and health checking
class SystemMonitor {
public:
    SystemMonitor();
    ~SystemMonitor();

    // Initialize monitoring
    HRESULT Initialize(UINT32 monitoringIntervalMs = 1000);
    void Cleanup();

    // Process monitoring
    HRESULT StartMonitoringProcess(DWORD processId);
    HRESULT StopMonitoringProcess(DWORD processId);
    
    // System-wide monitoring
    HRESULT StartSystemMonitoring();
    HRESULT StopSystemMonitoring();

    // Health metrics collection
    HRESULT GetProcessHealth(DWORD processId, HealthMetrics& metrics);
    HRESULT GetSystemHealth(HealthMetrics& metrics);
    std::vector<HealthMetrics> GetHealthHistory(DWORD processId, UINT32 maxRecords = 100);

    // Alert and threshold management
    HRESULT SetPerformanceThreshold(const std::wstring& metricName, UINT64 threshold);
    HRESULT SetHealthCallback(std::function<void(const HealthMetrics&)> callback);
    HRESULT SetAlertCallback(std::function<void(HealthStatus, const std::wstring&)> callback);

    // Monitoring status
    bool IsMonitoringProcess(DWORD processId) const;
    bool IsSystemMonitoringActive() const;
    std::vector<DWORD> GetMonitoredProcesses() const;

private:
    CRITICAL_SECTION m_criticalSection;
    bool m_initialized;
    HANDLE m_monitoringThread;
    bool m_monitoringActive;
    UINT32 m_monitoringInterval;
    
    // Monitored processes
    std::map<DWORD, HANDLE> m_monitoredProcesses;
    std::map<DWORD, std::vector<HealthMetrics>> m_healthHistory;
    
    // Thresholds and callbacks
    std::map<std::wstring, UINT64> m_thresholds;
    std::function<void(const HealthMetrics&)> m_healthCallback;
    std::function<void(HealthStatus, const std::wstring&)> m_alertCallback;

    // Internal monitoring
    static DWORD WINAPI MonitoringThreadProc(LPVOID parameter);
    void MonitoringLoop();
    HRESULT CollectProcessMetrics(DWORD processId, HealthMetrics& metrics);
    HRESULT CollectSystemMetrics(HealthMetrics& metrics);
    void CheckThresholds(const HealthMetrics& metrics);
};

// Safety validation for system operations
class SafetyValidator {
public:
    SafetyValidator();
    ~SafetyValidator();

    // Initialize validator
    HRESULT Initialize(SafetyCheckType enabledChecks = SafetyCheckType::All);
    void Cleanup();

    // Process safety validation
    HRESULT ValidateProcessSafety(DWORD processId, SafetyValidationResult& result);
    HRESULT ValidateProcessOperation(DWORD processId, const std::wstring& operation,
                                   SafetyValidationResult& result);

    // System safety validation
    HRESULT ValidateSystemSafety(SafetyValidationResult& result);
    HRESULT ValidateSystemOperation(const std::wstring& operation, SafetyValidationResult& result);

    // Specific safety checks
    HRESULT CheckProcessIntegrity(DWORD processId, SafetyValidationResult& result);
    HRESULT CheckMemoryIntegrity(DWORD processId, SafetyValidationResult& result);
    HRESULT CheckSystemStability(SafetyValidationResult& result);
    HRESULT CheckPerformanceImpact(DWORD processId, SafetyValidationResult& result);
    HRESULT CheckSecurityConstraints(DWORD processId, SafetyValidationResult& result);

    // Configuration
    HRESULT SetSafetyThreshold(SafetyCheckType checkType, UINT32 threshold);
    HRESULT EnableSafetyCheck(SafetyCheckType checkType, bool enable);
    HRESULT SetValidationTimeout(UINT32 timeoutMs);

private:
    CRITICAL_SECTION m_criticalSection;
    SafetyCheckType m_enabledChecks;
    std::map<SafetyCheckType, UINT32> m_thresholds;
    UINT32 m_validationTimeout;

    // Internal validation methods
    HRESULT PerformSafetyCheck(SafetyCheckType checkType, DWORD processId, 
                              SafetyValidationResult& result);
    HRESULT ValidateProcessMemoryLayout(DWORD processId, SafetyValidationResult& result);
    HRESULT ValidateModuleIntegrity(DWORD processId, SafetyValidationResult& result);
    HRESULT ValidateSystemResources(SafetyValidationResult& result);
};

// Rollback management for system operations
class RollbackManager {
public:
    RollbackManager();
    ~RollbackManager();

    // Initialize rollback system
    HRESULT Initialize(UINT32 maxSnapshots = 10);
    void Cleanup();

    // Snapshot management
    HRESULT CreateSystemSnapshot(const std::wstring& description, UINT32& snapshotId);
    HRESULT CreateProcessSnapshot(DWORD processId, const std::wstring& description, UINT32& snapshotId);
    HRESULT DeleteSnapshot(UINT32 snapshotId);

    // Rollback operations
    HRESULT RollbackToSnapshot(UINT32 snapshotId, UINT32& operationId);
    HRESULT RollbackProcess(DWORD processId, UINT32 snapshotId, UINT32& operationId);
    HRESULT AbortRollback(UINT32 operationId);

    // Automatic rollback triggers
    HRESULT RegisterRollbackTrigger(RollbackTrigger trigger, UINT32 snapshotId,
                                   std::function<void(UINT32)> callback = nullptr);
    HRESULT UnregisterRollbackTrigger(RollbackTrigger trigger);

    // Rollback status and information
    HRESULT GetRollbackStatus(UINT32 operationId, RollbackOperation& operation);
    std::vector<SystemSnapshot> GetAvailableSnapshots() const;
    std::vector<RollbackOperation> GetRollbackHistory(UINT32 maxRecords = 50) const;

    // Configuration
    HRESULT SetRollbackTimeout(UINT32 timeoutMs);
    HRESULT SetMaxSnapshotAge(UINT32 maxAgeMs);
    HRESULT EnableAutomaticCleanup(bool enable);

private:
    CRITICAL_SECTION m_criticalSection;
    bool m_initialized;
    UINT32 m_maxSnapshots;
    UINT32 m_rollbackTimeout;
    UINT32 m_maxSnapshotAge;
    bool m_automaticCleanup;
    UINT32 m_nextSnapshotId;
    UINT32 m_nextOperationId;
    
    // Snapshot storage
    std::map<UINT32, SystemSnapshot> m_snapshots;
    std::map<UINT32, RollbackOperation> m_rollbackOperations;
    
    // Triggers and callbacks
    std::map<RollbackTrigger, UINT32> m_triggerSnapshots;
    std::map<RollbackTrigger, std::function<void(UINT32)>> m_triggerCallbacks;

    // Internal operations
    HRESULT CaptureSystemState(SystemSnapshot& snapshot);
    HRESULT CaptureProcessState(DWORD processId, SystemSnapshot& snapshot);
    HRESULT RestoreSystemState(const SystemSnapshot& snapshot);
    HRESULT RestoreProcessState(DWORD processId, const SystemSnapshot& snapshot);
    
    // Cleanup and maintenance
    void CleanupOldSnapshots();
    HRESULT ValidateSnapshot(const SystemSnapshot& snapshot);
};

// Comprehensive health checker combining monitoring and validation
class HealthChecker {
public:
    HealthChecker();
    ~HealthChecker();

    // Initialize health checking system
    HRESULT Initialize(UINT32 checkIntervalMs = 5000);
    void Cleanup();

    // Comprehensive health assessment
    HRESULT PerformHealthCheck(DWORD processId, HealthStatus& overallStatus,
                              std::vector<SafetyValidationResult>& results);
    
    HRESULT PerformSystemHealthCheck(HealthStatus& overallStatus,
                                   std::vector<SafetyValidationResult>& results);

    // Continuous health monitoring
    HRESULT StartContinuousHealthCheck(DWORD processId);
    HRESULT StopContinuousHealthCheck(DWORD processId);

    // Component access
    std::unique_ptr<SystemMonitor> GetSystemMonitor() const;
    std::unique_ptr<SafetyValidator> GetSafetyValidator() const;
    std::unique_ptr<RollbackManager> GetRollbackManager() const;

    // Health status callbacks
    HRESULT SetHealthStatusCallback(std::function<void(DWORD, HealthStatus)> callback);
    HRESULT SetCriticalHealthCallback(std::function<void(DWORD, const std::wstring&)> callback);

    // Emergency procedures
    HRESULT TriggerEmergencyRollback(DWORD processId);
    HRESULT InitiateSystemProtection();

private:
    CRITICAL_SECTION m_criticalSection;
    bool m_initialized;
    UINT32 m_checkInterval;
    
    // Component managers
    std::unique_ptr<SystemMonitor> m_systemMonitor;
    std::unique_ptr<SafetyValidator> m_safetyValidator;
    std::unique_ptr<RollbackManager> m_rollbackManager;
    
    // Health check state
    std::map<DWORD, HealthStatus> m_processHealthStatus;
    std::map<DWORD, HANDLE> m_continuousCheckThreads;
    
    // Callbacks
    std::function<void(DWORD, HealthStatus)> m_healthStatusCallback;
    std::function<void(DWORD, const std::wstring&)> m_criticalHealthCallback;

    // Internal health checking
    static DWORD WINAPI HealthCheckThreadProc(LPVOID parameter);
    void HealthCheckLoop(DWORD processId);
    HealthStatus AggregateHealthStatus(const std::vector<SafetyValidationResult>& results);
};

// Configuration for safety and rollback systems
struct SafetySystemConfig {
    UINT32 monitoringInterval;
    UINT32 healthCheckInterval;
    UINT32 rollbackTimeout;
    UINT32 maxSnapshots;
    UINT32 maxSnapshotAge;
    SafetyCheckType enabledSafetyChecks;
    RollbackTrigger enabledTriggers;
    bool enableContinuousMonitoring;
    bool enableAutomaticRollback;
    bool enablePerformanceOptimizations;
    std::map<std::wstring, UINT64> performanceThresholds;
    std::map<SafetyCheckType, UINT32> safetyThresholds;
};

// Factory for creating safety and rollback components
class SafetySystemFactory {
public:
    // Create complete health checking system
    static std::unique_ptr<HealthChecker> CreateHealthChecker(const SafetySystemConfig& config);
    
    // Create individual components
    static std::unique_ptr<SystemMonitor> CreateSystemMonitor(UINT32 intervalMs);
    static std::unique_ptr<SafetyValidator> CreateSafetyValidator(SafetyCheckType enabledChecks);
    static std::unique_ptr<RollbackManager> CreateRollbackManager(UINT32 maxSnapshots);
    
    // Create predefined configurations
    static SafetySystemConfig CreateConservativeConfig();
    static SafetySystemConfig CreateBalancedConfig();
    static SafetySystemConfig CreateAggressiveConfig();
    
    // Validate configuration
    static bool ValidateSafetyConfig(const SafetySystemConfig& config);
};

} // namespace System
} // namespace CLRNet