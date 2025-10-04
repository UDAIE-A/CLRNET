#include "InteropManager.h"

namespace CLRNet {
namespace Interop {

// InteropManager Implementation
InteropManager::InteropManager() 
    : m_status(InteropSubsystemStatus::NotInitialized)
    , m_initialized(false) {
    
    InitializeCriticalSection(&m_criticalSection);
    
    // Initialize configuration with defaults
    m_configuration.enableWinRTBridge = true;
    m_configuration.enablePInvoke = true;
    m_configuration.enableHardwareAccess = true;
    m_configuration.enableSystemServices = true;
    m_configuration.securityLevel = SecurityLevel::Partial;
    m_configuration.sandboxLevel = SandboxLevel::Standard;
    m_configuration.enableAuditLogging = true;
    m_configuration.enablePermissionPrompts = true;
}

InteropManager::~InteropManager() {
    Shutdown();
    DeleteCriticalSection(&m_criticalSection);
}

HRESULT InteropManager::Initialize(const InteropConfiguration& config) {
    EnterCriticalSection(&m_criticalSection);
    
    if (m_initialized) {
        LeaveCriticalSection(&m_criticalSection);
        return S_OK;
    }
    
    // Validate configuration
    HRESULT hr = ValidateConfiguration(config);
    if (FAILED(hr)) {
        LeaveCriticalSection(&m_criticalSection);
        return hr;
    }
    
    // Store configuration
    m_configuration = config;
    m_applicationId = config.applicationId;
    m_status = InteropSubsystemStatus::Initializing;
    
    // Initialize subsystems
    hr = InitializeSubsystems();
    if (FAILED(hr)) {
        SetErrorStatus(L"Failed to initialize interop subsystems");
        LeaveCriticalSection(&m_criticalSection);
        return hr;
    }
    
    m_status = InteropSubsystemStatus::Ready;
    m_initialized = true;
    
    LeaveCriticalSection(&m_criticalSection);
    return S_OK;
}

void InteropManager::Shutdown() {
    EnterCriticalSection(&m_criticalSection);
    
    if (!m_initialized) {
        LeaveCriticalSection(&m_criticalSection);
        return;
    }
    
    m_status = InteropSubsystemStatus::Shutdown;
    CleanupSubsystems();
    m_initialized = false;
    
    LeaveCriticalSection(&m_criticalSection);
}

HRESULT InteropManager::InitializeSubsystems() {
    HRESULT hr = S_OK;
    
    // Initialize security enforcer first
    if (m_configuration.enableWinRTBridge || m_configuration.enablePInvoke || 
        m_configuration.enableHardwareAccess || m_configuration.enableSystemServices) {
        
        hr = InitializeSecurityEnforcer();
        if (FAILED(hr)) {
            return hr;
        }
    }
    
    // Initialize WinRT bridge
    if (m_configuration.enableWinRTBridge) {
        hr = InitializeWinRTBridge();
        if (FAILED(hr)) {
            HandleSubsystemError(L"WinRTBridge", hr);
            return hr;
        }
    }
    
    // Initialize P/Invoke engine
    if (m_configuration.enablePInvoke) {
        hr = InitializePInvokeEngine();
        if (FAILED(hr)) {
            HandleSubsystemError(L"PInvokeEngine", hr);
            return hr;
        }
    }
    
    // Initialize hardware manager
    if (m_configuration.enableHardwareAccess) {
        hr = InitializeHardwareManager();
        if (FAILED(hr)) {
            HandleSubsystemError(L"HardwareManager", hr);
            return hr;
        }
    }
    
    // Initialize system services
    if (m_configuration.enableSystemServices) {
        hr = InitializeSystemServices();
        if (FAILED(hr)) {
            HandleSubsystemError(L"SystemServices", hr);
            return hr;
        }
    }
    
    return S_OK;
}

HRESULT InteropManager::InitializeWinRTBridge() {
    m_winrtBridge = std::make_unique<WinRTBridge>();
    return m_winrtBridge->Initialize();
}

HRESULT InteropManager::InitializePInvokeEngine() {
    m_pinvokeEngine = std::make_unique<PInvokeEngine>();
    return m_pinvokeEngine->Initialize();
}

HRESULT InteropManager::InitializeHardwareManager() {
    m_hardwareManager = std::make_unique<HardwareAccessManager>();
    return m_hardwareManager->Initialize();
}

HRESULT InteropManager::InitializeSecurityEnforcer() {
    m_securityEnforcer = std::make_unique<SecurityEnforcer>();
    
    HRESULT hr = m_securityEnforcer->Initialize();
    if (FAILED(hr)) {
        return hr;
    }
    
    // Create security context for this application
    hr = m_securityEnforcer->CreateSecurityContext(
        m_configuration.applicationId,
        m_configuration.manifestPath,
        &m_securityContext);
    
    if (FAILED(hr)) {
        return hr;
    }
    
    // Apply security configuration
    m_securityEnforcer->SetEnforcementLevel(m_configuration.securityLevel);
    m_securityEnforcer->SetDefaultSandboxLevel(m_configuration.sandboxLevel);
    
    return S_OK;
}

HRESULT InteropManager::InitializeSystemServices() {
    m_systemServices = std::make_unique<SystemServicesManager>();
    return m_systemServices->Initialize();
}

// High-level interop operations
HRESULT InteropManager::ActivateWinRTComponent(const std::wstring& className, void** instance) {
    if (!m_winrtBridge || !instance) {
        return E_INVALIDARG;
    }
    
    // Security validation
    HRESULT hr = ValidateAccess(L"ActivateWinRTComponent", SystemCapability::InternetClient);
    if (FAILED(hr)) {
        return hr;
    }
    
    // Convert std::wstring to HSTRING
    HSTRING hClassName;
    hr = WindowsCreateString(className.c_str(), static_cast<UINT32>(className.length()), &hClassName);
    if (FAILED(hr)) {
        return hr;
    }
    
    // Activate the component
    IInspectable* inspectable = nullptr;
    hr = m_winrtBridge->ActivateInstance(hClassName, &inspectable);
    
    WindowsDeleteString(hClassName);
    
    if (SUCCEEDED(hr)) {
        *instance = inspectable;
    }
    
    return hr;
}

HRESULT InteropManager::CallNativeFunction(const std::wstring& library, const std::wstring& function,
                                          void* parameters, void* result) {
    if (!m_pinvokeEngine) {
        return E_NOT_VALID_STATE;
    }
    
    // Security validation
    HRESULT hr = ValidateAccess(L"CallNativeFunction", SystemCapability::InternetClient);
    if (FAILED(hr)) {
        return hr;
    }
    
    // Convert parameters to P/Invoke format
    PInvokeParameter* pinvokeParams = static_cast<PInvokeParameter*>(parameters);
    UINT32 paramCount = 0; // Would be determined from actual parameters
    
    return m_pinvokeEngine->InvokeFunction(
        std::string(library.begin(), library.end()),
        std::string(function.begin(), function.end()),
        pinvokeParams,
        paramCount,
        result);
}

HRESULT InteropManager::AccessHardware(HardwareCapability capability, void* parameters, void* result) {
    if (!m_hardwareManager) {
        return E_NOT_VALID_STATE;
    }
    
    // Convert hardware capability to system capability for security check
    SystemCapability securityCap = SystemCapability::InternetClient; // Default
    switch (capability) {
        case HardwareCapability::Camera:
            securityCap = SystemCapability::Webcam;
            break;
        case HardwareCapability::Microphone:
            securityCap = SystemCapability::Microphone;
            break;
        case HardwareCapability::GPS:
        case HardwareCapability::LocationService:
            securityCap = SystemCapability::Location;
            break;
    }
    
    // Security validation
    HRESULT hr = ValidateAccess(L"AccessHardware", securityCap);
    if (FAILED(hr)) {
        return hr;
    }
    
    // Check if capability is available
    if (!m_hardwareManager->IsCapabilityAvailable(capability)) {
        return E_NOT_SUPPORTED;
    }
    
    // Execute hardware operation based on capability
    switch (capability) {
        case HardwareCapability::Camera: {
            if (parameters) {
                std::wstring* filePath = static_cast<std::wstring*>(parameters);
                return m_hardwareManager->TakePhoto(*filePath);
            }
            return E_INVALIDARG;
        }
        
        case HardwareCapability::GPS:
        case HardwareCapability::LocationService: {
            if (result) {
                LocationReading* location = static_cast<LocationReading*>(result);
                return m_hardwareManager->GetCurrentLocation(location);
            }
            return E_INVALIDARG;
        }
        
        default:
            return E_NOTIMPL;
    }
}

HRESULT InteropManager::ValidateAccess(const std::wstring& operation, SystemCapability capability) {
    if (!m_securityEnforcer) {
        return S_OK; // No security enforcer, allow by default
    }
    
    return m_securityEnforcer->CheckCapabilityAccess(m_applicationId, capability, 
                                                    m_configuration.enablePermissionPrompts);
}

HRESULT InteropManager::ValidateConfiguration(const InteropConfiguration& config) {
    // Check required fields
    if (config.applicationId.empty()) {
        return E_INVALIDARG;
    }
    
    // Validate security level compatibility
    if (config.securityLevel == SecurityLevel::System || 
        config.securityLevel == SecurityLevel::Administrator) {
        // These levels not allowed for regular applications
        return E_ACCESSDENIED;
    }
    
    return S_OK;
}

void InteropManager::HandleSubsystemError(const std::wstring& subsystem, HRESULT error) {
    // Log the error (in a real implementation, would use proper logging)
    std::wstring errorMsg = L"Subsystem " + subsystem + L" failed to initialize";
    SetErrorStatus(errorMsg);
}

void InteropManager::SetErrorStatus(const std::wstring& errorMessage) {
    m_status = InteropSubsystemStatus::Error;
    // In a real implementation, would store error message for retrieval
}

void InteropManager::CleanupSubsystems() {
    m_systemServices.reset();
    m_hardwareManager.reset();
    m_securityEnforcer.reset();
    m_pinvokeEngine.reset();
    m_winrtBridge.reset();
}

// Accessors
WinRTBridge* InteropManager::GetWinRTBridge() {
    return m_winrtBridge.get();
}

PInvokeEngine* InteropManager::GetPInvokeEngine() {
    return m_pinvokeEngine.get();
}

HardwareAccessManager* InteropManager::GetHardwareManager() {
    return m_hardwareManager.get();
}

SecurityEnforcer* InteropManager::GetSecurityEnforcer() {
    return m_securityEnforcer.get();
}

SystemServicesManager* InteropManager::GetSystemServices() {
    return m_systemServices.get();
}

InteropSubsystemStatus InteropManager::GetStatus() const {
    return m_status;
}

// InteropFactory Implementation
InteropManager* InteropFactory::CreateStandardInstance(const std::wstring& applicationId) {
    InteropConfiguration config = CreateStandardConfiguration(applicationId);
    return CreateCustomInstance(config);
}

InteropManager* InteropFactory::CreateSecureInstance(const std::wstring& applicationId) {
    InteropConfiguration config = CreateSecureConfiguration(applicationId);
    return CreateCustomInstance(config);
}

InteropManager* InteropFactory::CreateCustomInstance(const InteropConfiguration& config) {
    auto instance = new InteropManager();
    
    HRESULT hr = instance->Initialize(config);
    if (FAILED(hr)) {
        delete instance;
        return nullptr;
    }
    
    return instance;
}

void InteropFactory::DestroyInstance(InteropManager* instance) {
    if (instance) {
        instance->Shutdown();
        delete instance;
    }
}

InteropConfiguration InteropFactory::CreateStandardConfiguration(const std::wstring& applicationId) {
    InteropConfiguration config;
    config.enableWinRTBridge = true;
    config.enablePInvoke = true;
    config.enableHardwareAccess = true;
    config.enableSystemServices = true;
    config.securityLevel = SecurityLevel::Partial;
    config.sandboxLevel = SandboxLevel::Standard;
    config.applicationId = applicationId;
    config.manifestPath = L""; // Would be resolved from application package
    config.enableAuditLogging = true;
    config.enablePermissionPrompts = true;
    
    return config;
}

InteropConfiguration InteropFactory::CreateSecureConfiguration(const std::wstring& applicationId) {
    InteropConfiguration config;
    config.enableWinRTBridge = true;
    config.enablePInvoke = false; // Disabled for security
    config.enableHardwareAccess = true;
    config.enableSystemServices = true;
    config.securityLevel = SecurityLevel::Trusted;
    config.sandboxLevel = SandboxLevel::Enhanced;
    config.applicationId = applicationId;
    config.manifestPath = L"";
    config.enableAuditLogging = true;
    config.enablePermissionPrompts = true;
    
    return config;
}

InteropConfiguration InteropFactory::CreateMinimalConfiguration(const std::wstring& applicationId) {
    InteropConfiguration config;
    config.enableWinRTBridge = true;
    config.enablePInvoke = false;
    config.enableHardwareAccess = false;
    config.enableSystemServices = false;
    config.securityLevel = SecurityLevel::Untrusted;
    config.sandboxLevel = SandboxLevel::Maximum;
    config.applicationId = applicationId;
    config.manifestPath = L"";
    config.enableAuditLogging = true;
    config.enablePermissionPrompts = false;
    
    return config;
}

} // namespace Interop
} // namespace CLRNet