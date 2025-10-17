#pragma once

// Core execution engine for Phase 1 userland CLR runtime
// Provides basic .NET execution without system integration

#ifndef CORE_EXECUTION_ENGINE_H
#define CORE_EXECUTION_ENGINE_H

#include <windows.h>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

#include "RuntimeTypes.h"

namespace CLRNet {
namespace Phase1 {

// Forward declarations
class TypeSystem;
struct MethodTable; // Changed from class to struct
class GarbageCollector;
class AssemblyLoader;
class SimpleJIT;

// Core execution engine class
class CoreExecutionEngine {
public:
    CoreExecutionEngine();
    ~CoreExecutionEngine();

    // Runtime initialization
    bool Initialize();
    void Shutdown();

    // Assembly and type management
    bool LoadAssembly(const std::wstring& assemblyPath);
    void* GetMethodAddress(const std::string& typeName, const std::string& methodName);
    
    // Execution control
    int ExecuteMethod(void* methodPtr, void** args, int argCount);
    void* AllocateObject(MethodTable* methodTable, size_t size);
    
    // Memory management integration
    void RegisterGCRoot(void** root);
    void UnregisterGCRoot(void** root);
    void CollectGarbage();

    // Exception handling
    void ThrowException(const std::string& exceptionType, const std::string& message);
    void SetExceptionHandler(void* handler);

private:
    // Core components
    std::unique_ptr<TypeSystem> m_typeSystem;
    std::unique_ptr<GarbageCollector> m_garbageCollector;
    std::unique_ptr<AssemblyLoader> m_assemblyLoader;
    std::unique_ptr<SimpleJIT> m_jitCompiler;
    
    // Runtime state
    bool m_initialized;
    std::vector<void**> m_gcRoots;
    void* m_exceptionHandler;
    
    // Method cache for performance
    std::unordered_map<std::string, void*> m_methodCache;

    // Internal helpers
    bool InitializeSubsystems();
    void CleanupSubsystems();
    MethodDesc* ResolveMethod(const std::string& typeName, const std::string& methodName);
    void* CompileMethod(MethodDesc* methodDesc);
};

// Global runtime instance (singleton pattern for simplicity)
extern CoreExecutionEngine* g_runtime;

// Runtime entry points for managed code
extern "C" {
    __declspec(dllexport) int RuntimeInitialize();
    __declspec(dllexport) void RuntimeShutdown();
    __declspec(dllexport) int LoadManagedAssembly(const wchar_t* assemblyPath);
    __declspec(dllexport) int ExecuteManagedMethod(const char* typeName, const char* methodName);
}

} // namespace Phase1
} // namespace CLRNet

#endif // CORE_EXECUTION_ENGINE_H