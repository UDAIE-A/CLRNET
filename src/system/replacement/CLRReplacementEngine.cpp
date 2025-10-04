#include "CLRReplacementEngine.h"
#include <algorithm>
#include <shlwapi.h>

#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "shlwapi.lib")

namespace CLRNet {
namespace System {

// LegacyCLRDetector Implementation
LegacyCLRDetector::LegacyCLRDetector() {
    InitializeCriticalSection(&m_criticalSection);
}

LegacyCLRDetector::~LegacyCLRDetector() {
    StopProcessMonitoring();
    DeleteCriticalSection(&m_criticalSection);
}

HRESULT LegacyCLRDetector::Initialize() {
    // Initial scan for managed processes
    return ScanForManagedProcesses();
}

HRESULT LegacyCLRDetector::ScanForManagedProcesses() {
    EnterCriticalSection(&m_criticalSection);
    
    // Take snapshot of running processes
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        LeaveCriticalSection(&m_criticalSection);
        return HRESULT_FROM_WIN32(GetLastError());
    }
    
    PROCESSENTRY32W processEntry;
    processEntry.dwSize = sizeof(processEntry);
    
    if (Process32FirstW(snapshot, &processEntry)) {
        do {
            // Check if process uses managed code
            if (IsProcessManaged(processEntry.th32ProcessID)) {
                ProcessInjectionInfo info;
                info.processId = processEntry.th32ProcessID;
                info.processName = processEntry.szExeFile;
                info.isManaged = true;
                info.isReplaced = false;
                info.status = ReplacementStatus::NotReplaced;
                GetSystemTimeAsFileTime(&info.injectionTime);
                
                m_managedProcesses[processEntry.th32ProcessID] = info;
            }
        } while (Process32NextW(snapshot, &processEntry));
    }
    
    CloseHandle(snapshot);
    LeaveCriticalSection(&m_criticalSection);
    return S_OK;
}

bool LegacyCLRDetector::IsProcessManaged(DWORD processId) {
    HANDLE processHandle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                                      FALSE, processId);
    if (!processHandle) {
        return false;
    }
    
    bool isManaged = IsAssemblyLoaded(processHandle);
    CloseHandle(processHandle);
    return isManaged;
}

bool LegacyCLRDetector::IsAssemblyLoaded(HANDLE processHandle) {
    // Get list of loaded modules
    HMODULE modules[1024];
    DWORD needed;
    
    if (!EnumProcessModules(processHandle, modules, sizeof(modules), &needed)) {
        return false;
    }
    
    DWORD moduleCount = needed / sizeof(HMODULE);
    
    // Check for CLR-related modules
    for (DWORD i = 0; i < moduleCount; i++) {
        WCHAR moduleName[MAX_PATH];
        if (GetModuleBaseNameW(processHandle, modules[i], moduleName, MAX_PATH)) {
            if (IsCLRModule(moduleName)) {
                return true;
            }
        }
    }
    
    return false;
}

bool LegacyCLRDetector::IsCLRModule(const std::wstring& moduleName) {
    // Check for common CLR modules
    std::vector<std::wstring> clrModules = {
        L"mscorwks.dll",    // .NET Framework 2.0-3.5
        L"clr.dll",         // .NET Framework 4.0+
        L"mscorlib.dll",    // Base class library
        L"System.dll",      // System libraries
        L"mscorjit.dll",    // JIT compiler
        L"mscorpe.dll"      // PE loader
    };
    
    std::wstring lowerModuleName = moduleName;
    std::transform(lowerModuleName.begin(), lowerModuleName.end(), 
                  lowerModuleName.begin(), ::towlower);
    
    for (const auto& clrModule : clrModules) {
        if (lowerModuleName == clrModule) {
            return true;
        }
    }
    
    return false;
}

std::vector<ProcessInjectionInfo> LegacyCLRDetector::GetManagedProcesses() const {
    std::vector<ProcessInjectionInfo> processes;
    
    EnterCriticalSection(const_cast<CRITICAL_SECTION*>(&m_criticalSection));
    
    for (const auto& pair : m_managedProcesses) {
        processes.push_back(pair.second);
    }
    
    LeaveCriticalSection(const_cast<CRITICAL_SECTION*>(&m_criticalSection));
    return processes;
}

// ProcessInjector Implementation
ProcessInjector::ProcessInjector() {
    InitializeCriticalSection(&m_criticalSection);
}

ProcessInjector::~ProcessInjector() {
    // Clean up all injections
    EnterCriticalSection(&m_criticalSection);
    for (const auto& pair : m_injectedProcesses) {
        CloseHandle(pair.second);
    }
    LeaveCriticalSection(&m_criticalSection);
    
    DeleteCriticalSection(&m_criticalSection);
}

