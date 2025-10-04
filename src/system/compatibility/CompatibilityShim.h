#pragma once

#include <windows.h>
#include <vector>
#include <map>
#include <string>
#include <memory>
#include <functional>

namespace CLRNet {
namespace System {

// Forward declarations
class LegacyApiShim;
class RuntimeCompatibilityLayer;
class AssemblyRedirector;
class TypeCompatibilityManager;

// Compatibility shim types
enum class ShimType : UINT32 {
    None = 0,
    ApiRedirection = 0x01,
    TypeMapping = 0x02,
    AssemblyRedirection = 0x04,
    RuntimeBehavior = 0x08,
    SecurityPolicy = 0x10,
    GarbageCollection = 0x20,
    Threading = 0x40,
    All = 0xFF
};

// Compatibility levels
enum class CompatibilityLevel : UINT32 {
    None,           // No compatibility
    Minimal,        // Basic API compatibility
    Standard,       // Standard .NET Framework compatibility
    Extended,       // Enhanced compatibility with legacy features
    Full            // Maximum compatibility including deprecated features
};

// Legacy .NET Framework versions
enum class FrameworkVersion : UINT32 {
    Unknown,
    NetFramework20,
    NetFramework35,
    NetFramework40,
    NetFramework45,
    NetFramework46,
    NetFramework47,
    NetFramework48
};

// Shim installation information
struct ShimInstallationInfo {
    ShimType type;
    std::wstring targetApi;
    std::wstring modernApi;
    PVOID originalFunction;
    PVOID shimFunction;
    bool isInstalled;
    FILETIME installTime;
    DWORD processId;
    std::wstring description;
};

// Assembly redirection information
struct AssemblyRedirection {
    std::wstring legacyAssemblyName;
    std::wstring legacyVersion;
    std::wstring modernAssemblyName;
    std::wstring modernVersion;
    std::wstring publicKeyToken;
    bool isGlobalRedirection;
};

// Type compatibility mapping
struct TypeCompatibilityMapping {
    std::wstring legacyTypeName;
    std::wstring legacyAssembly;
    std::wstring modernTypeName;
    std::wstring modernAssembly;
    bool requiresShim;
    PVOID shimFunction;
};

// Legacy API shim for individual API calls
class LegacyApiShim {
public:
    LegacyApiShim();
    ~LegacyApiShim();

    // Initialize API shimming
    HRESULT Initialize(CompatibilityLevel level);
    void Cleanup();

    // API redirection management
    HRESULT InstallApiShim(const std::wstring& apiName, PVOID legacyFunction, PVOID modernFunction);
    HRESULT RemoveApiShim(const std::wstring& apiName);
    
    // Bulk API shimming
    HRESULT InstallFrameworkShims(FrameworkVersion frameworkVersion);
    HRESULT InstallCommonApiShims();

    // Runtime API compatibility
    HRESULT ShimRuntimeApis(DWORD processId);
    HRESULT ShimGCApis(DWORD processId);
    HRESULT ShimThreadingApis(DWORD processId);

    // Shim information
    std::vector<ShimInstallationInfo> GetInstalledShims() const;
    bool IsApiShimmed(const std::wstring& apiName) const;

private:
    CRITICAL_SECTION m_criticalSection;
    CompatibilityLevel m_compatibilityLevel;
    std::map<std::wstring, ShimInstallationInfo> m_installedShims;
    
    // Internal shimming
    HRESULT InstallSingleShim(const std::wstring& apiName, PVOID original, PVOID replacement);
    HRESULT CreateApiThunk(PVOID originalFunction, PVOID shimFunction, PVOID* thunkFunction);
};

// Runtime compatibility layer for behavioral differences
class RuntimeCompatibilityLayer {
public:
    RuntimeCompatibilityLayer();
    ~RuntimeCompatibilityLayer();

    // Initialize runtime compatibility
    HRESULT Initialize(FrameworkVersion targetVersion);
    void Cleanup();

    // Garbage collection compatibility
    HRESULT ConfigureGCCompatibility(DWORD processId);
    HRESULT SetGCMode(DWORD processId, bool serverGC, bool concurrentGC);
    
    // Threading model compatibility
    HRESULT ConfigureThreadingCompatibility(DWORD processId);
    HRESULT SetThreadPoolSettings(DWORD processId, UINT32 minThreads, UINT32 maxThreads);

    // Exception handling compatibility
    HRESULT ConfigureExceptionHandling(DWORD processId);
    HRESULT EnableStructuredExceptionHandling(DWORD processId, bool enable);

