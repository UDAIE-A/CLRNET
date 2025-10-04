#include "CompatibilityShim.h"
#include <algorithm>
#include <shlwapi.h>
#include <tlhelp32.h>

#pragma comment(lib, "shlwapi.lib")

namespace CLRNet {
namespace System {

// LegacyApiShim Implementation
LegacyApiShim::LegacyApiShim() : m_compatibilityLevel(CompatibilityLevel::None) {
    InitializeCriticalSection(&m_criticalSection);
}

LegacyApiShim::~LegacyApiShim() {
    Cleanup();
    DeleteCriticalSection(&m_criticalSection);
}

HRESULT LegacyApiShim::Initialize(CompatibilityLevel level) {
    m_compatibilityLevel = level;
    
    EnterCriticalSection(&m_criticalSection);
    m_installedShims.clear();
    LeaveCriticalSection(&m_criticalSection);
    
    // Install basic shims based on compatibility level
    switch (level) {
        case CompatibilityLevel::Full:
            InstallFrameworkShims(FrameworkVersion::NetFramework20);
            // Fall through
        case CompatibilityLevel::Extended:
            InstallFrameworkShims(FrameworkVersion::NetFramework35);
            // Fall through
        case CompatibilityLevel::Standard:
            InstallFrameworkShims(FrameworkVersion::NetFramework40);
            InstallCommonApiShims();
            break;
        case CompatibilityLevel::Minimal:
            InstallCommonApiShims();
            break;
        default:
            break;
    }
    
    return S_OK;
}

void LegacyApiShim::Cleanup() {
    EnterCriticalSection(&m_criticalSection);
    
    // Remove all installed shims
    for (auto& shimPair : m_installedShims) {
        if (shimPair.second.isInstalled && shimPair.second.originalFunction) {
            // Restore original function
            // In real implementation, this would use VirtualProtect and memory patching
        }
    }
    
    m_installedShims.clear();
    LeaveCriticalSection(&m_criticalSection);
}

HRESULT LegacyApiShim::InstallApiShim(const std::wstring& apiName, 
                                     PVOID legacyFunction, PVOID modernFunction) {
    if (!legacyFunction || !modernFunction) {
        return E_INVALIDARG;
    }
    
    return InstallSingleShim(apiName, legacyFunction, modernFunction);
}

HRESULT LegacyApiShim::InstallSingleShim(const std::wstring& apiName, 
                                        PVOID original, PVOID replacement) {
    EnterCriticalSection(&m_criticalSection);
    
    ShimInstallationInfo shimInfo;
    shimInfo.type = ShimType::ApiRedirection;
    shimInfo.targetApi = apiName;
    shimInfo.modernApi = apiName + L"_Modern";
    shimInfo.originalFunction = original;
    shimInfo.shimFunction = replacement;
    shimInfo.isInstalled = true;
    GetSystemTimeAsFileTime(&shimInfo.installTime);
    shimInfo.processId = GetCurrentProcessId();
    shimInfo.description = L"API redirection shim for " + apiName;
    
    m_installedShims[apiName] = shimInfo;
    
    LeaveCriticalSection(&m_criticalSection);
    return S_OK;
}

HRESULT LegacyApiShim::InstallCommonApiShims() {
    // Install shims for common .NET Framework APIs
    struct ApiShim {
        std::wstring name;
        PVOID legacyAddr;
        PVOID modernAddr;
    };
    
    // Simulated API addresses (in real implementation, these would be resolved dynamically)
    std::vector<ApiShim> commonShims = {
        {L"System.GC.Collect", reinterpret_cast<PVOID>(0x10001000), reinterpret_cast<PVOID>(0x20001000)},
        {L"System.Threading.Thread.Start", reinterpret_cast<PVOID>(0x10002000), reinterpret_cast<PVOID>(0x20002000)},
        {L"System.Reflection.Assembly.LoadFrom", reinterpret_cast<PVOID>(0x10003000), reinterpret_cast<PVOID>(0x20003000)},
        {L"System.AppDomain.CreateDomain", reinterpret_cast<PVOID>(0x10004000), reinterpret_cast<PVOID>(0x20004000)},
        {L"System.Runtime.Remoting.RemotingServices.Marshal", reinterpret_cast<PVOID>(0x10005000), reinterpret_cast<PVOID>(0x20005000)}
    };
    
    for (const auto& shim : commonShims) {
        InstallSingleShim(shim.name, shim.legacyAddr, shim.modernAddr);
    }
    
    return S_OK;
}

HRESULT LegacyApiShim::InstallFrameworkShims(FrameworkVersion frameworkVersion) {
    // Install version-specific shims
    switch (frameworkVersion) {
        case FrameworkVersion::NetFramework20:
            // .NET 2.0 specific shims
            InstallSingleShim(L"System.Web.Security.Membership.CreateUser", 
                            reinterpret_cast<PVOID>(0x11001000), 
                            reinterpret_cast<PVOID>(0x21001000));
            break;
            
        case FrameworkVersion::NetFramework35:
            // .NET 3.5 specific shims (LINQ, WCF)
            InstallSingleShim(L"System.Linq.Enumerable.Where", 
                            reinterpret_cast<PVOID>(0x11002000), 
                            reinterpret_cast<PVOID>(0x21002000));
            break;
            
        case FrameworkVersion::NetFramework40:
            // .NET 4.0 specific shims (Task, dynamic)
            InstallSingleShim(L"System.Threading.Tasks.Task.Run", 
                            reinterpret_cast<PVOID>(0x11003000), 
                            reinterpret_cast<PVOID>(0x21003000));
            break;
            
        default:
            break;
    }
    
    return S_OK;
}

std::vector<ShimInstallationInfo> LegacyApiShim::GetInstalledShims() const {
    std::vector<ShimInstallationInfo> shims;
    
    EnterCriticalSection(const_cast<CRITICAL_SECTION*>(&m_criticalSection));
    
    for (const auto& shimPair : m_installedShims) {
        shims.push_back(shimPair.second);
    }
    
    LeaveCriticalSection(const_cast<CRITICAL_SECTION*>(&m_criticalSection));
    return shims;
}

bool LegacyApiShim::IsApiShimmed(const std::wstring& apiName) const {
    EnterCriticalSection(const_cast<CRITICAL_SECTION*>(&m_criticalSection));
    
    auto it = m_installedShims.find(apiName);
    bool isShimmed = (it != m_installedShims.end()) && it->second.isInstalled;
    
    LeaveCriticalSection(const_cast<CRITICAL_SECTION*>(&m_criticalSection));
    return isShimmed;
}

// RuntimeCompatibilityLayer Implementation
RuntimeCompatibilityLayer::RuntimeCompatibilityLayer() : m_targetVersion(FrameworkVersion::Unknown) {
    InitializeCriticalSection(&m_criticalSection);
}

RuntimeCompatibilityLayer::~RuntimeCompatibilityLayer() {
    Cleanup();
    DeleteCriticalSection(&m_criticalSection);
}

HRESULT RuntimeCompatibilityLayer::Initialize(FrameworkVersion targetVersion) {
    m_targetVersion = targetVersion;
    
    EnterCriticalSection(&m_criticalSection);
    m_configuredProcesses.clear();
    LeaveCriticalSection(&m_criticalSection);
    
    return S_OK;
}

void RuntimeCompatibilityLayer::Cleanup() {
    EnterCriticalSection(&m_criticalSection);
    m_configuredProcesses.clear();
    LeaveCriticalSection(&m_criticalSection);
}

HRESULT RuntimeCompatibilityLayer::ConfigureGCCompatibility(DWORD processId) {
    // Configure garbage collection to match target framework behavior
    switch (m_targetVersion) {
        case FrameworkVersion::NetFramework20:
            // .NET 2.0 GC behavior: workstation GC, non-concurrent by default
            SetGCMode(processId, false, false);
            break;
            
        case FrameworkVersion::NetFramework35:
            // .NET 3.5 GC behavior: introduce concurrent GC option
            SetGCMode(processId, false, true);
            break;
            
        case FrameworkVersion::NetFramework40:
            // .NET 4.0 GC behavior: background GC, improved server GC
            SetGCMode(processId, true, true);
            break;
            
        default:
            // Default to modern GC settings
            SetGCMode(processId, true, true);
            break;
    }
    
    EnterCriticalSection(&m_criticalSection);
    m_configuredProcesses[processId] = true;
    LeaveCriticalSection(&m_criticalSection);
    
    return S_OK;
}

HRESULT RuntimeCompatibilityLayer::SetGCMode(DWORD processId, bool serverGC, bool concurrentGC) {
    // In real implementation, this would configure the GC through runtime APIs
    // For now, simulate the configuration
    return S_OK;
}

HRESULT RuntimeCompatibilityLayer::ConfigureThreadingCompatibility(DWORD processId) {
    // Configure threading model based on target framework
    switch (m_targetVersion) {
        case FrameworkVersion::NetFramework20:
            // .NET 2.0 threading: basic thread pool, 25 threads per processor
            SetThreadPoolSettings(processId, 1, 25);
            break;
            
        case FrameworkVersion::NetFramework35:
            // .NET 3.5 threading: improved thread pool management
            SetThreadPoolSettings(processId, 2, 50);
            break;
            
        case FrameworkVersion::NetFramework40:
            // .NET 4.0 threading: Task-based async, work-stealing queues
            SetThreadPoolSettings(processId, 4, 100);
            break;
            
        default:
            // Modern threading settings
            SetThreadPoolSettings(processId, 8, 200);
            break;
    }
    
    return S_OK;
}

HRESULT RuntimeCompatibilityLayer::SetThreadPoolSettings(DWORD processId, 
                                                        UINT32 minThreads, UINT32 maxThreads) {
    // In real implementation, this would use ThreadPool APIs
    return S_OK;
}

// AssemblyRedirector Implementation
AssemblyRedirector::AssemblyRedirector() {
    InitializeCriticalSection(&m_criticalSection);
}

AssemblyRedirector::~AssemblyRedirector() {
    Cleanup();
    DeleteCriticalSection(&m_criticalSection);
}

HRESULT AssemblyRedirector::Initialize() {
    EnterCriticalSection(&m_criticalSection);
    m_redirections.clear();
    m_processRedirections.clear();
    LeaveCriticalSection(&m_criticalSection);
    
    return S_OK;
}

void AssemblyRedirector::Cleanup() {
    EnterCriticalSection(&m_criticalSection);
    m_redirections.clear();
    m_processRedirections.clear();
    LeaveCriticalSection(&m_criticalSection);
}

HRESULT AssemblyRedirector::AddAssemblyRedirection(const AssemblyRedirection& redirection) {
    EnterCriticalSection(&m_criticalSection);
    
    std::wstring key = redirection.legacyAssemblyName + L"_" + redirection.legacyVersion;
    m_redirections[key] = redirection;
    
    LeaveCriticalSection(&m_criticalSection);
    return S_OK;
}

HRESULT AssemblyRedirector::ApplyFrameworkRedirections(FrameworkVersion fromVersion, 
                                                      FrameworkVersion toVersion) {
    // Create common framework redirections
    std::vector<AssemblyRedirection> redirections;
    
    // mscorlib redirections
    AssemblyRedirection mscorlibRedirection;
    mscorlibRedirection.legacyAssemblyName = L"mscorlib";
    mscorlibRedirection.modernAssemblyName = L"System.Private.CoreLib";
    mscorlibRedirection.isGlobalRedirection = true;
    
    switch (fromVersion) {
        case FrameworkVersion::NetFramework20:
            mscorlibRedirection.legacyVersion = L"2.0.0.0";
            mscorlibRedirection.publicKeyToken = L"b77a5c561934e089";
            break;
        case FrameworkVersion::NetFramework40:
            mscorlibRedirection.legacyVersion = L"4.0.0.0";
            mscorlibRedirection.publicKeyToken = L"b77a5c561934e089";
            break;
        default:
            break;
    }
    
    mscorlibRedirection.modernVersion = L"5.0.0.0";
    redirections.push_back(mscorlibRedirection);
    
    // System.dll redirections
    AssemblyRedirection systemRedirection;
    systemRedirection.legacyAssemblyName = L"System";
    systemRedirection.legacyVersion = mscorlibRedirection.legacyVersion;
    systemRedirection.modernAssemblyName = L"System.Runtime";
    systemRedirection.modernVersion = L"5.0.0.0";
    systemRedirection.publicKeyToken = L"b03f5f7f11d50a3a";
    systemRedirection.isGlobalRedirection = true;
    redirections.push_back(systemRedirection);
    
    // Apply all redirections
    for (const auto& redirection : redirections) {
        AddAssemblyRedirection(redirection);
    }
    
    return S_OK;
}

HRESULT AssemblyRedirector::ResolveAssembly(const std::wstring& assemblyName, 
                                           const std::wstring& version, 
                                           std::wstring& resolvedPath) {
    EnterCriticalSection(&m_criticalSection);
    
    std::wstring key = assemblyName + L"_" + version;
    auto it = m_redirections.find(key);
    
    if (it != m_redirections.end()) {
        // Found a redirection
        resolvedPath = it->second.modernAssemblyName;
        LeaveCriticalSection(&m_criticalSection);
        return S_OK;
    }
    
    LeaveCriticalSection(&m_criticalSection);
    
    // No redirection found, use original
    resolvedPath = assemblyName;
    return S_FALSE;
}

// TypeCompatibilityManager Implementation
TypeCompatibilityManager::TypeCompatibilityManager() {
    InitializeCriticalSection(&m_criticalSection);
}

TypeCompatibilityManager::~TypeCompatibilityManager() {
    Cleanup();
    DeleteCriticalSection(&m_criticalSection);
}

HRESULT TypeCompatibilityManager::Initialize() {
    EnterCriticalSection(&m_criticalSection);
    m_typeMappings.clear();
    m_createdShims.clear();
    LeaveCriticalSection(&m_criticalSection);
    
    return S_OK;
}

void TypeCompatibilityManager::Cleanup() {
    EnterCriticalSection(&m_criticalSection);
    
    // Cleanup created shims
    for (const auto& shimPair : m_createdShims) {
        if (shimPair.second) {
            // Free shim resources
        }
    }
    
    m_typeMappings.clear();
    m_createdShims.clear();
    LeaveCriticalSection(&m_criticalSection);
}

HRESULT TypeCompatibilityManager::AddTypeMapping(const TypeCompatibilityMapping& mapping) {
    EnterCriticalSection(&m_criticalSection);
    
    std::wstring key = mapping.legacyTypeName + L"@" + mapping.legacyAssembly;
    m_typeMappings[key] = mapping;
    
    LeaveCriticalSection(&m_criticalSection);
    return S_OK;
}

HRESULT TypeCompatibilityManager::ApplyFrameworkTypeMappings(FrameworkVersion frameworkVersion) {
    // Create common type mappings for framework types
    std::vector<TypeCompatibilityMapping> mappings;
    
    // String type mapping
    TypeCompatibilityMapping stringMapping;
    stringMapping.legacyTypeName = L"System.String";
    stringMapping.legacyAssembly = L"mscorlib";
    stringMapping.modernTypeName = L"System.String";
    stringMapping.modernAssembly = L"System.Private.CoreLib";
    stringMapping.requiresShim = false;
    mappings.push_back(stringMapping);
    
    // Object type mapping
    TypeCompatibilityMapping objectMapping;
    objectMapping.legacyTypeName = L"System.Object";
    objectMapping.legacyAssembly = L"mscorlib";
    objectMapping.modernTypeName = L"System.Object";
    objectMapping.modernAssembly = L"System.Private.CoreLib";
    objectMapping.requiresShim = false;
    mappings.push_back(objectMapping);
    
    // Apply version-specific mappings
    switch (frameworkVersion) {
        case FrameworkVersion::NetFramework20:
            {
                TypeCompatibilityMapping genericMapping;
                genericMapping.legacyTypeName = L"System.Collections.Generic.List`1";
                genericMapping.legacyAssembly = L"mscorlib";
                genericMapping.modernTypeName = L"System.Collections.Generic.List`1";
                genericMapping.modernAssembly = L"System.Collections";
                genericMapping.requiresShim = false;
                mappings.push_back(genericMapping);
            }
            break;
        default:
            break;
    }
    
    // Apply all mappings
    for (const auto& mapping : mappings) {
        AddTypeMapping(mapping);
    }
    
    return S_OK;
}

// CompatibilityShim Implementation
CompatibilityShim::CompatibilityShim() : m_initialized(false) {
    InitializeCriticalSection(&m_criticalSection);
}

CompatibilityShim::~CompatibilityShim() {
    Cleanup();
    DeleteCriticalSection(&m_criticalSection);
}

HRESULT CompatibilityShim::Initialize() {
    if (m_initialized) {
        return S_OK;
    }
    
    HRESULT hr = InitializeComponents();
    if (FAILED(hr)) {
        return hr;
    }
    
    m_initialized = true;
    return S_OK;
}

HRESULT CompatibilityShim::InitializeComponents() {
    // Initialize API shim
    m_apiShim = std::make_unique<LegacyApiShim>();
    HRESULT hr = m_apiShim->Initialize(CompatibilityLevel::Standard);
    if (FAILED(hr)) {
        return hr;
    }
    
    // Initialize runtime compatibility layer
    m_runtimeLayer = std::make_unique<RuntimeCompatibilityLayer>();
    hr = m_runtimeLayer->Initialize(FrameworkVersion::NetFramework40);
    if (FAILED(hr)) {
        return hr;
    }
    
    // Initialize assembly redirector
    m_assemblyRedirector = std::make_unique<AssemblyRedirector>();
    hr = m_assemblyRedirector->Initialize();
    if (FAILED(hr)) {
        return hr;
    }
    
    // Initialize type compatibility manager
    m_typeManager = std::make_unique<TypeCompatibilityManager>();
    hr = m_typeManager->Initialize();
    if (FAILED(hr)) {
        return hr;
    }
    
    return S_OK;
}

void CompatibilityShim::Cleanup() {
    if (!m_initialized) {
        return;
    }
    
    EnterCriticalSection(&m_criticalSection);
    
    // Cleanup components
    if (m_apiShim) {
        m_apiShim->Cleanup();
        m_apiShim.reset();
    }
    
    if (m_runtimeLayer) {
        m_runtimeLayer->Cleanup();
        m_runtimeLayer.reset();
    }
    
    if (m_assemblyRedirector) {
        m_assemblyRedirector->Cleanup();
        m_assemblyRedirector.reset();
    }
    
    if (m_typeManager) {
        m_typeManager->Cleanup();
        m_typeManager.reset();
    }
    
    // Clear process tracking
    m_processCompatibility.clear();
    m_processVersions.clear();
    m_processShims.clear();
    m_compatibilityProfiles.clear();
    
    m_initialized = false;
    
    LeaveCriticalSection(&m_criticalSection);
}

HRESULT CompatibilityShim::ApplyShimsToProcess(DWORD processId) {
    if (!m_initialized) {
        return E_NOT_VALID_STATE;
    }
    
    // Detect framework version first
    FrameworkVersion version;
    HRESULT hr = DetectFrameworkVersion(processId, version);
    if (FAILED(hr)) {
        version = FrameworkVersion::NetFramework40; // Default assumption
    }
    
    // Apply compatibility adaptations
    hr = AdaptToFrameworkVersion(processId, version);
    if (FAILED(hr)) {
        return hr;
    }
    
    // Apply shim types
    hr = ApplyShimType(processId, ShimType::ApiRedirection);
    if (FAILED(hr)) {
        return hr;
    }
    
    hr = ApplyShimType(processId, ShimType::AssemblyRedirection);
    if (FAILED(hr)) {
        return hr;
    }
    
    hr = ApplyShimType(processId, ShimType::TypeMapping);
    if (FAILED(hr)) {
        return hr;
    }
    
    hr = ApplyShimType(processId, ShimType::RuntimeBehavior);
    if (FAILED(hr)) {
        return hr;
    }
    
    // Track process
    EnterCriticalSection(&m_criticalSection);
    m_processCompatibility[processId] = CompatibilityLevel::Standard;
    m_processVersions[processId] = version;
    LeaveCriticalSection(&m_criticalSection);
    
    return S_OK;
}

HRESULT CompatibilityShim::RemoveShimsFromProcess(DWORD processId) {
    EnterCriticalSection(&m_criticalSection);
    
    // Remove process tracking
    m_processCompatibility.erase(processId);
    m_processVersions.erase(processId);
    m_processShims.erase(processId);
    
    LeaveCriticalSection(&m_criticalSection);
    
    // In real implementation, this would remove actual shims from the process
    return S_OK;
}

HRESULT CompatibilityShim::DetectFrameworkVersion(DWORD processId, FrameworkVersion& version) {
    // Open process to examine modules
    HANDLE processHandle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, 
                                      FALSE, processId);
    if (!processHandle) {
        return HRESULT_FROM_WIN32(GetLastError());
    }
    