HRESULT ProcessInjector::Initialize(const std::wstring& injectionDllPath) {
    if (!PathFileExistsW(injectionDllPath.c_str())) {
        return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
    }
    
    m_injectionDllPath = injectionDllPath;
    return S_OK;
}

HRESULT ProcessInjector::InjectIntoProcess(DWORD processId) {
    // Use DLL injection as the primary method
    return InjectUsingDLL(processId, m_injectionDllPath);
}

HRESULT ProcessInjector::InjectUsingDLL(DWORD processId, const std::wstring& dllPath) {
    // Open target process
    HANDLE processHandle = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
                                     PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
                                     FALSE, processId);
    
    if (!processHandle) {
        return HRESULT_FROM_WIN32(GetLastError());
    }
    
    HRESULT hr = S_OK;
    
    // Allocate memory in target process for DLL path
    SIZE_T pathSize = (dllPath.length() + 1) * sizeof(WCHAR);
    LPVOID remoteMemory = VirtualAllocEx(processHandle, nullptr, pathSize,
                                        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    
    if (!remoteMemory) {
        hr = HRESULT_FROM_WIN32(GetLastError());
        CloseHandle(processHandle);
        return hr;
    }
    
    // Write DLL path to target process
    SIZE_T bytesWritten;
    if (!WriteProcessMemory(processHandle, remoteMemory, dllPath.c_str(),
                           pathSize, &bytesWritten)) {
        hr = HRESULT_FROM_WIN32(GetLastError());
        VirtualFreeEx(processHandle, remoteMemory, 0, MEM_RELEASE);
        CloseHandle(processHandle);
        return hr;
    }
    
    // Get address of LoadLibraryW
    HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
    FARPROC loadLibraryAddr = GetProcAddress(kernel32, "LoadLibraryW");
    
    if (!loadLibraryAddr) {
        hr = HRESULT_FROM_WIN32(GetLastError());
        VirtualFreeEx(processHandle, remoteMemory, 0, MEM_RELEASE);
        CloseHandle(processHandle);
        return hr;
    }
    
    // Create remote thread to load DLL
    HANDLE remoteThread = CreateRemoteThread(processHandle, nullptr, 0,
                                           reinterpret_cast<LPTHREAD_START_ROUTINE>(loadLibraryAddr),
                                           remoteMemory, 0, nullptr);
    
    if (!remoteThread) {
        hr = HRESULT_FROM_WIN32(GetLastError());
        VirtualFreeEx(processHandle, remoteMemory, 0, MEM_RELEASE);
        CloseHandle(processHandle);
        return hr;
    }
    
    // Wait for thread completion
    WaitForSingleObject(remoteThread, INFINITE);
    
    // Cleanup
    CloseHandle(remoteThread);
    VirtualFreeEx(processHandle, remoteMemory, 0, MEM_RELEASE);
    
    // Store injection information
    EnterCriticalSection(&m_criticalSection);
    m_injectedProcesses[processId] = processHandle;
    LeaveCriticalSection(&m_criticalSection);
    
    return hr;
}

bool ProcessInjector::IsProcessInjected(DWORD processId) const {
    EnterCriticalSection(const_cast<CRITICAL_SECTION*>(&m_criticalSection));
    bool isInjected = m_injectedProcesses.find(processId) != m_injectedProcesses.end();
    LeaveCriticalSection(const_cast<CRITICAL_SECTION*>(&m_criticalSection));
    return isInjected;
}

// ModernCLRInjector Implementation
ModernCLRInjector::ModernCLRInjector() {
}

ModernCLRInjector::~ModernCLRInjector() {
}

HRESULT ModernCLRInjector::Initialize(const std::wstring& modernCLRPath) {
    if (!PathFileExistsW(modernCLRPath.c_str())) {
        return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
    }
    
    m_modernCLRPath = modernCLRPath;
    return S_OK;
}

HRESULT ModernCLRInjector::ReplaceCLRInProcess(DWORD processId) {
    HANDLE processHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
    if (!processHandle) {
        return HRESULT_FROM_WIN32(GetLastError());
    }
    
    HRESULT hr = S_OK;
    
    // Load modern CLR into target process
    hr = LoadModernCLR(processHandle);
    if (FAILED(hr)) {
        CloseHandle(processHandle);
        return hr;
    }
    
    // Redirect CLR API calls
    hr = RedirectCLRCalls(processHandle);
    if (FAILED(hr)) {
        CloseHandle(processHandle);
        return hr;
    }
    
    // Initialize modern runtime
    hr = InitializeModernRuntime(processHandle);
    
    CloseHandle(processHandle);
    return hr;
}

