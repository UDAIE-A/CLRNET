#include "SecurityManager.h"

#include <algorithm>
#include <cwctype>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <vector>

#include <shlwapi.h>

#pragma comment(lib, "shlwapi.lib")

namespace CLRNet {
namespace Interop {

#ifndef RETURN_IF_FAILED
#define RETURN_IF_FAILED(expr)                                    \
    do {                                                         \
        HRESULT _hrTemp = (expr);                                \
        if (FAILED(_hrTemp)) {                                   \
            return _hrTemp;                                      \
        }                                                        \
    } while (0)
#endif

namespace
{
    std::wstring NormalizePathSlow(const std::wstring& path)
    {
        if (path.empty())
        {
            return std::wstring();
        }

        wchar_t buffer[MAX_PATH] = {0};
        if (PathCanonicalizeW(buffer, path.c_str()))
        {
            return std::wstring(buffer);
        }

        return path;
    }
}

// CapabilityManager Implementation
CapabilityManager::CapabilityManager()
{
    InitializeCriticalSection(&m_criticalSection);
    InitializeSystemRestrictions();
}

CapabilityManager::~CapabilityManager()
{
    DeleteCriticalSection(&m_criticalSection);
}

HRESULT CapabilityManager::Initialize()
{
    InitializeSystemRestrictions();
    return S_OK;
}

HRESULT CapabilityManager::LoadApplicationCapabilities(const std::wstring& applicationId,
                                                      const std::wstring& manifestPath)
{
    if (applicationId.empty())
    {
        return E_INVALIDARG;
    }

    std::vector<CapabilityDeclaration> capabilities;
    HRESULT hr = ParseManifestFile(manifestPath, &capabilities);
    if (FAILED(hr))
    {
        return hr;
    }

    EnterCriticalSection(&m_criticalSection);
    m_applicationCapabilities[applicationId] = capabilities;
    LeaveCriticalSection(&m_criticalSection);
    return S_OK;
}

HRESULT CapabilityManager::DeclareCapability(const std::wstring& applicationId,
                                             SystemCapability capability,
                                             const std::wstring& justification,
                                             bool isRequired)
{
    if (applicationId.empty())
    {
        return E_INVALIDARG;
    }

    CapabilityDeclaration declaration{};
    declaration.capability = capability;
    declaration.isRequired = isRequired;
    declaration.justification = justification;
    declaration.userConsent = false;
    GetSystemTimeAsFileTime(&declaration.declaredTime);
    declaration.version = L"1.0";

    HRESULT hr = ValidateCapabilityDeclaration(declaration);
    if (FAILED(hr))
    {
        return hr;
    }

    EnterCriticalSection(&m_criticalSection);
    auto& list = m_applicationCapabilities[applicationId];
    auto it = std::find_if(list.begin(), list.end(),
        [capability](const CapabilityDeclaration& item)
        {
            return item.capability == capability;
        });

    if (it == list.end())
    {
        list.push_back(declaration);
    }
    else
    {
        *it = declaration;
    }

    LeaveCriticalSection(&m_criticalSection);
    return S_OK;
}

bool CapabilityManager::IsCapabilityDeclared(const std::wstring& applicationId,
                                             SystemCapability capability) const
{
    EnterCriticalSection(const_cast<CRITICAL_SECTION*>(&m_criticalSection));
    auto it = m_applicationCapabilities.find(applicationId);
    bool declared = false;
    if (it != m_applicationCapabilities.end())
    {
        declared = std::any_of(it->second.begin(), it->second.end(),
            [capability](const CapabilityDeclaration& item)
            {
                return item.capability == capability;
            });
    }
    LeaveCriticalSection(const_cast<CRITICAL_SECTION*>(&m_criticalSection));
    return declared;
}

std::vector<SystemCapability> CapabilityManager::GetDeclaredCapabilities(const std::wstring& applicationId) const
{
    std::vector<SystemCapability> capabilities;
    EnterCriticalSection(const_cast<CRITICAL_SECTION*>(&m_criticalSection));
    auto it = m_applicationCapabilities.find(applicationId);
    if (it != m_applicationCapabilities.end())
    {
        for (const auto& entry : it->second)
        {
            capabilities.push_back(entry.capability);
        }
    }
    LeaveCriticalSection(const_cast<CRITICAL_SECTION*>(&m_criticalSection));
    return capabilities;
}

HRESULT CapabilityManager::ValidateCapabilityDeclaration(const CapabilityDeclaration& declaration)
{
    if (IsSystemRestricted(declaration.capability) && declaration.isRequired)
    {
        return HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED);
    }