    // Enumerate loaded modules to detect .NET version
    HMODULE modules[1024];
    DWORD needed;
    
    if (EnumProcessModules(processHandle, modules, sizeof(modules), &needed)) {
        DWORD moduleCount = needed / sizeof(HMODULE);
        
        for (DWORD i = 0; i < moduleCount; i++) {
            WCHAR moduleName[MAX_PATH];
            if (GetModuleBaseNameW(processHandle, modules[i], moduleName, MAX_PATH)) {
                // Check for CLR version indicators
                std::wstring moduleNameStr = moduleName;
                std::transform(moduleNameStr.begin(), moduleNameStr.end(),
                              moduleNameStr.begin(), ::towlower);
                
                if (moduleNameStr.find(L"mscorwks") != std::wstring::npos) {
                    version = FrameworkVersion::NetFramework20; // .NET 2.0-3.5
                    CloseHandle(processHandle);
                    return S_OK;
                } else if (moduleNameStr.find(L"clr.dll") != std::wstring::npos) {
                    version = FrameworkVersion::NetFramework40; // .NET 4.0+
                    CloseHandle(processHandle);
                    return S_OK;
                }
            }
        }
    }
    
    CloseHandle(processHandle);
    
    // Default to .NET 4.0 if not detected
    version = FrameworkVersion::NetFramework40;
    return S_FALSE;
}

HRESULT CompatibilityShim::AdaptToFrameworkVersion(DWORD processId, FrameworkVersion detectedVersion) {
    // Configure runtime compatibility
    HRESULT hr = m_runtimeLayer->ConfigureGCCompatibility(processId);
    if (FAILED(hr)) {
        return hr;
    }
    
    hr = m_runtimeLayer->ConfigureThreadingCompatibility(processId);
    if (FAILED(hr)) {
        return hr;
    }
    
    // Apply framework redirections
    hr = m_assemblyRedirector->ApplyFrameworkRedirections(detectedVersion, FrameworkVersion::NetFramework48);
    if (FAILED(hr)) {
        return hr;
    }
    
    // Apply type mappings
    hr = m_typeManager->ApplyFrameworkTypeMappings(detectedVersion);
    if (FAILED(hr)) {
        return hr;
    }
    
    return S_OK;
}

HRESULT CompatibilityShim::ApplyShimType(DWORD processId, ShimType shimType) {
    // Apply specific shim type based on process needs
    switch (shimType) {
        case ShimType::ApiRedirection:
            return m_apiShim->ShimRuntimeApis(processId);
            
        case ShimType::AssemblyRedirection:
            // Assembly redirections are applied globally
            return S_OK;
            
        case ShimType::TypeMapping:
            // Type mappings are applied during type resolution
            return S_OK;
            
        case ShimType::RuntimeBehavior:
            return m_runtimeLayer->ConfigureGCCompatibility(processId);
            
        default:
            return S_OK;
    }
}

// Factory Implementation
std::unique_ptr<CompatibilityShim> 
CompatibilityFactory::CreateCompatibilityShim(const CompatibilityConfig& config) {
    auto shim = std::make_unique<CompatibilityShim>();
    
    HRESULT hr = shim->Initialize();
    if (FAILED(hr)) {
        return nullptr;
    }
    
    return shim;
}

CompatibilityConfig CompatibilityFactory::CreateNetFramework40Config() {
    CompatibilityConfig config;
    config.defaultLevel = CompatibilityLevel::Standard;
    config.enabledShims = {
        ShimType::ApiRedirection,
        ShimType::AssemblyRedirection,
        ShimType::TypeMapping,
        ShimType::RuntimeBehavior
    };
    config.enableAutomaticDetection = true;
    config.enablePerformanceOptimizations = true;
    config.shimTimeoutMs = 10000;
    
    return config;
}

CompatibilityConfig CompatibilityFactory::CreateMinimalCompatibilityConfig() {
    CompatibilityConfig config;
    config.defaultLevel = CompatibilityLevel::Minimal;
    config.enabledShims = {ShimType::ApiRedirection};
    config.enableAutomaticDetection = false;
    config.enablePerformanceOptimizations = true;
    config.shimTimeoutMs = 5000;
    
    return config;
}

bool CompatibilityFactory::ValidateCompatibilityConfig(const CompatibilityConfig& config) {
    // Validate timeout
    if (config.shimTimeoutMs > 60000) {
        return false;
    }
    
    // Validate shim types
    if (config.enabledShims.empty()) {
        return false;
    }
    
    return true;
}

} // namespace System
} // namespace CLRNet