HRESULT ModernCLRInjector::LoadModernCLR(HANDLE processHandle) {
    // This would implement the actual loading of the modern CLR
    // For now, simulate the operation
    return S_OK;
}

HRESULT ModernCLRInjector::RedirectCLRCalls(HANDLE processHandle) {
    // This would implement API redirection from legacy to modern CLR
    // For now, simulate the operation
    return S_OK;
}

HRESULT ModernCLRInjector::InitializeModernRuntime(HANDLE processHandle) {
    // This would initialize the modern CLR runtime in the target process
    // For now, simulate the operation
    return S_OK;
}

// CLRReplacementEngine Implementation
CLRReplacementEngine::CLRReplacementEngine() 
    : m_currentLevel(ReplacementLevel::None)
    , m_initialized(false)
    , m_monitoringThread(nullptr)
    , m_monitoringActive(false) {
    
    InitializeCriticalSection(&m_criticalSection);
}

CLRReplacementEngine::~CLRReplacementEngine() {
    if (m_initialized) {
        StopMonitoring();
    }
    DeleteCriticalSection(&m_criticalSection);
}

HRESULT CLRReplacementEngine::Initialize(ReplacementLevel level) {
    if (m_initialized) {
        return S_OK;
    }
    
    m_currentLevel = level;
    
    HRESULT hr = InitializeComponents();
    if (FAILED(hr)) {
        return hr;
    }
    
    hr = StartMonitoring();
    if (FAILED(hr)) {
        return hr;
    }
    
    m_initialized = true;
    return S_OK;
}

HRESULT CLRReplacementEngine::InitializeComponents() {
    // Initialize legacy CLR detector
    m_legacyDetector = std::make_unique<LegacyCLRDetector>();
    HRESULT hr = m_legacyDetector->Initialize();
    if (FAILED(hr)) {
        return hr;
    }
    
    // Initialize process injector
    m_processInjector = std::make_unique<ProcessInjector>();
    hr = m_processInjector->Initialize(L"clrnet-injection.dll");
    if (FAILED(hr)) {
        return hr;
    }
    
    // Initialize modern CLR injector
    m_modernInjector = std::make_unique<ModernCLRInjector>();
    hr = m_modernInjector->Initialize(L"clrnet.dll");
    if (FAILED(hr)) {
        return hr;
    }
    
    // Initialize compatibility shim
    m_compatibilityShim = std::make_unique<CompatibilityShim>();
    hr = m_compatibilityShim->Initialize();
    if (FAILED(hr)) {
        return hr;
    }
    
    return S_OK;
}

HRESULT CLRReplacementEngine::ReplaceProcessCLR(DWORD processId) {
    if (!m_initialized) {
        return E_NOT_VALID_STATE;
    }
    
    // Check if process can be safely replaced
    if (!CanReplaceProcess(processId)) {
        return E_ACCESSDENIED;
    }
    
    HRESULT hr = S_OK;
    
    // Inject process injector
    hr = m_processInjector->InjectIntoProcess(processId);
    if (FAILED(hr)) {
        return hr;
    }
    
    // Replace CLR with modern version
    hr = m_modernInjector->ReplaceCLRInProcess(processId);
    if (FAILED(hr)) {
        m_processInjector->RemoveFromProcess(processId);
        return hr;
    }
    
    // Apply compatibility shims
    hr = m_compatibilityShim->ApplyShimsToProcess(processId);
    if (FAILED(hr)) {
        RollbackProcess(processId);
        return hr;
    }
    
    // Update tracking information
    EnterCriticalSection(&m_criticalSection);
    ProcessInjectionInfo info;
    info.processId = processId;
    info.isManaged = true;
    info.isReplaced = true;
    info.status = ReplacementStatus::FullyReplaced;
    GetSystemTimeAsFileTime(&info.injectionTime);
    
    m_replacedProcesses[processId] = info;
    LeaveCriticalSection(&m_criticalSection);
    
    return S_OK;
}

HRESULT CLRReplacementEngine::RollbackProcess(DWORD processId) {
    EnterCriticalSection(&m_criticalSection);
    
    auto it = m_replacedProcesses.find(processId);
    if (it == m_replacedProcesses.end()) {
        LeaveCriticalSection(&m_criticalSection);
        return E_INVALIDARG;
    }
    
    // Update status to rollback in progress
    it->second.status = ReplacementStatus::RollbackInProgress;
    
    LeaveCriticalSection(&m_criticalSection);
    
    HRESULT hr = S_OK;
    
    // Remove compatibility shims
    hr = m_compatibilityShim->RemoveShimsFromProcess(processId);
    
    // Remove process injection
    hr = m_processInjector->RemoveFromProcess(processId);
    
    // Update final status
    EnterCriticalSection(&m_criticalSection);
    it = m_replacedProcesses.find(processId);
    if (it != m_replacedProcesses.end()) {
        it->second.status = ReplacementStatus::NotReplaced;
        it->second.isReplaced = false;
    }
    LeaveCriticalSection(&m_criticalSection);
    
    return hr;
}