    return S_OK;
}

HRESULT CapabilityManager::GetCapabilityRequirements(SystemCapability capability,
                                                     bool* requiresUserConsent,
                                                     bool* requiresAdminApproval,
                                                     bool* requiresSystemAccess)
{
    if (!requiresUserConsent || !requiresAdminApproval || !requiresSystemAccess)
    {
        return E_INVALIDARG;
    }

    *requiresUserConsent = false;
    *requiresAdminApproval = false;
    *requiresSystemAccess = false;

    switch (capability)
    {
        case SystemCapability::Location:
        case SystemCapability::Webcam:
        case SystemCapability::Microphone:
            *requiresUserConsent = true;
            break;
        case SystemCapability::EnterpriseAuthentication:
        case SystemCapability::SharedUserCertificates:
            *requiresAdminApproval = true;
            *requiresSystemAccess = true;
            break;
        default:
            break;
    }

    return S_OK;
}

bool CapabilityManager::IsSystemRestricted(SystemCapability capability) const
{
    return m_systemRestrictedCapabilities.find(capability) != m_systemRestrictedCapabilities.end();
}

HRESULT CapabilityManager::ParseManifestFile(const std::wstring& manifestPath,
                                             std::vector<CapabilityDeclaration>* capabilities)
{
    if (!capabilities)
    {
        return E_INVALIDARG;
    }

    capabilities->clear();

    if (manifestPath.empty())
    {
        return S_OK;
    }

    std::wifstream stream(manifestPath);
    if (!stream.is_open())
    {
        return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
    }

    std::wstring line;
    while (std::getline(stream, line))
    {
        auto capabilityPos = line.find(L"Capability");
        auto namePos = line.find(L"Name=");
        if (capabilityPos == std::wstring::npos || namePos == std::wstring::npos)
        {
            continue;
        }

        auto quoteStart = line.find(L'"', namePos);
        auto quoteEnd = line.find(L'"', quoteStart + 1);
        if (quoteStart == std::wstring::npos || quoteEnd == std::wstring::npos)
        {
            continue;
        }

        std::wstring token = line.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
        auto capability = SecurityUtils::StringToCapability(token);
        if (capability != SystemCapability::InternetClient || token == L"ID_CAP_NETWORKING")
        {
            CapabilityDeclaration declaration{};
            declaration.capability = capability;
            declaration.isRequired = true;
            declaration.justification = L"Manifest declared capability";
            declaration.userConsent = false;
            GetSystemTimeAsFileTime(&declaration.declaredTime);
            declaration.version = L"1.0";
            capabilities->push_back(declaration);
        }
    }

    return S_OK;
}

void CapabilityManager::InitializeSystemRestrictions()
{
    m_systemRestrictedCapabilities.clear();
    m_systemRestrictedCapabilities.insert(SystemCapability::EnterpriseAuthentication);
    m_systemRestrictedCapabilities.insert(SystemCapability::SharedUserCertificates);
}

std::wstring CapabilityManager::GetCapabilityName(SystemCapability capability) const
{
    return SecurityUtils::CapabilityToString(capability);
}

// SandboxManager Implementation
SandboxManager::SandboxManager()
{
    InitializeCriticalSection(&m_criticalSection);
}

SandboxManager::~SandboxManager()
{
    DeleteCriticalSection(&m_criticalSection);
}

HRESULT SandboxManager::Initialize()
{
    EnterCriticalSection(&m_criticalSection);
    m_allowedPaths.clear();
    m_blockedPaths.clear();
    m_allowedRegistryKeys.clear();
    LeaveCriticalSection(&m_criticalSection);
    return S_OK;
}

HRESULT SandboxManager::SetSandboxLevel(const std::wstring& applicationId, SandboxLevel level)
{
    EnterCriticalSection(&m_criticalSection);
    m_applicationSandboxLevels[applicationId] = level;
    LeaveCriticalSection(&m_criticalSection);
    return S_OK;
}

SandboxLevel SandboxManager::GetSandboxLevel(const std::wstring& applicationId) const
{
    EnterCriticalSection(const_cast<CRITICAL_SECTION*>(&m_criticalSection));
    auto it = m_applicationSandboxLevels.find(applicationId);
    SandboxLevel level = SandboxLevel::Standard;
    if (it != m_applicationSandboxLevels.end())
    {
        level = it->second;
    }
    LeaveCriticalSection(const_cast<CRITICAL_SECTION*>(&m_criticalSection));
    return level;
}

bool SandboxManager::IsFilePathAllowed(const std::wstring& applicationId,
                                       const std::wstring& filePath,
                                       DWORD) const
{
    auto normalized = NormalizePath(filePath);
    EnterCriticalSection(const_cast<CRITICAL_SECTION*>(&m_criticalSection));
    bool allowed = !IsPathInBlockedList(normalized);
    if (allowed && !m_allowedPaths.empty())
    {
        allowed = IsPathInAllowedList(normalized);
    }
    LeaveCriticalSection(const_cast<CRITICAL_SECTION*>(&m_criticalSection));

    if (!allowed && SandboxLevel::Maximum == GetSandboxLevel(applicationId))
    {
        return false;
    }

    return allowed;
}

bool SandboxManager::IsRegistryKeyAllowed(const std::wstring& applicationId,
                                          const std::wstring& keyPath,
                                          DWORD) const
{
    auto normalized = NormalizePathSlow(keyPath);
    EnterCriticalSection(const_cast<CRITICAL_SECTION*>(&m_criticalSection));
    bool allowed = m_allowedRegistryKeys.empty() ||
                   m_allowedRegistryKeys.find(normalized) != m_allowedRegistryKeys.end();
    LeaveCriticalSection(const_cast<CRITICAL_SECTION*>(&m_criticalSection));

    if (!allowed && SandboxLevel::Maximum == GetSandboxLevel(applicationId))
    {
        return false;
    }

    return allowed;
}

bool SandboxManager::IsNetworkAccessAllowed(const std::wstring& applicationId,
                                            const std::wstring&,
                                            UINT16) const
{
    auto level = GetSandboxLevel(applicationId);
    return level != SandboxLevel::Maximum;
}

bool SandboxManager::IsProcessCreationAllowed(const std::wstring& applicationId,
                                              const std::wstring&) const
{
    auto level = GetSandboxLevel(applicationId);
    return level == SandboxLevel::None || level == SandboxLevel::Basic;
}

HRESULT SandboxManager::AddAllowedPath(const std::wstring& path)
{
    EnterCriticalSection(&m_criticalSection);
    m_allowedPaths.insert(NormalizePath(path));
    LeaveCriticalSection(&m_criticalSection);
    return S_OK;
}

HRESULT SandboxManager::AddBlockedPath(const std::wstring& path)
{
    EnterCriticalSection(&m_criticalSection);
    m_blockedPaths.insert(NormalizePath(path));
    LeaveCriticalSection(&m_criticalSection);
    return S_OK;
}

HRESULT SandboxManager::ConfigureFileSystemIsolation(const std::wstring&)
{
    return S_OK;
}

HRESULT SandboxManager::ConfigureNetworkIsolation(const std::wstring&)
{
    return S_OK;
}

HRESULT SandboxManager::ConfigureRegistryIsolation(const std::wstring&)
{
    return S_OK;
}

bool SandboxManager::IsPathInAllowedList(const std::wstring& path) const
{
    return m_allowedPaths.find(path) != m_allowedPaths.end();
}

bool SandboxManager::IsPathInBlockedList(const std::wstring& path) const
{
    return m_blockedPaths.find(path) != m_blockedPaths.end();
}

std::wstring SandboxManager::NormalizePath(const std::wstring& path) const
{
    return NormalizePathSlow(path);
}

// PermissionPromptManager Implementation
PermissionPromptManager::PermissionPromptManager()
    : m_allowPrompts(true)
{
    InitializeCriticalSection(&m_criticalSection);
}

PermissionPromptManager::~PermissionPromptManager()
{
    DeleteCriticalSection(&m_criticalSection);
}

HRESULT PermissionPromptManager::Initialize()
{
    return S_OK;
}

HRESULT PermissionPromptManager::ShowPermissionPrompt(const PermissionPrompt& prompt)
{
    if (!m_allowPrompts)
    {
        return S_OK;
    }

    auto remembered = GetRememberedChoice(prompt.capability);
    if (remembered == PermissionPromptResult::Allow || remembered == PermissionPromptResult::AlwaysAllow)
    {
        if (prompt.callback)
        {
            prompt.callback(remembered);
        }
        return S_OK;
    }

    // Simulate user approval by default in headless scenarios
    if (prompt.callback)
    {
        prompt.callback(PermissionPromptResult::Allow);
    }

    return S_OK;
}

HRESULT PermissionPromptManager::ShowPermissionPromptAsync(const PermissionPrompt& prompt)
{
    return ShowPermissionPrompt(prompt);
}

PermissionPromptResult PermissionPromptManager::GetRememberedChoice(SystemCapability capability) const
{
    EnterCriticalSection(const_cast<CRITICAL_SECTION*>(&m_criticalSection));
    auto it = m_rememberedChoices.find(capability);
    auto result = (it != m_rememberedChoices.end()) ? it->second : PermissionPromptResult::Cancel;
    LeaveCriticalSection(const_cast<CRITICAL_SECTION*>(&m_criticalSection));
    return result;
}

HRESULT PermissionPromptManager::SetRememberedChoice(SystemCapability capability,
                                                     PermissionPromptResult result)
{
    EnterCriticalSection(&m_criticalSection);
    m_rememberedChoices[capability] = result;
    LeaveCriticalSection(&m_criticalSection);
    return S_OK;
}

HRESULT PermissionPromptManager::ClearRememberedChoices()
{
    EnterCriticalSection(&m_criticalSection);
    m_rememberedChoices.clear();
    LeaveCriticalSection(&m_criticalSection);
    return S_OK;
}

void PermissionPromptManager::SetPromptsEnabled(bool enabled)
{
    m_allowPrompts = enabled;
}

std::wstring PermissionPromptManager::GetCapabilityDisplayName(SystemCapability capability) const
{
    return SecurityUtils::CapabilityToString(capability);
}

std::wstring PermissionPromptManager::GetCapabilityDescription(SystemCapability capability) const
{
    return L"Access to " + SecurityUtils::CapabilityToString(capability);
}

std::wstring PermissionPromptManager::GetCapabilityRiskLevel(SystemCapability) const
{
    return L"Medium";
}

PermissionPromptResult PermissionPromptManager::ShowNativePrompt(const PermissionPrompt&) const
{
    return PermissionPromptResult::Allow;
}

HRESULT PermissionPromptManager::CreatePromptDialog(const PermissionPrompt&, HWND*)
{
    return S_OK;
}

std::wstring PermissionPromptManager::GeneratePromptMessage(const PermissionPrompt& prompt) const
{
    return L"Application requests access to " + GetCapabilityDisplayName(prompt.capability);
}

std::wstring PermissionPromptManager::GenerateDetailedExplanation(SystemCapability capability) const
{
    return L"Access required for " + GetCapabilityDisplayName(capability);
}

// SecurityEnforcer Implementation
SecurityEnforcer::SecurityEnforcer()
    : m_enforcementEnabled(true)
    , m_defaultSecurityLevel(SecurityLevel::Partial)
    , m_defaultSandboxLevel(SandboxLevel::Standard)
{
    InitializeCriticalSection(&m_criticalSection);
}

SecurityEnforcer::~SecurityEnforcer()
{
    Shutdown();
    DeleteCriticalSection(&m_criticalSection);
}

HRESULT SecurityEnforcer::Initialize()
{
    m_capabilityManager = std::make_unique<CapabilityManager>();
    m_sandboxManager = std::make_unique<SandboxManager>();
    m_promptManager = std::make_unique<PermissionPromptManager>();

    RETURN_IF_FAILED(m_capabilityManager->Initialize());
    RETURN_IF_FAILED(m_sandboxManager->Initialize());
    RETURN_IF_FAILED(m_promptManager->Initialize());

    return S_OK;
}

void SecurityEnforcer::Shutdown()
{
    EnterCriticalSection(&m_criticalSection);
    m_securityContexts.clear();
    m_violationLog.clear();
    LeaveCriticalSection(&m_criticalSection);
}

HRESULT SecurityEnforcer::CreateSecurityContext(const std::wstring& applicationId,
                                                const std::wstring& manifestPath,
                                                SecurityContext* context)
{
    if (!context || applicationId.empty())
    {
        return E_INVALIDARG;
    }

    SecurityContext newContext{};
    newContext.applicationId = applicationId;
    newContext.publisherId = L"Unknown";
    newContext.level = m_defaultSecurityLevel;
    newContext.sandboxLevel = m_defaultSandboxLevel;
    newContext.isDebugging = false;
    GetSystemTimeAsFileTime(&newContext.createdTime);

    RETURN_IF_FAILED(m_capabilityManager->LoadApplicationCapabilities(applicationId, manifestPath));
    auto declared = m_capabilityManager->GetDeclaredCapabilities(applicationId);
    newContext.declaredCapabilities.insert(declared.begin(), declared.end());
    newContext.grantedCapabilities = newContext.declaredCapabilities;

    EnterCriticalSection(&m_criticalSection);
    m_securityContexts[applicationId] = newContext;
    *context = newContext;
    LeaveCriticalSection(&m_criticalSection);

    return S_OK;
}

HRESULT SecurityEnforcer::SetSecurityContext(const std::wstring& applicationId,
                                             const SecurityContext& context)
{
    if (applicationId.empty())
    {
        return E_INVALIDARG;
    }

    RETURN_IF_FAILED(ValidateSecurityContext(context));

    EnterCriticalSection(&m_criticalSection);
    m_securityContexts[applicationId] = context;
    LeaveCriticalSection(&m_criticalSection);
    return S_OK;
}

const SecurityContext* SecurityEnforcer::GetSecurityContext(const std::wstring& applicationId) const
{
    EnterCriticalSection(const_cast<CRITICAL_SECTION*>(&m_criticalSection));
    auto it = m_securityContexts.find(applicationId);
    const SecurityContext* context = (it != m_securityContexts.end()) ? &it->second : nullptr;
    LeaveCriticalSection(const_cast<CRITICAL_SECTION*>(&m_criticalSection));
    return context;
}

HRESULT SecurityEnforcer::CheckCapabilityAccess(const std::wstring& applicationId,
                                                SystemCapability capability,
                                                bool promptIfNeeded)
{
    auto context = GetSecurityContext(applicationId);
    if (!context)
    {
        return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
    }

    HRESULT hr = EnforceCapabilityAccess(*context, capability);
    if (hr == HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED) && promptIfNeeded)
    {
        PermissionPrompt prompt{};
        prompt.capability = capability;
        prompt.applicationName = applicationId;
        prompt.message = L"Runtime permission required";
        prompt.canRemember = true;
        prompt.isOneTime = false;
        prompt.callback = [this, applicationId, capability](PermissionPromptResult result)
        {
            if (result == PermissionPromptResult::Allow || result == PermissionPromptResult::AlwaysAllow)
            {
                EnterCriticalSection(&m_criticalSection);
                auto& ctx = m_securityContexts[applicationId];
                ctx.grantedCapabilities.insert(capability);
                LeaveCriticalSection(&m_criticalSection);
            }
        };

        m_promptManager->ShowPermissionPrompt(prompt);
        hr = S_OK;
    }

