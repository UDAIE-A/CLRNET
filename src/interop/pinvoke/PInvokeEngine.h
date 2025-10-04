#pragma once

#include <windows.h>
#include <map>
#include <string>
#include <vector>
#include <memory>

namespace CLRNet {
namespace Interop {

// Calling conventions supported on ARM32
enum class CallingConvention {
    StdCall,        // Standard calling convention
    CDecl,          // C calling convention  
    FastCall,       // Fast calling convention
    ThisCall,       // C++ member function calling convention
    ARM_AAPCS,      // ARM Architecture Procedure Call Standard
    ARM_AAPCS_VFP   // AAPCS with VFP (floating point) support
};

// Parameter types for P/Invoke marshaling
enum class PInvokeParameterType {
    Void,
    Boolean,
    SByte, Byte,
    Int16, UInt16,
    Int32, UInt32,
    Int64, UInt64,
    Single, Double,
    IntPtr, UIntPtr,
    String_Ansi,
    String_Unicode,
    String_Auto,
    Array,
    Struct,
    Delegate,
    Object
};

// Parameter marshaling flags
enum class MarshalFlags {
    None = 0,
    In = 1,
    Out = 2,
    InOut = 3,
    Optional = 4,
    ByRef = 8
};

// ARM32 register allocation for function calls
struct ARM32CallFrame {
    // ARM32 registers R0-R3 for parameter passing
    DWORD r0, r1, r2, r3;
    
    // Stack parameters (beyond R0-R3)
    std::vector<DWORD> stackParams;
    
    // VFP registers for floating point
    float s0, s1, s2, s3;
    double d0, d1, d2, d3;
    
    // Return value location
    DWORD returnRegister;
    DWORD returnRegisterHigh; // For 64-bit returns
    
    // Stack pointer adjustment
    DWORD stackAdjustment;
};

// P/Invoke parameter descriptor
struct PInvokeParameter {
    PInvokeParameterType type;
    MarshalFlags flags;
    void* data;
    size_t size;
    std::string marshalAs; // Custom marshaling info
    bool isArray;
    size_t arrayLength;
};

// Function signature for P/Invoke
struct PInvokeFunctionSignature {
    std::string libraryName;
    std::string functionName;
    CallingConvention convention;
    PInvokeParameterType returnType;
    std::vector<PInvokeParameter> parameters;
    bool isVarArgs;
    std::string entryPoint; // Alternative entry point name
};

// Native library information
struct NativeLibrary {
    HMODULE handle;
    std::string libraryPath;
    std::string libraryName;
    DWORD loadTime;
    std::map<std::string, FARPROC> functionCache;
};

// Parameter marshaler for different types
class ParameterMarshaler {
private:
    std::map<PInvokeParameterType, size_t> m_typeSizes;

public:
    ParameterMarshaler();
    ~ParameterMarshaler();
    
    // Marshal managed parameter to native
    HRESULT MarshalToNative(const void* managedValue, 
                           const PInvokeParameter& paramInfo,
                           void** nativeValue, size_t* nativeSize);
    
    // Marshal native parameter back to managed
    HRESULT MarshalToManaged(const void* nativeValue,
                            const PInvokeParameter& paramInfo,
                            void** managedValue);
    
    // Marshal string parameters
    HRESULT MarshalString(const WCHAR* managedString, 
                         PInvokeParameterType stringType,
                         void** nativeString, size_t* stringSize);
    
    // Marshal array parameters
    HRESULT MarshalArray(const void* managedArray, size_t elementCount,
                        PInvokeParameterType elementType,
                        void** nativeArray, size_t* arraySize);
    
    // Marshal struct parameters
    HRESULT MarshalStruct(const void* managedStruct, size_t structSize,
                         void** nativeStruct);
    
    // Clean up marshaled parameters
    void CleanupMarshaledParameters(void** nativeParams, 
                                   const std::vector<PInvokeParameter>& params);
    
private:
    void InitializeTypeSizes();
    size_t GetParameterSize(PInvokeParameterType type);
};

// Native library manager
class NativeLibraryManager {
private:
    std::map<std::string, NativeLibrary> m_libraries;
    std::string m_searchPaths;
    CRITICAL_SECTION m_criticalSection;

public:
    NativeLibraryManager();
    ~NativeLibraryManager();
    
    // Load native library
    HRESULT LoadLibrary(const std::string& libraryName, HMODULE* handle);
    
    // Unload native library
    HRESULT UnloadLibrary(const std::string& libraryName);
    
    // Get function address from library
    HRESULT GetProcAddress(const std::string& libraryName,
                          const std::string& functionName,
                          FARPROC* functionAddress);
    
    // Set library search paths
    void SetSearchPaths(const std::string& paths);
    
    // Check if library is loaded
    bool IsLibraryLoaded(const std::string& libraryName) const;
    
