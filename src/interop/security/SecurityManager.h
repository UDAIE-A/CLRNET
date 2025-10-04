#pragma once

#include <windows.h>
#include <wincrypt.h>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <functional>

namespace CLRNet {
namespace Interop {

// System capability enumeration (matches Windows Phone 8.1 manifest)
enum class SystemCapability {
    // Network capabilities
    InternetClient,
    InternetClientServer,
    PrivateNetworkClientServer,
    
    // Device capabilities
    Location,
    Webcam,
    Microphone,
    MusicLibrary,
    PicturesLibrary,
    VideosLibrary,
    RemovableStorage,
    
    // Communication capabilities
    PhoneDialer,
    SMS,
    Contacts,
    Calendar,
    
    // System capabilities
    AppointmentsSystem,
    ContactsSystem,
    EmailSystem,
    GameBarServices,
    
    // Hardware capabilities
    Bluetooth,
    WiFiControl,
    
    // Enterprise capabilities
    EnterpriseAuthentication,
    SharedUserCertificates
};

// Security context levels
enum class SecurityLevel {
    Untrusted,      // No system access
    Partial,        // Limited system access with prompts
    Trusted,        // Full declared capabilities
    System,         // System-level access (not for apps)
    Administrator   // Full system access (not for apps)
};

// Sandboxing enforcement levels
enum class SandboxLevel {
    None,          // No sandboxing (dangerous)
    Basic,         // Basic file system isolation
    Standard,      // Standard app isolation
    Enhanced,      // Enhanced isolation with restricted API access
    Maximum        // Maximum isolation (very limited functionality)
};

// Permission prompt result
enum class PermissionPromptResult {
    Allow,
    Deny,
    AlwaysAllow,
    AlwaysDeny,
    Cancel
};

// Security violation types
enum class SecurityViolationType {
    UnauthorizedAPICall,
    FileSystemViolation,
    NetworkViolation,
    CapabilityViolation,
    SandboxViolation,
    PermissionViolation
};

// Forward declarations
class CapabilityManager;
class SandboxManager;
class SecurityEnforcer;
class PermissionPromptManager;

// Capability declaration and management
struct CapabilityDeclaration {
    SystemCapability capability;
    bool isRequired;
    std::wstring justification;
    bool userConsent;
    FILETIME declaredTime;
    std::wstring version;
};

// Security context for runtime enforcement
struct SecurityContext {
    SecurityLevel level;
    SandboxLevel sandboxLevel;
    std::set<SystemCapability> declaredCapabilities;
    std::set<SystemCapability> grantedCapabilities;
    std::wstring applicationId;
    std::wstring publisherId;
    bool isDebugging;
    FILETIME createdTime;
};

// Security violation record
struct SecurityViolation {
    SecurityViolationType type;
    std::wstring description;
    std::wstring source;
    SystemCapability attemptedCapability;
    FILETIME timestamp;
    bool wasBlocked;
    std::wstring callStack;
};

// Permission prompt information
struct PermissionPrompt {
    SystemCapability capability;
    std::wstring applicationName;
    std::wstring message;
    std::wstring detailedReason;
    bool isOneTime;
    bool canRemember;
    std::function<void(PermissionPromptResult)> callback;
};

// Capability manager - handles capability declarations and validation
class CapabilityManager {
private:
    std::map<std::wstring, std::vector<CapabilityDeclaration>> m_applicationCapabilities;
    std::set<SystemCapability> m_systemRestrictedCapabilities;
    CRITICAL_SECTION m_criticalSection;

public:
    CapabilityManager();
    ~CapabilityManager();
    
    // Initialize capability management
    HRESULT Initialize();
    
    // Load application capabilities from manifest
    HRESULT LoadApplicationCapabilities(const std::wstring& applicationId,
                                      const std::wstring& manifestPath);
    
    // Declare capability programmatically
    HRESULT DeclareCapability(const std::wstring& applicationId,
                             SystemCapability capability,
                             const std::wstring& justification,
                             bool isRequired = true);
    
    // Check if capability is declared
    bool IsCapabilityDeclared(const std::wstring& applicationId,
                             SystemCapability capability) const;
    
    // Get declared capabilities for application
    std::vector<SystemCapability> GetDeclaredCapabilities(const std::wstring& applicationId) const;
    
    // Validate capability declaration
    HRESULT ValidateCapabilityDeclaration(const CapabilityDeclaration& declaration);
    
    // Get capability requirements (user consent, admin approval, etc.)
    HRESULT GetCapabilityRequirements(SystemCapability capability,
                                     bool* requiresUserConsent,
                                     bool* requiresAdminApproval,
                                     bool* requiresSystemAccess);
    