    return hr;
}

HRESULT SecurityEnforcer::ValidateSystemCall(const std::wstring& applicationId,
                                             const std::wstring& functionName,
                                             void*)
{
    if (!IsSystemLevelOperation(functionName))
    {
        return S_OK;
    }

    return CheckCapabilityAccess(applicationId, SystemCapability::EnterpriseAuthentication, false);
}

HRESULT SecurityEnforcer::ValidateFileAccess(const std::wstring& applicationId,
                                             const std::wstring& filePath,
                                             DWORD desiredAccess)
{
    if (!m_sandboxManager->IsFilePathAllowed(applicationId, filePath, desiredAccess))
    {
        return HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED);
    }

    return S_OK;
}

HRESULT SecurityEnforcer::ValidateNetworkAccess(const std::wstring& applicationId,
                                                const std::wstring& hostname,
                                                UINT16 port)
{
    if (!m_sandboxManager->IsNetworkAccessAllowed(applicationId, hostname, port))
    {
        return HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED);
    }

    return CheckCapabilityAccess(applicationId, SystemCapability::InternetClient, false);
}

HRESULT SecurityEnforcer::ValidateRegistryAccess(const std::wstring& applicationId,
                                                 const std::wstring& keyPath,
                                                 DWORD desiredAccess)
{
    if (!m_sandboxManager->IsRegistryKeyAllowed(applicationId, keyPath, desiredAccess))
    {
        return HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED);
    }

    return S_OK;
}

