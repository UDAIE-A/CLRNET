#include "CoreExecutionEngine.h"
#include "TypeSystem.h"
#include "GarbageCollector.h" 
#include "AssemblyLoader.h"
#include "SimpleJIT.h"
#include <iostream>

namespace CLRNet {
namespace Phase1 {

// Global runtime instance
CoreExecutionEngine* g_runtime = nullptr;

CoreExecutionEngine::CoreExecutionEngine() 
    : m_initialized(false)
    , m_exceptionHandler(nullptr) {
}

CoreExecutionEngine::~CoreExecutionEngine() {
    if (m_initialized) {
        Shutdown();
    }
}

bool CoreExecutionEngine::Initialize() {
    if (m_initialized) {
        return true;
    }

    try {
        // Initialize core subsystems
        if (!InitializeSubsystems()) {
            return false;
        }

        m_initialized = true;
        return true;
    }
    catch (...) {
        CleanupSubsystems();
        return false;
    }
}

void CoreExecutionEngine::Shutdown() {
    if (!m_initialized) {
        return;
    }

    CleanupSubsystems();
    m_initialized = false;
}

bool CoreExecutionEngine::InitializeSubsystems() {
    // Initialize type system
    m_typeSystem = std::make_unique<TypeSystem>();
    if (!m_typeSystem->Initialize()) {
        return false;
    }

    // Set global type system instance
    g_typeSystem = m_typeSystem.get();

    // Initialize garbage collector
    m_garbageCollector = std::make_unique<GarbageCollector>();
    if (!m_garbageCollector->Initialize()) {
        return false;
    }

    // Set global garbage collector instance
    g_garbageCollector = m_garbageCollector.get();

    // Initialize assembly loader
    m_assemblyLoader = std::make_unique<AssemblyLoader>(m_typeSystem.get());
    if (!m_assemblyLoader->Initialize()) {
        return false;
    }

    // Set global assembly loader instance  
    g_assemblyLoader = m_assemblyLoader.get();

    // Initialize JIT compiler
    m_jitCompiler = std::make_unique<SimpleJIT>(m_typeSystem.get());
    if (!m_jitCompiler->Initialize()) {
        return false;
    }

    // Set global JIT compiler instance
    g_jitCompiler = m_jitCompiler.get();

    return true;
}

void CoreExecutionEngine::CleanupSubsystems() {
    m_methodCache.clear();
    m_gcRoots.clear();
    
    // Clear global instances
    g_jitCompiler = nullptr;
    g_assemblyLoader = nullptr;
    g_garbageCollector = nullptr;
    g_typeSystem = nullptr;
    
    m_jitCompiler.reset();
    m_assemblyLoader.reset();
    m_garbageCollector.reset();
    m_typeSystem.reset();
}

bool CoreExecutionEngine::LoadAssembly(const std::wstring& assemblyPath) {
    if (!m_initialized || !m_assemblyLoader) {
        return false;
    }

    return m_assemblyLoader->LoadAssembly(assemblyPath);
}

void* CoreExecutionEngine::GetMethodAddress(const std::string& typeName, const std::string& methodName) {
    // Check method cache first
    std::string key = typeName + "::" + methodName;
    auto it = m_methodCache.find(key);
    if (it != m_methodCache.end()) {
        return it->second;
    }

    // Resolve method
    MethodDesc* methodDesc = ResolveMethod(typeName, methodName);
    if (!methodDesc) {
        return nullptr;
    }

    // Compile method if not already compiled
    void* nativeCode = methodDesc->nativeCode;
    if (!nativeCode) {
        nativeCode = CompileMethod(methodDesc);
        methodDesc->nativeCode = nativeCode;
    }

    // Cache the result
    if (nativeCode) {
        m_methodCache[key] = nativeCode;
    }

    return nativeCode;
}

int CoreExecutionEngine::ExecuteMethod(void* methodPtr, void** args, int argCount) {
    if (!methodPtr) {
        return -1; // Invalid method
    }

    try {
        // Simple method invocation - in real implementation this would
        // handle calling conventions, stack frames, etc.
        typedef int (*MethodFunction)();
        MethodFunction func = reinterpret_cast<MethodFunction>(methodPtr);
        
        return func();
    }
    catch (...) {
        return -2; // Exception during execution
    }
}

void* CoreExecutionEngine::AllocateObject(MethodTable* methodTable, size_t size) {
    if (!m_garbageCollector) {
        return nullptr;
    }

    return m_garbageCollector->AllocateObject(size + sizeof(ObjectHeader));
}

void CoreExecutionEngine::RegisterGCRoot(void** root) {
    if (root) {
        m_gcRoots.push_back(root);
    }
}

void CoreExecutionEngine::UnregisterGCRoot(void** root) {
    auto it = std::find(m_gcRoots.begin(), m_gcRoots.end(), root);
    if (it != m_gcRoots.end()) {
        m_gcRoots.erase(it);
    }
}

void CoreExecutionEngine::CollectGarbage() {
    if (m_garbageCollector) {
        m_garbageCollector->Collect(m_gcRoots);
    }
}

void CoreExecutionEngine::ThrowException(const std::string& exceptionType, const std::string& message) {
    // Simple exception throwing - in real implementation would create
    // managed exception objects and unwind stack properly
    std::string fullMessage = exceptionType + ": " + message;
    throw std::runtime_error(fullMessage);
}

void CoreExecutionEngine::SetExceptionHandler(void* handler) {
    m_exceptionHandler = handler;
}

MethodDesc* CoreExecutionEngine::ResolveMethod(const std::string& typeName, const std::string& methodName) {
    if (!m_assemblyLoader) return nullptr;
    
    return m_assemblyLoader->ResolveMethod(typeName, methodName);
}

void* CoreExecutionEngine::CompileMethod(MethodDesc* methodDesc) {
    if (!m_jitCompiler || !methodDesc) return nullptr;
    
    // Get IL code for method
    void* ilCode = nullptr;
    size_t ilSize = 0;
    
    if (methodDesc->ilCode && methodDesc->ilCodeSize > 0) {
        ilCode = methodDesc->ilCode;
        ilSize = methodDesc->ilCodeSize;
    } else if (m_assemblyLoader) {
        // Try to get IL from assembly loader - simplified
        ilCode = m_assemblyLoader->GetMethodIL("", ""); 
        ilSize = 32; // Placeholder size
    }
    
    if (!ilCode || ilSize == 0) return nullptr;
    
    // Compile with JIT
    return m_jitCompiler->CompileMethod(methodDesc, ilCode, ilSize);
}

} // namespace Phase1
} // namespace CLRNet