    // Check if capability is system-restricted
    bool IsSystemRestricted(SystemCapability capability) const;
    
private:
    // Internal capability validation
    HRESULT ParseManifestFile(const std::wstring& manifestPath,
                             std::vector<CapabilityDeclaration>* capabilities);
    void InitializeSystemRestrictions();
    std::wstring GetCapabilityName(SystemCapability capability) const;
};

// Sandbox manager - enforces application isolation
class SandboxManager {
private:
    std::map<std::wstring, SandboxLevel> m_applicationSandboxLevels;
    std::set<std::wstring> m_allowedPaths;
    std::set<std::wstring> m_blockedPaths;
    std::set<std::wstring> m_allowedRegistryKeys;
    CRITICAL_SECTION m_criticalSection;

public:
    SandboxManager();
    ~SandboxManager();
    
    // Initialize sandbox system
    HRESULT Initialize();
    
    // Set sandbox level for application
    HRESULT SetSandboxLevel(const std::wstring& applicationId, SandboxLevel level);
    
    // Get current sandbox level
    SandboxLevel GetSandboxLevel(const std::wstring& applicationId) const;
    
    // File system access control
    bool IsFilePathAllowed(const std::wstring& applicationId,
                          const std::wstring& filePath,
                          DWORD desiredAccess) const;
    
    // Registry access control
    bool IsRegistryKeyAllowed(const std::wstring& applicationId,
                             const std::wstring& keyPath,
                             DWORD desiredAccess) const;
    
    // Network access control
    bool IsNetworkAccessAllowed(const std::wstring& applicationId,
                               const std::wstring& hostname,
                               UINT16 port) const;
    
    // Process creation control
    bool IsProcessCreationAllowed(const std::wstring& applicationId,
                                 const std::wstring& executablePath) const;
    
    // Add allowed/blocked paths
    HRESULT AddAllowedPath(const std::wstring& path);
    HRESULT AddBlockedPath(const std::wstring& path);
    
    // Configure sandbox isolation
    HRESULT ConfigureFileSystemIsolation(const std::wstring& applicationId);
    HRESULT ConfigureNetworkIsolation(const std::wstring& applicationId);
    HRESULT ConfigureRegistryIsolation(const std::wstring& applicationId);

private:
    // Internal path validation
    bool IsPathInAllowedList(const std::wstring& path) const;
    bool IsPathInBlockedList(const std::wstring& path) const;
    std::wstring NormalizePath(const std::wstring& path) const;
    
    // Isolation helpers
    HRESULT CreateAppDataContainer(const std::wstring& applicationId);
    HRESULT SetupFileSystemRedirection(const std::wstring& applicationId);
};

// Permission prompt manager - handles user consent dialogs
class PermissionPromptManager {
private:
    std::map<SystemCapability, PermissionPromptResult> m_rememberedChoices;
    bool m_allowPrompts;
    CRITICAL_SECTION m_criticalSection;

public:
    PermissionPromptManager();
    ~PermissionPromptManager();
    
    // Initialize prompt system
    HRESULT Initialize();
    
    // Show permission prompt to user
    HRESULT ShowPermissionPrompt(const PermissionPrompt& prompt);
    
    // Show permission prompt asynchronously
    HRESULT ShowPermissionPromptAsync(const PermissionPrompt& prompt);
    
    // Check if user has made a remembered choice
    PermissionPromptResult GetRememberedChoice(SystemCapability capability) const;
    
    // Set remembered choice
    HRESULT SetRememberedChoice(SystemCapability capability, 
                               PermissionPromptResult result);
    
    // Clear remembered choices
    HRESULT ClearRememberedChoices();
    
    // Enable/disable prompts (for automated scenarios)
    void SetPromptsEnabled(bool enabled);
    
    // Get capability display information
    std::wstring GetCapabilityDisplayName(SystemCapability capability) const;
    std::wstring GetCapabilityDescription(SystemCapability capability) const;
    std::wstring GetCapabilityRiskLevel(SystemCapability capability) const;

private:
    // Internal prompt implementation
    PermissionPromptResult ShowNativePrompt(const PermissionPrompt& prompt);
    HRESULT CreatePromptDialog(const PermissionPrompt& prompt, HWND* dialogHandle);
    
    // Prompt text generation
    std::wstring GeneratePromptMessage(const PermissionPrompt& prompt) const;
    std::wstring GenerateDetailedExplanation(SystemCapability capability) const;
};

// Security enforcer - main security policy engine
class SecurityEnforcer {
private:
    std::unique_ptr<CapabilityManager> m_capabilityManager;
    std::unique_ptr<SandboxManager> m_sandboxManager;
    std::unique_ptr<PermissionPromptManager> m_promptManager;
    