HRESULT SecurityEnforcer::LogSecurityViolation(const std::wstring& applicationId,
                                               SecurityViolationType violationType,
                                               const std::wstring& description,
                                               SystemCapability attemptedCapability)
{
    SecurityViolation violation{};
    violation.type = violationType;
    violation.description = description;
    violation.source = applicationId;
    violation.attemptedCapability = attemptedCapability;
    GetSystemTimeAsFileTime(&violation.timestamp);
    violation.wasBlocked = true;

    EnterCriticalSection(&m_criticalSection);
    m_violationLog.push_back(violation);
    LeaveCriticalSection(&m_criticalSection);

    return S_OK;
}

HRESULT SecurityEnforcer::SetEnforcementLevel(SecurityLevel level)
{
    m_defaultSecurityLevel = level;
    return S_OK;
}

HRESULT SecurityEnforcer::SetDefaultSandboxLevel(SandboxLevel level)
{
    m_defaultSandboxLevel = level;
    return S_OK;
}

HRESULT SecurityEnforcer::EnableEnforcement(bool enabled)
{
    m_enforcementEnabled = enabled;
    return S_OK;
}

std::vector<SecurityViolation> SecurityEnforcer::GetSecurityViolations(const std::wstring& applicationId) const
{
    std::vector<SecurityViolation> violations;
    EnterCriticalSection(const_cast<CRITICAL_SECTION*>(&m_criticalSection));
    for (const auto& violation : m_violationLog)
    {
        if (violation.source == applicationId)
        {
            violations.push_back(violation);
        }
    }
    LeaveCriticalSection(const_cast<CRITICAL_SECTION*>(&m_criticalSection));
    return violations;
}