    // Get library information
    const NativeLibrary* GetLibraryInfo(const std::string& libraryName) const;
    
private:
    std::string FindLibraryInPaths(const std::string& libraryName);
    void CacheFunction(const std::string& libraryName, 
                      const std::string& functionName, FARPROC address);
};

// Function signature cache
class FunctionSignatureCache {
private:
    std::map<std::string, PInvokeFunctionSignature> m_signatures;
    CRITICAL_SECTION m_criticalSection;

public:
    FunctionSignatureCache();
    ~FunctionSignatureCache();
    
    // Cache function signature
    HRESULT CacheSignature(const std::string& key, 
                          const PInvokeFunctionSignature& signature);
    
    // Get cached signature
    HRESULT GetSignature(const std::string& key, 
                        PInvokeFunctionSignature* signature);
    
    // Remove signature from cache
    void RemoveSignature(const std::string& key);
    
    // Clear all cached signatures
    void ClearCache();
    
    // Generate signature key
    static std::string GenerateKey(const std::string& library, 
                                  const std::string& function);
};

// ARM32 code generation for P/Invoke calls
class ARM32CallGenerator {
private:
    std::vector<BYTE> m_codeBuffer;
    size_t m_codeSize;

public:
    ARM32CallGenerator();
    ~ARM32CallGenerator();
    
    // Prepare call frame for ARM32
    HRESULT PrepareCallFrame(const PInvokeFunctionSignature& signature,
                            PInvokeParameter* parameters,
                            ARM32CallFrame* callFrame);
    
    // Generate ARM32 call code
    HRESULT GenerateCallCode(FARPROC targetFunction,
                            const ARM32CallFrame& callFrame,
                            CallingConvention convention,
                            void** callCode, size_t* codeSize);
    
    // Execute generated call code
    HRESULT ExecuteCall(void* callCode, ARM32CallFrame* callFrame,
                       void* returnValue);
    
private:
    // ARM32 instruction generation helpers
    void EmitMoveRegister(DWORD destReg, DWORD srcReg);
    void EmitLoadImmediate(DWORD reg, DWORD value);
    void EmitPushRegisters(DWORD registerMask);
    void EmitPopRegisters(DWORD registerMask);
    void EmitBranchAndLink(FARPROC target);
    void EmitReturn();
    
    // Register allocation for parameters
    HRESULT AllocateParameters(const std::vector<PInvokeParameter>& parameters,
                              ARM32CallFrame* callFrame);
};

// Main P/Invoke engine
class PInvokeEngine {
private:
    std::unique_ptr<NativeLibraryManager> m_libraryManager;
    std::unique_ptr<FunctionSignatureCache> m_signatureCache;
    std::unique_ptr<ParameterMarshaler> m_marshaler;
    std::unique_ptr<ARM32CallGenerator> m_callGenerator;
    
    bool m_initialized;
    CRITICAL_SECTION m_criticalSection;

public:
    PInvokeEngine();
    ~PInvokeEngine();
    
    // Initialize P/Invoke engine
    HRESULT Initialize();
    
    // Shutdown P/Invoke engine
    void Shutdown();
    
    // Register P/Invoke function signature
    HRESULT RegisterFunction(const PInvokeFunctionSignature& signature);
    
    // Execute P/Invoke call
    HRESULT InvokeFunction(const std::string& libraryName,
                          const std::string& functionName,
                          PInvokeParameter* parameters,
                          UINT32 parameterCount,
                          void* returnValue);
    
    // Execute P/Invoke call with signature
    HRESULT InvokeFunctionWithSignature(const PInvokeFunctionSignature& signature,
                                       PInvokeParameter* parameters,
                                       void* returnValue);
    
    // Load and prepare native library
    HRESULT LoadNativeLibrary(const std::string& libraryName);
    
    // Set library search paths
    void SetLibrarySearchPaths(const std::string& paths);
    
    // Validate function signature
    bool ValidateFunctionSignature(const PInvokeFunctionSignature& signature);
    
    // Handle P/Invoke exceptions
    HRESULT HandlePInvokeException(DWORD exceptionCode, 
                                  EXCEPTION_POINTERS* exceptionInfo);
    
private:
    // Internal helper methods
    HRESULT PrepareFunction(const PInvokeFunctionSignature& signature,
                           FARPROC* functionAddress);
    
    HRESULT ExecuteNativeCall(FARPROC function,
                             const PInvokeFunctionSignature& signature,
                             PInvokeParameter* parameters,
                             void* returnValue);
    
    void CleanupCallResources(PInvokeParameter* parameters, UINT32 count);
};

// P/Invoke engine factory
class PInvokeEngineFactory {
public:
    static PInvokeEngine* CreateInstance();
    static void DestroyInstance(PInvokeEngine* instance);
    
private:
    PInvokeEngineFactory() = default;
};

} // namespace Interop  
} // namespace CLRNet