bool CLRReplacementEngine::CanReplaceProcess(DWORD processId) {
    // Check if process is system critical
    if (IsSystemCriticalProcess(processId)) {
        return false;
    }
    
    // Check if process is in exclusion list
    // Implementation would check against m_config.excludedProcesses
    
    // Additional safety checks would go here
    
    return true;
}

bool CLRReplacementEngine::IsSystemCriticalProcess(DWORD processId) {
    // List of system critical processes that should never be replaced
    std::vector<std::wstring> criticalProcesses = {
        L"winlogon.exe",
        L"csrss.exe",
        L"services.exe",
        L"lsass.exe",
        L"explorer.exe"
    };
    
    // Get process name and check against critical list
    HANDLE processHandle = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, processId);
    if (!processHandle) {
        return true; // Assume critical if we can't query
    }
    
    WCHAR processName[MAX_PATH];
    DWORD size = MAX_PATH;
    bool isCritical = false;
    
    if (QueryFullProcessImageNameW(processHandle, 0, processName, &size)) {
        WCHAR* fileName = PathFindFileNameW(processName);
        std::wstring processNameStr = fileName;
        
        std::transform(processNameStr.begin(), processNameStr.end(),
                      processNameStr.begin(), ::towlower);
        
        for (const auto& criticalProcess : criticalProcesses) {
            std::wstring lowerCritical = criticalProcess;
            std::transform(lowerCritical.begin(), lowerCritical.end(),
                          lowerCritical.begin(), ::towlower);
            
            if (processNameStr == lowerCritical) {
                isCritical = true;
                break;
            }
        }
    }
    
    CloseHandle(processHandle);
    return isCritical;
}

HRESULT CLRReplacementEngine::StartMonitoring() {
    if (m_monitoringActive) {
        return S_OK;
    }
    
    m_monitoringActive = true;
    m_monitoringThread = CreateThread(nullptr, 0, MonitoringThreadProc, this, 0, nullptr);
    
    if (!m_monitoringThread) {
        m_monitoringActive = false;
        return HRESULT_FROM_WIN32(GetLastError());
    }
    
    return S_OK;
}

void CLRReplacementEngine::StopMonitoring() {
    if (m_monitoringActive) {
        m_monitoringActive = false;
        
        if (m_monitoringThread) {
            WaitForSingleObject(m_monitoringThread, 5000);
            CloseHandle(m_monitoringThread);
            m_monitoringThread = nullptr;
        }
    }
}

DWORD WINAPI CLRReplacementEngine::MonitoringThreadProc(LPVOID parameter) {
    CLRReplacementEngine* engine = static_cast<CLRReplacementEngine*>(parameter);
    engine->MonitoringLoop();
    return 0;
}

void CLRReplacementEngine::MonitoringLoop() {
    while (m_monitoringActive) {
        // Scan for new managed processes
        m_legacyDetector->ScanForManagedProcesses();
        
        // Health check on replaced processes
        // Implementation would validate that replaced processes are still healthy
        
        // Sleep before next monitoring cycle
        Sleep(5000); // 5 second intervals
    }
}

// Factory Implementation
CLRReplacementEngine* CLRReplacementFactory::CreateEngine(ReplacementLevel level) {
    auto engine = new CLRReplacementEngine();
    
    HRESULT hr = engine->Initialize(level);
    if (FAILED(hr)) {
        delete engine;
        return nullptr;
    }
    
    return engine;
}

CLRReplacementConfig CLRReplacementFactory::CreateSafeConfiguration() {
    CLRReplacementConfig config;
    config.level = ReplacementLevel::ProcessLevel;
    config.strategy = ReplacementStrategy::Conservative;
    config.enablePerformanceOptimizations = false;
    config.enableCompatibilityMode = true;
    config.enableDetailedLogging = true;
    config.healthCheckInterval = 10000; // 10 seconds
    config.rollbackTimeoutMs = 30000;   // 30 seconds
    
    return config;
}

void CLRReplacementFactory::DestroyEngine(CLRReplacementEngine* engine) {
    delete engine;
}

} // namespace System
} // namespace CLRNet