#pragma once

#include "winrt/WinRTBridge.h"
#include "pinvoke/PInvokeEngine.h"
#include "hardware/HardwareAccess.h"
#include "security/SecurityManager.h"
#include "SystemServices.h"

namespace CLRNet {
namespace Interop {

// Interop subsystem status
enum class InteropSubsystemStatus {
    NotInitialized,
    Initializing,
    Ready,
    Error,
    Shutdown
};

// Interop configuration
struct InteropConfiguration {
    bool enableWinRTBridge;
    bool enablePInvoke;
    bool enableHardwareAccess;
    bool enableSystemServices;
    SecurityLevel securityLevel;
    SandboxLevel sandboxLevel;
    std::wstring applicationId;
    std::wstring manifestPath;
    bool enableAuditLogging;
    bool enablePermissionPrompts;
};

// Interop subsystem manager - coordinates all interop functionality
class InteropManager {
private:
    // Core interop components
    std::unique_ptr<WinRTBridge> m_winrtBridge;
    std::unique_ptr<PInvokeEngine> m_pinvokeEngine;
    std::unique_ptr<HardwareAccessManager> m_hardwareManager;
    std::unique_ptr<SecurityEnforcer> m_securityEnforcer;
    std::unique_ptr<SystemServicesManager> m_systemServices;
    
    // Configuration and state
    InteropConfiguration m_configuration;
    InteropSubsystemStatus m_status;
    std::wstring m_applicationId;
    SecurityContext m_securityContext;
    
    // Synchronization
    CRITICAL_SECTION m_criticalSection;
    bool m_initialized;

public:
    InteropManager();
    ~InteropManager();
    
    // Initialize interop system
    HRESULT Initialize(const InteropConfiguration& config);
    
    // Shutdown interop system
    void Shutdown();
    
    // Configuration management
    HRESULT SetConfiguration(const InteropConfiguration& config);
    InteropConfiguration GetConfiguration() const;
    
    // Status and health checks
    InteropSubsystemStatus GetStatus() const;
    HRESULT GetSubsystemStatus(std::map<std::wstring, InteropSubsystemStatus>* status);
    bool IsHealthy() const;
    
    // Component access
    WinRTBridge* GetWinRTBridge();
    PInvokeEngine* GetPInvokeEngine();
    HardwareAccessManager* GetHardwareManager();
    SecurityEnforcer* GetSecurityEnforcer();
    SystemServicesManager* GetSystemServices();
    
    // High-level interop operations
    
    // WinRT operations
    HRESULT ActivateWinRTComponent(const std::wstring& className, void** instance);
    HRESULT CallWinRTMethod(void* instance, const std::wstring& methodName,
                           void* parameters, void* result);
    
    // P/Invoke operations
    HRESULT CallNativeFunction(const std::wstring& library, const std::wstring& function,
                              void* parameters, void* result);
    
    // Hardware operations
    HRESULT AccessHardware(HardwareCapability capability, void* parameters, void* result);
    HRESULT RequestHardwarePermission(HardwareCapability capability);
    
    // System service operations
    HRESULT AccessSystemService(SystemServiceType service, const std::wstring& operation,
                               void* parameters, void* result);
    
    // Security operations
    HRESULT ValidateOperation(const std::wstring& operation, void* parameters);
    HRESULT CheckPermissions(const std::vector<SystemCapability>& capabilities);
    
    // Utility functions
    HRESULT GetAvailableCapabilities(std::vector<HardwareCapability>* capabilities);
    HRESULT GetSystemServiceStatus(std::map<SystemServiceType, bool>* serviceStatus);
    
private:
    // Internal initialization
    HRESULT InitializeSubsystems();
    HRESULT InitializeWinRTBridge();
    HRESULT InitializePInvokeEngine();
    HRESULT InitializeHardwareManager();
    HRESULT InitializeSecurityEnforcer();
    HRESULT InitializeSystemServices();
    
    // Security enforcement
    HRESULT ValidateAccess(const std::wstring& operation, SystemCapability capability);
    HRESULT EnforceSecurityPolicy(const std::wstring& operation);
    
    // Error handling
    void HandleSubsystemError(const std::wstring& subsystem, HRESULT error);
    void SetErrorStatus(const std::wstring& errorMessage);
    
    // Cleanup
    void CleanupSubsystems();
    
    // Configuration validation
    HRESULT ValidateConfiguration(const InteropConfiguration& config);
};

// Interop factory for creating configured instances
class InteropFactory {
public:
    // Create standard interop manager for regular apps
    static InteropManager* CreateStandardInstance(const std::wstring& applicationId);
    
    // Create secure interop manager for sensitive apps
    static InteropManager* CreateSecureInstance(const std::wstring& applicationId);
    
    // Create custom interop manager with specific configuration
    static InteropManager* CreateCustomInstance(const InteropConfiguration& config);
    
    // Destroy interop manager instance
    static void DestroyInstance(InteropManager* instance);
    
    // Create pre-configured configurations
    static InteropConfiguration CreateStandardConfiguration(const std::wstring& applicationId);
    static InteropConfiguration CreateSecureConfiguration(const std::wstring& applicationId);
    static InteropConfiguration CreateMinimalConfiguration(const std::wstring& applicationId);
    
private:
    InteropFactory() = default;
};

} // namespace Interop
} // namespace CLRNet