    // Security policy compatibility
    HRESULT ConfigureSecurityCompatibility(DWORD processId);
    HRESULT ApplyLegacySecurityPolicy(DWORD processId, const std::wstring& policyFile);

    // Application domain compatibility
    HRESULT ConfigureAppDomainCompatibility(DWORD processId);
    HRESULT EnableLegacyAppDomainBehavior(DWORD processId, bool enable);

private:
    CRITICAL_SECTION m_criticalSection;
    FrameworkVersion m_targetVersion;
    std::map<DWORD, bool> m_configuredProcesses;
    
    // Configuration helpers
    HRESULT ApplyVersionSpecificBehavior(DWORD processId, FrameworkVersion version);
    HRESULT ConfigureRuntimeFlags(DWORD processId, const std::map<std::wstring, bool>& flags);
};

// Assembly redirection and loading compatibility
class AssemblyRedirector {
public:
    AssemblyRedirector();
    ~AssemblyRedirector();

    // Initialize assembly redirection
    HRESULT Initialize();
    void Cleanup();

    // Redirection management
    HRESULT AddAssemblyRedirection(const AssemblyRedirection& redirection);
    HRESULT RemoveAssemblyRedirection(const std::wstring& legacyAssemblyName);
    
    // Bulk redirections
    HRESULT LoadRedirectionsFromConfig(const std::wstring& configFile);
    HRESULT ApplyFrameworkRedirections(FrameworkVersion fromVersion, FrameworkVersion toVersion);

    // Assembly resolution
    HRESULT ResolveAssembly(const std::wstring& assemblyName, const std::wstring& version, 
                           std::wstring& resolvedPath);
    HRESULT InterceptAssemblyLoad(DWORD processId, const std::wstring& assemblyName, 
                                 std::wstring& redirectedName);

    // Redirection information
    std::vector<AssemblyRedirection> GetActiveRedirections() const;
    bool IsAssemblyRedirected(const std::wstring& assemblyName) const;

private:
    CRITICAL_SECTION m_criticalSection;
    std::map<std::wstring, AssemblyRedirection> m_redirections;
    std::map<DWORD, std::vector<std::wstring>> m_processRedirections;
    
    // Internal redirection logic
    HRESULT ParseAssemblyName(const std::wstring& fullName, std::wstring& name, 
                             std::wstring& version, std::wstring& publicKey);
    HRESULT FindBestMatch(const std::wstring& requestedName, const std::wstring& requestedVersion,
                         AssemblyRedirection& bestMatch);
};

// Type compatibility and marshaling
class TypeCompatibilityManager {
public:
    TypeCompatibilityManager();
    ~TypeCompatibilityManager();

    // Initialize type compatibility
    HRESULT Initialize();
    void Cleanup();

    // Type mapping management
    HRESULT AddTypeMapping(const TypeCompatibilityMapping& mapping);
    HRESULT RemoveTypeMapping(const std::wstring& legacyTypeName);
    
    // Bulk type mappings
    HRESULT LoadTypeMappingsFromConfig(const std::wstring& configFile);
    HRESULT ApplyFrameworkTypeMappings(FrameworkVersion frameworkVersion);

    // Type resolution and marshaling
    HRESULT ResolveType(const std::wstring& legacyTypeName, const std::wstring& legacyAssembly,
                       std::wstring& modernTypeName, std::wstring& modernAssembly);
    
    HRESULT CreateTypeShim(const std::wstring& legacyTypeName, PVOID legacyTypeHandle,
                          PVOID modernTypeHandle, PVOID* shimHandle);

    // Interface compatibility
    HRESULT MapInterface(PVOID legacyInterface, const std::wstring& modernInterfaceName,
                        PVOID* modernInterface);
    
    HRESULT CreateInterfaceProxy(PVOID legacyObject, const std::wstring& targetInterface,
                               PVOID* proxyObject);

    // Type information
    std::vector<TypeCompatibilityMapping> GetTypeMappings() const;
    bool IsTypeRedirected(const std::wstring& typeName) const;

private:
    CRITICAL_SECTION m_criticalSection;
    std::map<std::wstring, TypeCompatibilityMapping> m_typeMappings;
    std::map<std::wstring, PVOID> m_createdShims;
    
