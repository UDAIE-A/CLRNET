#include "PInvokeEngine.h"
#include <shlwapi.h>
#include <algorithm>

#pragma comment(lib, "shlwapi.lib")

namespace CLRNet {
namespace Interop {

// ParameterMarshaler Implementation
ParameterMarshaler::ParameterMarshaler() {
    InitializeTypeSizes();
}

ParameterMarshaler::~ParameterMarshaler() {
}

void ParameterMarshaler::InitializeTypeSizes() {
    m_typeSizes[PInvokeParameterType::Void] = 0;
    m_typeSizes[PInvokeParameterType::Boolean] = sizeof(BOOL);
    m_typeSizes[PInvokeParameterType::SByte] = sizeof(INT8);
    m_typeSizes[PInvokeParameterType::Byte] = sizeof(UINT8);
    m_typeSizes[PInvokeParameterType::Int16] = sizeof(INT16);
    m_typeSizes[PInvokeParameterType::UInt16] = sizeof(UINT16);
    m_typeSizes[PInvokeParameterType::Int32] = sizeof(INT32);
    m_typeSizes[PInvokeParameterType::UInt32] = sizeof(UINT32);
    m_typeSizes[PInvokeParameterType::Int64] = sizeof(INT64);
    m_typeSizes[PInvokeParameterType::UInt64] = sizeof(UINT64);
    m_typeSizes[PInvokeParameterType::Single] = sizeof(float);
    m_typeSizes[PInvokeParameterType::Double] = sizeof(double);
    m_typeSizes[PInvokeParameterType::IntPtr] = sizeof(void*);
    m_typeSizes[PInvokeParameterType::UIntPtr] = sizeof(void*);
}

HRESULT ParameterMarshaler::MarshalToNative(const void* managedValue, 
                                           const PInvokeParameter& paramInfo,
                                           void** nativeValue, size_t* nativeSize) {
    if (!managedValue || !nativeValue || !nativeSize) {
        return E_INVALIDARG;
    }
    
    size_t typeSize = GetParameterSize(paramInfo.type);
    
    switch (paramInfo.type) {
        case PInvokeParameterType::Boolean:
        case PInvokeParameterType::SByte:
        case PInvokeParameterType::Byte:
        case PInvokeParameterType::Int16:
        case PInvokeParameterType::UInt16:
        case PInvokeParameterType::Int32:
        case PInvokeParameterType::UInt32:
        case PInvokeParameterType::Int64:
        case PInvokeParameterType::UInt64:
        case PInvokeParameterType::Single:
        case PInvokeParameterType::Double:
        case PInvokeParameterType::IntPtr:
        case PInvokeParameterType::UIntPtr: {
            // Simple value types - direct copy
            *nativeValue = malloc(typeSize);
            if (!*nativeValue) {
                return E_OUTOFMEMORY;
            }
            memcpy(*nativeValue, managedValue, typeSize);
            *nativeSize = typeSize;
            break;
        }
        
        case PInvokeParameterType::String_Ansi:
        case PInvokeParameterType::String_Unicode:
        case PInvokeParameterType::String_Auto: {
            return MarshalString(static_cast<const WCHAR*>(managedValue),
                               paramInfo.type, nativeValue, nativeSize);
        }
        
        case PInvokeParameterType::Array: {
            return MarshalArray(managedValue, paramInfo.arrayLength,
                              paramInfo.type, nativeValue, nativeSize);
        }
        
        case PInvokeParameterType::Struct: {
            return MarshalStruct(managedValue, paramInfo.size, nativeValue);
        }
        
        default:
            return E_NOTIMPL;
    }
    
    return S_OK;
}

HRESULT ParameterMarshaler::MarshalString(const WCHAR* managedString, 
                                         PInvokeParameterType stringType,
                                         void** nativeString, size_t* stringSize) {
    if (!managedString || !nativeString || !stringSize) {
        return E_INVALIDARG;
    }
    
    switch (stringType) {
        case PInvokeParameterType::String_Ansi: {
            // Convert Unicode to ANSI
            int ansiLength = WideCharToMultiByte(CP_ACP, 0, managedString, -1,
                                               nullptr, 0, nullptr, nullptr);
            if (ansiLength == 0) {
                return HRESULT_FROM_WIN32(GetLastError());
            }
            
            char* ansiString = static_cast<char*>(malloc(ansiLength));
            if (!ansiString) {
                return E_OUTOFMEMORY;
            }
            
            WideCharToMultiByte(CP_ACP, 0, managedString, -1,
                              ansiString, ansiLength, nullptr, nullptr);
            
            *nativeString = ansiString;
            *stringSize = ansiLength;
            break;
        }
        
        case PInvokeParameterType::String_Unicode: {
            // Direct Unicode copy
            size_t unicodeLength = (wcslen(managedString) + 1) * sizeof(WCHAR);
            WCHAR* unicodeString = static_cast<WCHAR*>(malloc(unicodeLength));
            if (!unicodeString) {
                return E_OUTOFMEMORY;
            }
            
            wcscpy_s(unicodeString, wcslen(managedString) + 1, managedString);
            
            *nativeString = unicodeString;
            *stringSize = unicodeLength;
            break;
        }
        
        case PInvokeParameterType::String_Auto: {
            // Use Unicode on Windows Phone (default)
            return MarshalString(managedString, PInvokeParameterType::String_Unicode,
                               nativeString, stringSize);
        }
        
        default:
            return E_INVALIDARG;
    }
    
    return S_OK;
}

size_t ParameterMarshaler::GetParameterSize(PInvokeParameterType type) {
    auto it = m_typeSizes.find(type);
    return (it != m_typeSizes.end()) ? it->second : 0;
}

// NativeLibraryManager Implementation
NativeLibraryManager::NativeLibraryManager() {
    InitializeCriticalSection(&m_criticalSection);
}

NativeLibraryManager::~NativeLibraryManager() {
    // Unload all libraries
    for (auto& pair : m_libraries) {
        if (pair.second.handle) {
            FreeLibrary(pair.second.handle);
        }
    }
    DeleteCriticalSection(&m_criticalSection);
}

HRESULT NativeLibraryManager::LoadLibrary(const std::string& libraryName, HMODULE* handle) {
    if (libraryName.empty() || !handle) {
        return E_INVALIDARG;
    }
    
    EnterCriticalSection(&m_criticalSection);
    
    // Check if already loaded
    auto it = m_libraries.find(libraryName);
    if (it != m_libraries.end()) {
        *handle = it->second.handle;
        LeaveCriticalSection(&m_criticalSection);
        return S_OK;
    }
    
    // Find library in search paths
    std::string libraryPath = FindLibraryInPaths(libraryName);
    if (libraryPath.empty()) {
        libraryPath = libraryName; // Try as-is
    }
    
    // Convert to wide string for LoadLibrary
    std::wstring wLibraryPath(libraryPath.begin(), libraryPath.end());
    HMODULE hModule = ::LoadLibraryW(wLibraryPath.c_str());
    
    if (!hModule) {
        DWORD error = GetLastError();
        LeaveCriticalSection(&m_criticalSection);
        return HRESULT_FROM_WIN32(error);
    }
    
    // Create library entry
    NativeLibrary library;
    library.handle = hModule;
    library.libraryPath = libraryPath;
    library.libraryName = libraryName;
    library.loadTime = GetTickCount();
    
    m_libraries[libraryName] = library;
    *handle = hModule;
    
    LeaveCriticalSection(&m_criticalSection);
    return S_OK;
}

HRESULT NativeLibraryManager::GetProcAddress(const std::string& libraryName,
                                            const std::string& functionName,
                                            FARPROC* functionAddress) {
    if (libraryName.empty() || functionName.empty() || !functionAddress) {
        return E_INVALIDARG;
    }
    
    EnterCriticalSection(&m_criticalSection);
    
    auto it = m_libraries.find(libraryName);
    if (it == m_libraries.end()) {
        LeaveCriticalSection(&m_criticalSection);
        return HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND);
    }
    