HRESULT SecurityEnforcer::GenerateSecurityReport(const std::wstring& filePath) const
{
    std::wofstream stream(filePath);
    if (!stream.is_open())
    {
        return HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED);
    }

    stream << L"CLRNet Security Report\n";
    stream << L"=====================\n\n";

    EnterCriticalSection(const_cast<CRITICAL_SECTION*>(&m_criticalSection));
    for (const auto& entry : m_securityContexts)
    {
        stream << L"Application: " << entry.first << L"\n";
        stream << L"  Security Level: " << SecurityUtils::SecurityLevelToString(entry.second.level) << L"\n";
        stream << L"  Sandbox: " << static_cast<int>(entry.second.sandboxLevel) << L"\n";
    }

    if (!m_violationLog.empty())
    {
        stream << L"\nRecorded Violations:\n";
        for (const auto& violation : m_violationLog)
        {
            stream << L"  - App: " << violation.source << L", Capability: "
                   << SecurityUtils::CapabilityToString(violation.attemptedCapability)
                   << L", Description: " << violation.description << L"\n";
        }
    }
    LeaveCriticalSection(const_cast<CRITICAL_SECTION*>(&m_criticalSection));

    return S_OK;
}

HRESULT SecurityEnforcer::EmergencyLockdown(const std::wstring& applicationId)
{
    RETURN_IF_FAILED(SetDefaultSandboxLevel(SandboxLevel::Maximum));
    RETURN_IF_FAILED(EnableEnforcement(true));
    return LogSecurityViolation(applicationId, SecurityViolationType::SecurityViolation,
                                 L"Emergency lockdown engaged", SystemCapability::InternetClient);
}