    std::map<std::wstring, SecurityContext> m_securityContexts;
    std::vector<SecurityViolation> m_violationLog;
    
    bool m_enforcementEnabled;
    SecurityLevel m_defaultSecurityLevel;
    SandboxLevel m_defaultSandboxLevel;
    
    CRITICAL_SECTION m_criticalSection;

public:
    SecurityEnforcer();
    ~SecurityEnforcer();
    
    // Initialize security enforcement
    HRESULT Initialize();
    
    // Shutdown security enforcement
    void Shutdown();
    
    // Security context management
    HRESULT CreateSecurityContext(const std::wstring& applicationId,
                                 const std::wstring& manifestPath,
                                 SecurityContext* context);
    
    HRESULT SetSecurityContext(const std::wstring& applicationId,
                              const SecurityContext& context);
    
    const SecurityContext* GetSecurityContext(const std::wstring& applicationId) const;
    
    // Capability enforcement
    HRESULT CheckCapabilityAccess(const std::wstring& applicationId,
                                 SystemCapability capability,
                                 bool promptIfNeeded = true);
    
    // System call interception
    HRESULT ValidateSystemCall(const std::wstring& applicationId,
                              const std::wstring& functionName,
                              void* parameters);
    
    // File system access validation
    HRESULT ValidateFileAccess(const std::wstring& applicationId,
                              const std::wstring& filePath,
                              DWORD desiredAccess);
    
    // Network access validation
    HRESULT ValidateNetworkAccess(const std::wstring& applicationId,
                                 const std::wstring& hostname,
                                 UINT16 port);
    
    // Registry access validation
    HRESULT ValidateRegistryAccess(const std::wstring& applicationId,
                                  const std::wstring& keyPath,
                                  DWORD desiredAccess);
    
    // Security violation handling
    HRESULT LogSecurityViolation(const std::wstring& applicationId,
                                SecurityViolationType violationType,
                                const std::wstring& description,
                                SystemCapability attemptedCapability = SystemCapability::InternetClient);
    
    // Security policy configuration
    HRESULT SetEnforcementLevel(SecurityLevel level);
    HRESULT SetDefaultSandboxLevel(SandboxLevel level);
    HRESULT EnableEnforcement(bool enabled);
    
    // Security audit and reporting
    std::vector<SecurityViolation> GetSecurityViolations(const std::wstring& applicationId) const;
    HRESULT GenerateSecurityReport(const std::wstring& filePath) const;
    
    // Emergency security controls
    HRESULT EmergencyLockdown(const std::wstring& applicationId);
    HRESULT RestoreFromLockdown(const std::wstring& applicationId);

private:
    // Internal enforcement helpers
    HRESULT EnforceCapabilityAccess(const SecurityContext& context,
                                   SystemCapability capability);
    
    HRESULT HandleCapabilityViolation(const std::wstring& applicationId,
                                     SystemCapability capability);
    
    bool IsSystemLevelOperation(const std::wstring& functionName) const;
    
    // Context validation
    HRESULT ValidateSecurityContext(const SecurityContext& context);
    void UpdateContextFromManifest(const std::wstring& manifestPath,
                                  SecurityContext* context);
};

// Security manager factory
class SecurityManagerFactory {
public:
    static SecurityEnforcer* CreateInstance();
    static void DestroyInstance(SecurityEnforcer* instance);
    
    // Create pre-configured security contexts
    static SecurityContext CreateStandardAppContext(const std::wstring& applicationId);
    static SecurityContext CreateTrustedAppContext(const std::wstring& applicationId);
    static SecurityContext CreateSandboxedContext(const std::wstring& applicationId);
    
private:
    SecurityManagerFactory() = default;
};

// Security helper utilities
class SecurityUtils {
public:
    // Capability conversion helpers
    static std::wstring CapabilityToString(SystemCapability capability);
    static SystemCapability StringToCapability(const std::wstring& capabilityName);
    
    // Security level helpers
    static std::wstring SecurityLevelToString(SecurityLevel level);
    static SecurityLevel StringToSecurityLevel(const std::wstring& levelName);
    
    // Path security helpers
    static bool IsSecurePath(const std::wstring& path);
    static std::wstring GetSecureAppDataPath(const std::wstring& applicationId);
    static bool IsSystemPath(const std::wstring& path);
    
    // Cryptographic helpers for security tokens
    static HRESULT GenerateSecurityToken(const SecurityContext& context, std::wstring* token);
    static HRESULT ValidateSecurityToken(const std::wstring& token, SecurityContext* context);
    
private:
    SecurityUtils() = default;
};

} // namespace Interop
} // namespace CLRNet