// C API exports for managed code integration
extern "C" {

__declspec(dllexport) int RuntimeInitialize() {
    if (CLRNet::Phase1::g_runtime) {
        return 1; // Already initialized
    }

    CLRNet::Phase1::g_runtime = new CLRNet::Phase1::CoreExecutionEngine();
    if (CLRNet::Phase1::g_runtime->Initialize()) {
        return 0; // Success
    } else {
        delete CLRNet::Phase1::g_runtime;
        CLRNet::Phase1::g_runtime = nullptr;
        return -1; // Initialization failed
    }
}

__declspec(dllexport) void RuntimeShutdown() {
    if (CLRNet::Phase1::g_runtime) {
        delete CLRNet::Phase1::g_runtime;
        CLRNet::Phase1::g_runtime = nullptr;
    }
}

__declspec(dllexport) int LoadManagedAssembly(const wchar_t* assemblyPath) {
    if (!CLRNet::Phase1::g_runtime) {
        return -1; // Runtime not initialized
    }

    return CLRNet::Phase1::g_runtime->LoadAssembly(assemblyPath) ? 0 : -1;
}

__declspec(dllexport) int ExecuteManagedMethod(const char* typeName, const char* methodName) {
    if (!CLRNet::Phase1::g_runtime) {
        return -1; // Runtime not initialized
    }

    void* methodPtr = CLRNet::Phase1::g_runtime->GetMethodAddress(typeName, methodName);
    if (!methodPtr) {
        return -2; // Method not found
    }

    return CLRNet::Phase1::g_runtime->ExecuteMethod(methodPtr, nullptr, 0);
}