    // Internal type operations
    HRESULT CreateMethodThunks(PVOID legacyType, PVOID modernType, PVOID shimType);
    HRESULT MapMethodSignature(PVOID legacyMethod, PVOID modernMethod);
};

// Main compatibility shim coordinator
class CompatibilityShim {
public:
    CompatibilityShim();
    ~CompatibilityShim();

    // Initialize compatibility system
    HRESULT Initialize();
    void Cleanup();

    // Process-level shim application
    HRESULT ApplyShimsToProcess(DWORD processId);
    HRESULT RemoveShimsFromProcess(DWORD processId);
    
    // Configuration-based shimming
    HRESULT ApplyCompatibilityProfile(DWORD processId, const std::wstring& profileName);
    HRESULT LoadCompatibilityConfig(const std::wstring& configFile);

    // Framework version detection and adaptation
    HRESULT DetectFrameworkVersion(DWORD processId, FrameworkVersion& version);
    HRESULT AdaptToFrameworkVersion(DWORD processId, FrameworkVersion detectedVersion);

    // Component access
    std::unique_ptr<LegacyApiShim> GetApiShim() const;
    std::unique_ptr<RuntimeCompatibilityLayer> GetRuntimeLayer() const;
    std::unique_ptr<AssemblyRedirector> GetAssemblyRedirector() const;
    std::unique_ptr<TypeCompatibilityManager> GetTypeManager() const;

    // Monitoring and health
    HRESULT ValidateShimIntegrity(DWORD processId) const;
    HRESULT GetCompatibilityStatus(DWORD processId, std::map<std::wstring, bool>& status) const;
    
    // Rollback capabilities
    HRESULT CreateShimSnapshot(DWORD processId);
    HRESULT RollbackToSnapshot(DWORD processId);

private:
    CRITICAL_SECTION m_criticalSection;
    bool m_initialized;
    
    // Component managers
    std::unique_ptr<LegacyApiShim> m_apiShim;
    std::unique_ptr<RuntimeCompatibilityLayer> m_runtimeLayer;
    std::unique_ptr<AssemblyRedirector> m_assemblyRedirector;
    std::unique_ptr<TypeCompatibilityManager> m_typeManager;
    
    // Process tracking
    std::map<DWORD, CompatibilityLevel> m_processCompatibility;
    std::map<DWORD, FrameworkVersion> m_processVersions;
    std::map<DWORD, std::vector<ShimInstallationInfo>> m_processShims;
    
    // Configuration and profiles
    std::map<std::wstring, CompatibilityLevel> m_compatibilityProfiles;
    
    // Internal operations
    HRESULT InitializeComponents();
    HRESULT ApplyShimType(DWORD processId, ShimType shimType);
    HRESULT DetectLegacyApis(DWORD processId, std::vector<std::wstring>& legacyApis);
};

// Compatibility configuration structures
struct CompatibilityConfig {
    CompatibilityLevel defaultLevel;
    std::vector<ShimType> enabledShims;
    std::vector<AssemblyRedirection> assemblyRedirections;
    std::vector<TypeCompatibilityMapping> typeMappings;
    std::map<std::wstring, std::wstring> apiRedirections;
    bool enableAutomaticDetection;
    bool enablePerformanceOptimizations;
    UINT32 shimTimeoutMs;
    std::vector<std::wstring> excludedProcesses;
};

// Factory for creating compatibility components
class CompatibilityFactory {
public:
    // Create compatibility shim with configuration
    static std::unique_ptr<CompatibilityShim> CreateCompatibilityShim(const CompatibilityConfig& config);
    
    // Create individual components
    static std::unique_ptr<LegacyApiShim> CreateApiShim(CompatibilityLevel level);
    static std::unique_ptr<RuntimeCompatibilityLayer> CreateRuntimeLayer(FrameworkVersion targetVersion);
    static std::unique_ptr<AssemblyRedirector> CreateAssemblyRedirector();
    static std::unique_ptr<TypeCompatibilityManager> CreateTypeManager();
    
    // Create predefined configurations
    static CompatibilityConfig CreateNetFramework40Config();
    static CompatibilityConfig CreateNetFramework45Config();
    static CompatibilityConfig CreateMinimalCompatibilityConfig();
    static CompatibilityConfig CreateMaximalCompatibilityConfig();
    
    // Validate configuration
    static bool ValidateCompatibilityConfig(const CompatibilityConfig& config);
    
    // Load configuration from file
    static HRESULT LoadConfigFromFile(const std::wstring& configPath, CompatibilityConfig& config);
};

} // namespace System
} // namespace CLRNet