HRESULT SecurityEnforcer::RestoreFromLockdown(const std::wstring& applicationId)
{
    RETURN_IF_FAILED(SetDefaultSandboxLevel(SandboxLevel::Standard));
    return LogSecurityViolation(applicationId, SecurityViolationType::SecurityViolation,
                                 L"Security lockdown released", SystemCapability::InternetClient);
}

HRESULT SecurityEnforcer::EnforceCapabilityAccess(const SecurityContext& context,
                                                  SystemCapability capability)
{
    if (!m_enforcementEnabled)
    {
        return S_OK;
    }

    if (context.grantedCapabilities.find(capability) != context.grantedCapabilities.end())
    {
        return S_OK;
    }

    if (context.declaredCapabilities.find(capability) != context.declaredCapabilities.end())
    {
        HandleCapabilityViolation(context.applicationId, capability);
        return HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED);
    }

    HandleCapabilityViolation(context.applicationId, capability);
    return HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED);
}

HRESULT SecurityEnforcer::HandleCapabilityViolation(const std::wstring& applicationId,
                                                    SystemCapability capability)
{
    std::wstringstream description;
    description << L"Capability violation for " << SecurityUtils::CapabilityToString(capability);
    return LogSecurityViolation(applicationId, SecurityViolationType::CapabilityViolation, description.str(), capability);
}

bool SecurityEnforcer::IsSystemLevelOperation(const std::wstring& functionName) const
{
    static const std::vector<std::wstring> systemPrefixes =
    {
        L"Nt", L"Zw", L"Rtl"
    };

    for (const auto& prefix : systemPrefixes)
    {
        if (functionName.rfind(prefix, 0) == 0)
        {
            return true;
        }
    }

    return false;
}

HRESULT SecurityEnforcer::ValidateSecurityContext(const SecurityContext& context)
{
    if (context.applicationId.empty())
    {
        return E_INVALIDARG;
    }

    return S_OK;
}

void SecurityEnforcer::UpdateContextFromManifest(const std::wstring& manifestPath,
                                                 SecurityContext* context)
{
    if (!context)
    {
        return;
    }

    if (manifestPath.empty())
    {
        return;
    }

    if (SUCCEEDED(m_capabilityManager->LoadApplicationCapabilities(context->applicationId, manifestPath)))
    {
        auto caps = m_capabilityManager->GetDeclaredCapabilities(context->applicationId);
        context->declaredCapabilities.insert(caps.begin(), caps.end());
    }
}

