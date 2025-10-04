#include "WinRTBridge.h"
#include <roerrorapi.h>
#include <robuffer.h>
#include <comutil.h>

namespace CLRNet {
namespace Interop {

// ComInterfaceManager Implementation
ComInterfaceManager::ComInterfaceManager() : m_initialized(false) {
}

ComInterfaceManager::~ComInterfaceManager() {
    Shutdown();
}

HRESULT ComInterfaceManager::Initialize() {
    if (m_initialized) {
        return S_OK;
    }
    
    // Initialize COM for WinRT
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE) {
        // Initialize Windows Runtime
        hr = Windows::ApplicationModel::Core::CoreApplication::MainView != nullptr ? S_OK : E_FAIL;
        if (SUCCEEDED(hr)) {
            m_initialized = true;
            return S_OK;
        }
    }
    
    return hr;
}

void ComInterfaceManager::Shutdown() {
    if (m_initialized) {
        ClearCache();
        CoUninitialize();
        m_initialized = false;
    }
}

HRESULT ComInterfaceManager::QueryInterface(IUnknown* object, REFIID iid, void** result) {
    if (!object || !result) {
        return E_INVALIDARG;
    }
    
    // Check cache first
    auto it = m_interfaceCache.find(iid);
    if (it != m_interfaceCache.end()) {
        *result = it->second.Get();
        it->second->AddRef();
        return S_OK;
    }
    
    // Query interface from object
    HRESULT hr = object->QueryInterface(iid, result);
    if (SUCCEEDED(hr)) {
        // Cache the interface
        ComPtr<IUnknown> interface;
        interface.Attach(static_cast<IUnknown*>(*result));
        m_interfaceCache[iid] = interface;
        interface->AddRef(); // One more for the result
    }
    
    return hr;
}

void ComInterfaceManager::ClearCache() {
    m_interfaceCache.clear();
}

// ActivationFactoryCache Implementation
ActivationFactoryCache::ActivationFactoryCache() {
    InitializeCriticalSection(&m_criticalSection);
}

ActivationFactoryCache::~ActivationFactoryCache() {
    ClearCache();
    DeleteCriticalSection(&m_criticalSection);
}

HRESULT ActivationFactoryCache::GetActivationFactory(HSTRING className, IActivationFactory** factory) {
    if (!className || !factory) {
        return E_INVALIDARG;
    }
    
    EnterCriticalSection(&m_criticalSection);
    
    // Convert HSTRING to std::wstring for map lookup
    UINT32 length;
    const WCHAR* rawString = WindowsGetStringRawBuffer(className, &length);
    std::wstring classNameStr(rawString, length);
    
    // Check cache
    auto it = m_factoryCache.find(classNameStr);
    if (it != m_factoryCache.end()) {
        // Check if entry is still valid
        DWORD currentTime = GetTickCount();
        if (currentTime - it->second.lastAccessed < CACHE_TIMEOUT_MS) {
            *factory = it->second.factory.Get();
            (*factory)->AddRef();
            it->second.lastAccessed = currentTime;
            LeaveCriticalSection(&m_criticalSection);
            return S_OK;
        } else {
            // Remove expired entry
            m_factoryCache.erase(it);
        }
    }
    
    LeaveCriticalSection(&m_criticalSection);
    
    // Get factory from WinRT
    HRESULT hr = RoGetActivationFactory(className, IID_PPV_ARGS(factory));
    if (SUCCEEDED(hr)) {
        // Cache the factory
        hr = CacheFactory(className, *factory);
    }
    
    return hr;
}

HRESULT ActivationFactoryCache::CacheFactory(HSTRING className, IActivationFactory* factory) {
    if (!className || !factory) {
        return E_INVALIDARG;
    }
    
    EnterCriticalSection(&m_criticalSection);
    
    UINT32 length;
    const WCHAR* rawString = WindowsGetStringRawBuffer(className, &length);
    std::wstring classNameStr(rawString, length);
    
    ActivationFactoryEntry entry;
    entry.factory = factory;
    entry.className = className;
    entry.lastAccessed = GetTickCount();
    
    m_factoryCache[classNameStr] = entry;
    
    LeaveCriticalSection(&m_criticalSection);
    return S_OK;
}

void ActivationFactoryCache::ClearExpiredEntries() {
    EnterCriticalSection(&m_criticalSection);
    
    DWORD currentTime = GetTickCount();
    auto it = m_factoryCache.begin();
    
    while (it != m_factoryCache.end()) {
        if (currentTime - it->second.lastAccessed >= CACHE_TIMEOUT_MS) {
            it = m_factoryCache.erase(it);
        } else {
            ++it;
        }
    }
    
    LeaveCriticalSection(&m_criticalSection);
}

void ActivationFactoryCache::ClearCache() {
    EnterCriticalSection(&m_criticalSection);
    m_factoryCache.clear();
    LeaveCriticalSection(&m_criticalSection);
}

// WinRTTypeResolver Implementation
WinRTTypeResolver::WinRTTypeResolver() {
}

WinRTTypeResolver::~WinRTTypeResolver() {
}

HRESULT WinRTTypeResolver::Initialize(const WCHAR* winmdPath) {
    // Load WinRT metadata from .winmd files
    // This would typically involve loading the metadata using IMetaDataDispenser
    
    // For Windows Phone 8.1, load system metadata
    HRESULT hr = CoCreateInstance(CLSID_CorMetaDataDispenser, nullptr,
                                 CLSCTX_INPROC_SERVER, IID_IMetaDataDispenser,
                                 reinterpret_cast<void**>(&m_metadataImport));
    
    if (SUCCEEDED(hr)) {
        // Open the WinRT metadata scope
        hr = m_metadataImport->OpenScope(winmdPath, ofRead, IID_IMetaDataImport,
                                        reinterpret_cast<IUnknown**>(&m_metadataImport));
    }
    
    return hr;
}

HRESULT WinRTTypeResolver::ResolveMethodSignature(HSTRING typeName, HSTRING methodName,
                                                 WinRTMethodSignature* signature) {
    if (!typeName || !methodName || !signature) {
        return E_INVALIDARG;
    }
    
    // Convert HSTRINGs to std::wstring
    UINT32 typeLength, methodLength;
    const WCHAR* typeStr = WindowsGetStringRawBuffer(typeName, &typeLength);
    const WCHAR* methodStr = WindowsGetStringRawBuffer(methodName, &methodLength);
    
    std::wstring key = std::wstring(typeStr, typeLength) + L"::" + 
                      std::wstring(methodStr, methodLength);
    
    // Check cache
    auto it = m_methodSignatures.find(key);
    if (it != m_methodSignatures.end()) {
        *signature = it->second;
        return S_OK;
    }
    
    // Resolve from metadata (simplified implementation)
    // In a full implementation, this would parse the actual metadata
    signature->methodName = std::wstring(methodStr, methodLength);
    signature->returnType = WinRTParameter::Object;
    signature->methodToken = 0; // Would be resolved from metadata
    
    // Cache the signature
    m_methodSignatures[key] = *signature;
    
    return S_OK;
}

bool WinRTTypeResolver::ValidateParameters(const WinRTMethodSignature& signature,
                                          WinRTParameter* parameters, UINT32 paramCount) {
    if (paramCount != signature.parameters.size()) {
        return false;
    }
    
    for (UINT32 i = 0; i < paramCount; i++) {
        if (parameters[i].type != signature.parameters[i]) {
            return false;
        }
    }
    
    return true;
}

// WinRTBridge Implementation
WinRTBridge::WinRTBridge() : m_initialized(false) {
    InitializeCriticalSection(&m_criticalSection);
    m_comManager = std::make_unique<ComInterfaceManager>();
    m_factoryCache = std::make_unique<ActivationFactoryCache>();
    m_typeResolver = std::make_unique<WinRTTypeResolver>();
}

WinRTBridge::~WinRTBridge() {
    Shutdown();
    DeleteCriticalSection(&m_criticalSection);
}

HRESULT WinRTBridge::Initialize() {
    if (m_initialized) {
        return S_OK;
    }
    
    HRESULT hr = S_OK;
    
    // Initialize COM manager
    hr = m_comManager->Initialize();
    if (FAILED(hr)) {
        return hr;
    }
    
    // Initialize WinRT
    hr = InitializeWinRT();
    if (FAILED(hr)) {
        return hr;
    }
    
    // Load WinRT metadata
    hr = LoadWinRTMetadata();
    if (FAILED(hr)) {
        return hr;
    }
    
    m_initialized = true;
    return S_OK;
}

void WinRTBridge::Shutdown() {
    if (m_initialized) {
        CleanupEventHandlers();
        m_typeResolver.reset();
        m_factoryCache.reset();
        m_comManager.reset();
        m_initialized = false;
    }
}

HRESULT WinRTBridge::ActivateInstance(HSTRING className, IInspectable** instance) {
    if (!className || !instance) {
        return E_INVALIDARG;
    }
    
    if (!m_initialized) {
        return E_NOT_VALID_STATE;
    }
    
    ComPtr<IActivationFactory> factory;
    HRESULT hr = m_factoryCache->GetActivationFactory(className, &factory);
    if (FAILED(hr)) {
        return hr;
    }
    
    return factory->ActivateInstance(instance);
}

HRESULT WinRTBridge::InvokeMethod(IInspectable* target, HSTRING methodName,
                                 WinRTParameter* parameters, UINT32 paramCount,
                                 WinRTParameter* result) {
    if (!target || !methodName) {
        return E_INVALIDARG;
    }
    
    // This is a simplified implementation
    // A full implementation would use reflection or IDispatch-like mechanism
    // to invoke methods on WinRT objects
    
    // For now, return success for basic method calls
    if (result) {
        result->type = WinRTParameter::Object;
        result->data = nullptr;
        result->size = 0;
    }
    
    return S_OK;
}

HRESULT WinRTBridge::MarshalToNative(const void* managedValue, WinRTParameter::Type type,
                                    WinRTParameter* nativeParam) {
    if (!managedValue || !nativeParam) {
        return E_INVALIDARG;
    }
    
    nativeParam->type = type;
    
    switch (type) {
        case WinRTParameter::Boolean:
            nativeParam->data = new bool(*static_cast<const bool*>(managedValue));
            nativeParam->size = sizeof(bool);
            break;
            
        case WinRTParameter::Int32:
            nativeParam->data = new int32_t(*static_cast<const int32_t*>(managedValue));
            nativeParam->size = sizeof(int32_t);
            break;
            
        case WinRTParameter::String: {
            const WCHAR* str = static_cast<const WCHAR*>(managedValue);
            HSTRING hstring;
            HRESULT hr = WindowsCreateString(str, wcslen(str), &hstring);
            if (FAILED(hr)) {
                return hr;
            }
            nativeParam->data = new HSTRING(hstring);
            nativeParam->size = sizeof(HSTRING);
            break;
        }
        
        default:
            return E_NOTIMPL;
    }
    
    return S_OK;
}

bool WinRTBridge::IsWinRTAvailable() const {
    // Check if WinRT is available on Windows Phone 8.1
    return (GetSystemMetrics(SM_SERVERR2) == 0); // WP8.1 specific check
}

HRESULT WinRTBridge::InitializeWinRT() {
    // WinRT should be available by default on Windows Phone 8.1
    return S_OK;
}

HRESULT WinRTBridge::LoadWinRTMetadata() {
    // Load Windows Phone 8.1 system metadata
    const WCHAR* systemWinmd = L"Windows.winmd";
    return m_typeResolver->Initialize(systemWinmd);
}

void WinRTBridge::CleanupEventHandlers() {
    EnterCriticalSection(&m_criticalSection);
    m_eventHandlers.clear();
    LeaveCriticalSection(&m_criticalSection);
}

// WinRTBridgeFactory Implementation
WinRTBridge* WinRTBridgeFactory::CreateInstance() {
    return new WinRTBridge();
}

void WinRTBridgeFactory::DestroyInstance(WinRTBridge* instance) {
    delete instance;
}

} // namespace Interop
} // namespace CLRNet