#pragma once

#include <windows.h>
#include <inspectable.h>
#include <roapi.h>
#include <wrl/client.h>
#include <wrl/implements.h>
#include <map>
#include <string>
#include <vector>

using namespace Microsoft::WRL;

namespace CLRNet {
namespace Interop {

// Forward declarations
class WinRTTypeResolver;
class ActivationFactoryCache;
class ComInterfaceManager;

// WinRT parameter marshaling information
struct WinRTParameter {
    enum Type {
        Boolean,
        Int32,
        UInt32,
        Int64,
        UInt64,
        Single,
        Double,
        String,
        Object,
        Array
    };
    
    Type type;
    void* data;
    size_t size;
    bool isOutput;
};

// Event callback function pointer
typedef HRESULT(*EventCallback)(IInspectable* sender, IInspectable* args);

// WinRT method signature
struct WinRTMethodSignature {
    std::wstring methodName;
    std::vector<WinRTParameter::Type> parameters;
    WinRTParameter::Type returnType;
    UINT32 methodToken;
};

// Activation factory cache entry
struct ActivationFactoryEntry {
    ComPtr<IActivationFactory> factory;
    HSTRING className;
    DWORD lastAccessed;
};

// COM interface management
class ComInterfaceManager {
private:
    std::map<IID, ComPtr<IUnknown>> m_interfaceCache;
    bool m_initialized;

public:
    ComInterfaceManager();
    ~ComInterfaceManager();
    
    // Initialize COM subsystem
    HRESULT Initialize();
    
    // Shutdown COM subsystem
    void Shutdown();
    
    // Query interface with caching
    HRESULT QueryInterface(IUnknown* object, REFIID iid, void** result);
    
    // Release cached interfaces
    void ClearCache();
    
    // Check if COM is initialized
    bool IsInitialized() const { return m_initialized; }
};

// WinRT activation factory cache
class ActivationFactoryCache {
private:
    std::map<std::wstring, ActivationFactoryEntry> m_factoryCache;
    CRITICAL_SECTION m_criticalSection;
    static const DWORD CACHE_TIMEOUT_MS = 300000; // 5 minutes

public:
    ActivationFactoryCache();
    ~ActivationFactoryCache();
    
    // Get activation factory for class
    HRESULT GetActivationFactory(HSTRING className, IActivationFactory** factory);
    
    // Cache activation factory
    HRESULT CacheFactory(HSTRING className, IActivationFactory* factory);
    
    // Clear expired cache entries
    void ClearExpiredEntries();
    
    // Clear all cache entries
    void ClearCache();
};

// WinRT type resolver
class WinRTTypeResolver {
private:
    std::map<std::wstring, WinRTMethodSignature> m_methodSignatures;
    ComPtr<IMetaDataImport> m_metadataImport;

public:
    WinRTTypeResolver();
    ~WinRTTypeResolver();
    
    // Initialize with WinRT metadata
    HRESULT Initialize(const WCHAR* winmdPath);
    
    // Resolve method signature
    HRESULT ResolveMethodSignature(HSTRING typeName, HSTRING methodName,
                                  WinRTMethodSignature* signature);
    
    // Get type information
    HRESULT GetTypeInfo(HSTRING typeName, IInspectable** typeInfo);
    
    // Validate parameter types
    bool ValidateParameters(const WinRTMethodSignature& signature,
                          WinRTParameter* parameters, UINT32 paramCount);
};

// Main WinRT Bridge implementation
class WinRTBridge {
private:
    // Component managers
    std::unique_ptr<ComInterfaceManager> m_comManager;
    std::unique_ptr<ActivationFactoryCache> m_factoryCache;
    std::unique_ptr<WinRTTypeResolver> m_typeResolver;
    
    // WinRT runtime state
    bool m_initialized;
    CRITICAL_SECTION m_criticalSection;
    
    // Event handlers
    std::map<std::pair<IInspectable*, std::wstring>, EventCallback> m_eventHandlers;

public:
    WinRTBridge();
    ~WinRTBridge();
    
    // Initialize WinRT subsystem
    HRESULT Initialize();
    
    // Shutdown WinRT subsystem
    void Shutdown();
    
    // Activate WinRT instance
    HRESULT ActivateInstance(HSTRING className, IInspectable** instance);
    
    // Create WinRT instance with parameters
    HRESULT CreateInstance(HSTRING className, WinRTParameter* parameters,
                          UINT32 paramCount, IInspectable** instance);
    
    // Invoke WinRT method
    HRESULT InvokeMethod(IInspectable* target, HSTRING methodName,
                        WinRTParameter* parameters, UINT32 paramCount,
                        WinRTParameter* result);
    
    // Get WinRT property value
    HRESULT GetProperty(IInspectable* target, HSTRING propertyName,
                       WinRTParameter* result);
    
    // Set WinRT property value
    HRESULT SetProperty(IInspectable* target, HSTRING propertyName,
                       const WinRTParameter& value);
    
    // Register event handler
    HRESULT RegisterEventHandler(IInspectable* source, HSTRING eventName,
                                EventCallback callback);
    
    // Unregister event handler
    HRESULT UnregisterEventHandler(IInspectable* source, HSTRING eventName);
    
    // Marshal parameter from managed to native
    HRESULT MarshalToNative(const void* managedValue, WinRTParameter::Type type,
                           WinRTParameter* nativeParam);
    
    // Marshal parameter from native to managed
    HRESULT MarshalToManaged(const WinRTParameter& nativeParam, void** managedValue);
    
    // Handle WinRT exceptions
    HRESULT HandleWinRTException(HRESULT hr, IRestrictedErrorInfo** errorInfo);
    
    // Check if WinRT is available
    bool IsWinRTAvailable() const;
    
private:
    // Internal helper methods
    HRESULT InitializeWinRT();
    HRESULT LoadWinRTMetadata();
    void CleanupEventHandlers();
    
    // Parameter marshaling helpers
    HRESULT MarshalString(const WCHAR* str, HSTRING* hstring);
    HRESULT UnmarshalString(HSTRING hstring, WCHAR** str);
    HRESULT MarshalArray(const void* array, UINT32 count, 
                        WinRTParameter::Type elementType, void** result);
};

// WinRT Bridge factory
class WinRTBridgeFactory {
public:
    static WinRTBridge* CreateInstance();
    static void DestroyInstance(WinRTBridge* instance);
    
private:
    WinRTBridgeFactory() = default;
};

} // namespace Interop
} // namespace CLRNet