// SecurityManagerFactory Implementation
SecurityEnforcer* SecurityManagerFactory::CreateInstance()
{
    auto instance = new SecurityEnforcer();
    if (FAILED(instance->Initialize()))
    {
        delete instance;
        return nullptr;
    }
    return instance;
}

void SecurityManagerFactory::DestroyInstance(SecurityEnforcer* instance)
{
    delete instance;
}

SecurityContext SecurityManagerFactory::CreateStandardAppContext(const std::wstring& applicationId)
{
    SecurityContext context{};
    context.applicationId = applicationId;
    context.level = SecurityLevel::Partial;
    context.sandboxLevel = SandboxLevel::Standard;
    context.createdTime.dwLowDateTime = 0;
    context.createdTime.dwHighDateTime = 0;
    return context;
}

SecurityContext SecurityManagerFactory::CreateTrustedAppContext(const std::wstring& applicationId)
{
    SecurityContext context = CreateStandardAppContext(applicationId);
    context.level = SecurityLevel::Trusted;
    return context;
}

SecurityContext SecurityManagerFactory::CreateSandboxedContext(const std::wstring& applicationId)
{
    SecurityContext context = CreateStandardAppContext(applicationId);
    context.sandboxLevel = SandboxLevel::Enhanced;
    return context;
}

// SecurityUtils Implementation
std::wstring SecurityUtils::CapabilityToString(SystemCapability capability)
{
    switch (capability)
    {
        case SystemCapability::InternetClient: return L"InternetClient";
        case SystemCapability::InternetClientServer: return L"InternetClientServer";
        case SystemCapability::PrivateNetworkClientServer: return L"PrivateNetworkClientServer";
        case SystemCapability::Location: return L"Location";
        case SystemCapability::Webcam: return L"Webcam";
        case SystemCapability::Microphone: return L"Microphone";
        case SystemCapability::MusicLibrary: return L"MusicLibrary";
        case SystemCapability::PicturesLibrary: return L"PicturesLibrary";
        case SystemCapability::VideosLibrary: return L"VideosLibrary";
        case SystemCapability::RemovableStorage: return L"RemovableStorage";
        case SystemCapability::PhoneDialer: return L"PhoneDialer";
        case SystemCapability::SMS: return L"SMS";
        case SystemCapability::Contacts: return L"Contacts";
        case SystemCapability::Calendar: return L"Calendar";
        case SystemCapability::AppointmentsSystem: return L"AppointmentsSystem";
        case SystemCapability::ContactsSystem: return L"ContactsSystem";
        case SystemCapability::EmailSystem: return L"EmailSystem";
        case SystemCapability::GameBarServices: return L"GameBarServices";
        case SystemCapability::Bluetooth: return L"Bluetooth";
        case SystemCapability::WiFiControl: return L"WiFiControl";
        case SystemCapability::EnterpriseAuthentication: return L"EnterpriseAuthentication";
        case SystemCapability::SharedUserCertificates: return L"SharedUserCertificates";
        default: return L"InternetClient";
    }
}

SystemCapability SecurityUtils::StringToCapability(const std::wstring& capabilityName)
{
    std::wstring normalized = capabilityName;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), ::towupper);

    if (normalized == L"ID_CAP_NETWORKING" || normalized == L"INTERNETCLIENT")
    {
        return SystemCapability::InternetClient;
    }
    if (normalized == L"ID_CAP_NETWORKING_ADMIN" || normalized == L"INTERNETCLIENTSERVER")
    {
        return SystemCapability::InternetClientServer;
    }
    if (normalized == L"ID_CAP_PROXIMITY" || normalized == L"PRIVATENETWORKCLIENTSERVER")
    {
        return SystemCapability::PrivateNetworkClientServer;
    }
    if (normalized == L"ID_CAP_LOCATION" || normalized == L"LOCATION")
    {
        return SystemCapability::Location;
    }
    if (normalized == L"ID_CAP_ISV_CAMERA" || normalized == L"WEBCAM")
    {
        return SystemCapability::Webcam;
    }
    if (normalized == L"ID_CAP_MICROPHONE" || normalized == L"MICROPHONE")
    {
        return SystemCapability::Microphone;
    }
    if (normalized == L"ID_CAP_MEDIALIB_AUDIO" || normalized == L"MUSICLIBRARY")
    {
        return SystemCapability::MusicLibrary;
    }
    if (normalized == L"ID_CAP_MEDIALIB_PHOTO" || normalized == L"PICTURESLIBRARY")
    {
        return SystemCapability::PicturesLibrary;
    }
    if (normalized == L"ID_CAP_MEDIALIB_VIDEO" || normalized == L"VIDEOSLIBRARY")
    {
        return SystemCapability::VideosLibrary;
    }
    if (normalized == L"ID_CAP_REMOVABLE_STORAGE" || normalized == L"REMOVABLESTORAGE")
    {
        return SystemCapability::RemovableStorage;
    }
    if (normalized == L"ID_CAP_PHONEDIALER" || normalized == L"PHONEDIALER")
    {
        return SystemCapability::PhoneDialer;
    }
    if (normalized == L"ID_CAP_SMS" || normalized == L"SMS")
    {
        return SystemCapability::SMS;
    }
    if (normalized == L"ID_CAP_CONTACTS" || normalized == L"CONTACTS")
    {
        return SystemCapability::Contacts;
    }
    if (normalized == L"ID_CAP_APPOINTMENTS" || normalized == L"CALENDAR")
    {
        return SystemCapability::Calendar;
    }
    if (normalized == L"ENTERPRISEAUTHENTICATION")
    {
        return SystemCapability::EnterpriseAuthentication;
    }
    if (normalized == L"SHAREDUSERCERTIFICATES")
    {
        return SystemCapability::SharedUserCertificates;
    }

    return SystemCapability::InternetClient;
}