    // Check function cache first
    auto funcIt = it->second.functionCache.find(functionName);
    if (funcIt != it->second.functionCache.end()) {
        *functionAddress = funcIt->second;
        LeaveCriticalSection(&m_criticalSection);
        return S_OK;
    }
    
    // Get function address
    FARPROC proc = ::GetProcAddress(it->second.handle, functionName.c_str());
    if (!proc) {
        DWORD error = GetLastError();
        LeaveCriticalSection(&m_criticalSection);
        return HRESULT_FROM_WIN32(error);
    }
    
    // Cache the function
    it->second.functionCache[functionName] = proc;
    *functionAddress = proc;
    
    LeaveCriticalSection(&m_criticalSection);
    return S_OK;
}

std::string NativeLibraryManager::FindLibraryInPaths(const std::string& libraryName) {
    // Simple implementation - in a full version, this would search multiple paths
    if (PathFileExistsA(libraryName.c_str())) {
        return libraryName;
    }
    
    // Try system32 directory
    char systemDir[MAX_PATH];
    if (GetSystemDirectoryA(systemDir, MAX_PATH)) {
        std::string fullPath = std::string(systemDir) + "\\" + libraryName;
        if (PathFileExistsA(fullPath.c_str())) {
            return fullPath;
        }
    }
    
    return "";
}

// ARM32CallGenerator Implementation
ARM32CallGenerator::ARM32CallGenerator() : m_codeSize(0) {
}

ARM32CallGenerator::~ARM32CallGenerator() {
}

HRESULT ARM32CallGenerator::PrepareCallFrame(const PInvokeFunctionSignature& signature,
                                            PInvokeParameter* parameters,
                                            ARM32CallFrame* callFrame) {
    if (!parameters || !callFrame) {
        return E_INVALIDARG;
    }
    
    // Initialize call frame
    memset(callFrame, 0, sizeof(ARM32CallFrame));
    
    // ARM32 AAPCS: First 4 parameters in R0-R3, rest on stack
    size_t paramIndex = 0;
    DWORD* registers[] = { &callFrame->r0, &callFrame->r1, &callFrame->r2, &callFrame->r3 };
    
    for (size_t i = 0; i < signature.parameters.size() && paramIndex < 4; i++) {
        const auto& param = signature.parameters[i];
        
        switch (param.type) {
            case PInvokeParameterType::Int32:
            case PInvokeParameterType::UInt32:
            case PInvokeParameterType::IntPtr:
            case PInvokeParameterType::UIntPtr:
                if (param.data && paramIndex < 4) {
                    *registers[paramIndex] = *static_cast<DWORD*>(param.data);
                    paramIndex++;
                }
                break;
                
            case PInvokeParameterType::Int64:
            case PInvokeParameterType::UInt64:
                // 64-bit values use two registers
                if (param.data && paramIndex < 3) {
                    UINT64 value = *static_cast<UINT64*>(param.data);
                    *registers[paramIndex] = static_cast<DWORD>(value & 0xFFFFFFFF);
                    *registers[paramIndex + 1] = static_cast<DWORD>(value >> 32);
                    paramIndex += 2;
                }
                break;
                
            case PInvokeParameterType::Single:
                // Single precision floating point in VFP registers
                if (param.data) {
                    callFrame->s0 = *static_cast<float*>(param.data);
                }
                break;
                
            default:
                // For now, treat other types as 32-bit values
                if (param.data && paramIndex < 4) {
                    *registers[paramIndex] = reinterpret_cast<DWORD>(param.data);
                    paramIndex++;
                }
                break;
        }
    }
    
    // Remaining parameters go on stack
    for (size_t i = paramIndex; i < signature.parameters.size(); i++) {
        const auto& param = signature.parameters[i];
        if (param.data) {
            callFrame->stackParams.push_back(reinterpret_cast<DWORD>(param.data));
        }
    }
    
    return S_OK;
}

// PInvokeEngine Implementation
PInvokeEngine::PInvokeEngine() : m_initialized(false) {
    InitializeCriticalSection(&m_criticalSection);
    m_libraryManager = std::make_unique<NativeLibraryManager>();
    m_signatureCache = std::make_unique<FunctionSignatureCache>();
    m_marshaler = std::make_unique<ParameterMarshaler>();
    m_callGenerator = std::make_unique<ARM32CallGenerator>();
}

PInvokeEngine::~PInvokeEngine() {
    Shutdown();
    DeleteCriticalSection(&m_criticalSection);
}

HRESULT PInvokeEngine::Initialize() {
    if (m_initialized) {
        return S_OK;
    }
    
    // Set default library search paths for Windows Phone 8.1
    m_libraryManager->SetSearchPaths("\\Windows\\System32;\\Windows");
    
    m_initialized = true;
    return S_OK;
}

void PInvokeEngine::Shutdown() {
    if (m_initialized) {
        m_callGenerator.reset();
        m_marshaler.reset();
        m_signatureCache.reset();
        m_libraryManager.reset();
        m_initialized = false;
    }
}

HRESULT PInvokeEngine::InvokeFunction(const std::string& libraryName,
                                     const std::string& functionName,
                                     PInvokeParameter* parameters,
                                     UINT32 parameterCount,
                                     void* returnValue) {
    if (!m_initialized) {
        return E_NOT_VALID_STATE;
    }
    
    // Get cached signature or create default
    std::string signatureKey = FunctionSignatureCache::GenerateKey(libraryName, functionName);
    PInvokeFunctionSignature signature;
    
    HRESULT hr = m_signatureCache->GetSignature(signatureKey, &signature);
    if (FAILED(hr)) {
        // Create default signature
        signature.libraryName = libraryName;
        signature.functionName = functionName;
        signature.convention = CallingConvention::StdCall;
        signature.returnType = PInvokeParameterType::Int32;
        signature.isVarArgs = false;
        
        // Add parameters to signature
        for (UINT32 i = 0; i < parameterCount; i++) {
            signature.parameters.push_back(parameters[i]);
        }
    }
    
    return InvokeFunctionWithSignature(signature, parameters, returnValue);
}

HRESULT PInvokeEngine::InvokeFunctionWithSignature(const PInvokeFunctionSignature& signature,
                                                  PInvokeParameter* parameters,
                                                  void* returnValue) {
    if (!m_initialized) {
        return E_NOT_VALID_STATE;
    }
    
    // Load library if needed
    HMODULE libraryHandle;
    HRESULT hr = m_libraryManager->LoadLibrary(signature.libraryName, &libraryHandle);
    if (FAILED(hr)) {
        return hr;
    }
    
    // Get function address
    FARPROC functionAddress;
    hr = m_libraryManager->GetProcAddress(signature.libraryName, 
                                         signature.functionName, &functionAddress);
    if (FAILED(hr)) {
        return hr;
    }
    
    // Execute the native call
    return ExecuteNativeCall(functionAddress, signature, parameters, returnValue);
}

HRESULT PInvokeEngine::ExecuteNativeCall(FARPROC function,
                                        const PInvokeFunctionSignature& signature,
                                        PInvokeParameter* parameters,
                                        void* returnValue) {
    // Prepare call frame
    ARM32CallFrame callFrame;
    HRESULT hr = m_callGenerator->PrepareCallFrame(signature, parameters, &callFrame);
    if (FAILED(hr)) {
        return hr;
    }
    
    // For simplified implementation, we'll use direct function calls
    // In a full implementation, this would generate ARM32 assembly code
    
    __try {
        // Simplified call - assumes stdcall convention with up to 4 parameters
        DWORD result = 0;
        
        switch (signature.parameters.size()) {
            case 0:
                result = reinterpret_cast<DWORD(*)()>(function)();
                break;
            case 1:
                result = reinterpret_cast<DWORD(*)(DWORD)>(function)(callFrame.r0);
                break;
            case 2:
                result = reinterpret_cast<DWORD(*)(DWORD, DWORD)>(function)(
                    callFrame.r0, callFrame.r1);
                break;
            case 3:
                result = reinterpret_cast<DWORD(*)(DWORD, DWORD, DWORD)>(function)(
                    callFrame.r0, callFrame.r1, callFrame.r2);
                break;
            case 4:
                result = reinterpret_cast<DWORD(*)(DWORD, DWORD, DWORD, DWORD)>(function)(
                    callFrame.r0, callFrame.r1, callFrame.r2, callFrame.r3);
                break;
            default:
                return E_NOTIMPL; // More complex cases need stack parameter handling
        }
        
        // Copy return value
        if (returnValue && signature.returnType != PInvokeParameterType::Void) {
            switch (signature.returnType) {
                case PInvokeParameterType::Int32:
                case PInvokeParameterType::UInt32:
                    *static_cast<DWORD*>(returnValue) = result;
                    break;
                default:
                    *static_cast<DWORD*>(returnValue) = result;
                    break;
            }
        }
        
        return S_OK;
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        return E_FAIL;
    }
}

// FunctionSignatureCache Implementation
FunctionSignatureCache::FunctionSignatureCache() {
    InitializeCriticalSection(&m_criticalSection);
}

FunctionSignatureCache::~FunctionSignatureCache() {
    ClearCache();
    DeleteCriticalSection(&m_criticalSection);
}

std::string FunctionSignatureCache::GenerateKey(const std::string& library, 
                                               const std::string& function) {
    return library + "::" + function;
}

HRESULT FunctionSignatureCache::CacheSignature(const std::string& key, 
                                              const PInvokeFunctionSignature& signature) {
    EnterCriticalSection(&m_criticalSection);
    m_signatures[key] = signature;
    LeaveCriticalSection(&m_criticalSection);
    return S_OK;
}

HRESULT FunctionSignatureCache::GetSignature(const std::string& key, 
                                            PInvokeFunctionSignature* signature) {
    if (!signature) {
        return E_INVALIDARG;
    }
    
    EnterCriticalSection(&m_criticalSection);
    auto it = m_signatures.find(key);
    if (it != m_signatures.end()) {
        *signature = it->second;
        LeaveCriticalSection(&m_criticalSection);
        return S_OK;
    }
    LeaveCriticalSection(&m_criticalSection);
    return E_NOT_FOUND;
}

// Factory Implementation
PInvokeEngine* PInvokeEngineFactory::CreateInstance() {
    return new PInvokeEngine();
}

void PInvokeEngineFactory::DestroyInstance(PInvokeEngine* instance) {
    delete instance;
}

} // namespace Interop
} // namespace CLRNet