std::wstring SecurityUtils::SecurityLevelToString(SecurityLevel level)
{
    switch (level)
    {
        case SecurityLevel::Untrusted: return L"Untrusted";
        case SecurityLevel::Partial: return L"Partial";
        case SecurityLevel::Trusted: return L"Trusted";
        case SecurityLevel::System: return L"System";
        case SecurityLevel::Administrator: return L"Administrator";
        default: return L"Untrusted";
    }
}

SecurityLevel SecurityUtils::StringToSecurityLevel(const std::wstring& levelName)
{
    std::wstring normalized = levelName;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), ::towupper);
    if (normalized == L"PARTIAL")
    {
        return SecurityLevel::Partial;
    }
    if (normalized == L"TRUSTED")
    {
        return SecurityLevel::Trusted;
    }
    if (normalized == L"SYSTEM")
    {
        return SecurityLevel::System;
    }
    if (normalized == L"ADMINISTRATOR")
    {
        return SecurityLevel::Administrator;
    }
    return SecurityLevel::Untrusted;
}

bool SecurityUtils::IsSecurePath(const std::wstring& path)
{
    auto normalized = NormalizePathSlow(path);
    return normalized.find(L"\\AppData\\Local") != std::wstring::npos;
}

std::wstring SecurityUtils::GetSecureAppDataPath(const std::wstring& applicationId)
{
    std::wstring base = L"\\AppData\\Local\\";
    return base + applicationId;
}

bool SecurityUtils::IsSystemPath(const std::wstring& path)
{
    auto normalized = NormalizePathSlow(path);
    return normalized.rfind(L"C:\\Windows", 0) == 0 || normalized.rfind(L"C:\\Program Files", 0) == 0;
}

HRESULT SecurityUtils::GenerateSecurityToken(const SecurityContext& context, std::wstring* token)
{
    if (!token)
    {
        return E_INVALIDARG;
    }

    std::wstringstream stream;
    stream << context.applicationId << L"|" << static_cast<int>(context.level)
           << L"|" << static_cast<int>(context.sandboxLevel)
           << L"|" << context.declaredCapabilities.size();
    *token = stream.str();
    return S_OK;
}

HRESULT SecurityUtils::ValidateSecurityToken(const std::wstring& token, SecurityContext* context)
{
    if (!context)
    {
        return E_INVALIDARG;
    }

    std::wistringstream stream(token);
    std::wstring applicationId;
    std::wstring level;
    std::wstring sandbox;
    std::wstring capabilityCount;

    if (!std::getline(stream, applicationId, L'|'))
    {
        return E_INVALIDARG;
    }
    if (!std::getline(stream, level, L'|'))
    {
        return E_INVALIDARG;
    }
    if (!std::getline(stream, sandbox, L'|'))
    {
        return E_INVALIDARG;
    }
    if (!std::getline(stream, capabilityCount, L'|'))
    {
        return E_INVALIDARG;
    }

    context->applicationId = applicationId;
    context->level = static_cast<SecurityLevel>(std::stoi(level));
    context->sandboxLevel = static_cast<SandboxLevel>(std::stoi(sandbox));
    return S_OK;
}

} // namespace Interop
} // namespace CLRNet
