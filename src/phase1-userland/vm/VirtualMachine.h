#pragma once

// CLRNET Track B - Userspace IL virtual machine
// Provides IL interpretation, bytecode caching, and host syscall bridging

#ifndef CLRNET_VM_VIRTUAL_MACHINE_H
#define CLRNET_VM_VIRTUAL_MACHINE_H

#include <windows.h>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace CLRNet {
namespace Phase1 {
namespace VM {

class BytecodeCache;
class BytecodeCompiler;
struct VmProgram;
struct VmInstruction;
struct VmExecutionContext;
struct VmExecutionResult;
struct VmCallSite;

// Opcodes understood by the VM bytecode interpreter
enum class VmOpcode : uint8_t {
    Nop = 0x00,
    LoadArgument,
    LoadLocal,
    StoreLocal,
    StoreArgument,
    LoadField,
    StoreField,
    LoadConstantI4,
    LoadConstantI8,
    LoadString,
    LoadNull,
    Box,
    UnboxAny,
    CastClass,
    Add,
    Subtract,
    Multiply,
    Divide,
    Branch,
    BranchIfTrue,
    BranchIfFalse,
    CompareEqual,
    CompareNotEqual,
    CompareGreaterThan,
    CompareLessThan,
    Call,
    CallVirtual,
    HostCall,
    NewObject,
    Return
};

// Result of executing bytecode in the VM
struct VmExecutionResult {
    bool success;
    uint32_t stepsExecuted;
    void* returnValue;
    std::wstring failureReason;

    VmExecutionResult()
        : success(false)
        , stepsExecuted(0)
        , returnValue(nullptr) {}
};

// Represents a value on the evaluation stack or in locals
struct VmValue {
    enum class Kind : uint8_t {
        Uninitialized,
        Int32,
        Int64,
        Float,
        Double,
        Object,
        ManagedPointer,
        Null
    };

    Kind kind;
    union {
        int32_t i32;
        int64_t i64;
        float f32;
        double f64;
        void* object;
    } data;

    VmValue()
        : kind(Kind::Uninitialized) {
        data.i64 = 0;
    }

    explicit VmValue(int32_t value) {
        kind = Kind::Int32;
        data.i32 = value;
    }

    explicit VmValue(int64_t value) {
        kind = Kind::Int64;
        data.i64 = value;
    }

    explicit VmValue(double value) {
        kind = Kind::Double;
        data.f64 = value;
    }

    explicit VmValue(void* value, Kind valueKind = Kind::Object) {
        kind = valueKind;
        data.object = value;
    }
};

struct VmExecutionContextNative {
    VmValue* arguments;
    uint32_t argumentCount;
    VmValue* locals;
    uint32_t localCount;
    uint64_t timeBudgetTicks;
    size_t memoryBudgetBytes;
    const wchar_t* sandboxNamespace;
    void* userData;
};

// Execution context describing locals, arguments, and sandbox limits
struct VmExecutionContext {
    std::vector<VmValue> arguments;
    std::vector<VmValue> locals;
    uint64_t timeBudgetTicks;
    size_t memoryBudgetBytes;
    std::wstring sandboxNamespace;
    void* userData;

    VmExecutionContext()
        : timeBudgetTicks(0)
        , memoryBudgetBytes(0)
        , userData(nullptr) {}
};

struct VmExecutionResultNative {
    BOOL success;
    uint32_t stepsExecuted;
    void* returnValue;
    const wchar_t* failureReason;
};

// Host syscall table exposed to the VM
struct VmHostCall {
    enum class Kind : uint8_t {
        None,
        Timer,
        Http,
        Storage,
        Logging,
        Custom
    } kind;

    uint32_t identifier;
    void* callback;

    VmHostCall()
        : kind(Kind::None)
        , identifier(0)
        , callback(nullptr) {}
};

struct VmHostCallbacks {
    void (*logCallback)(const wchar_t* message, void* context);
    bool (*timerCallback)(uint32_t milliseconds, void* context);
    bool (*httpCallback)(const wchar_t* verb, const wchar_t* url, const wchar_t* payload, void* context);
    bool (*storageCallback)(const wchar_t* path, uint32_t operation, const void* inputBuffer, uint32_t inputSize, void* context);
    bool (*managedCallCallback)(uint32_t metadataToken, void* managedTarget, VmValue* arguments, uint32_t argumentCount, VmValue& returnValue, void* context);
    bool (*managedCtorCallback)(uint32_t metadataToken, void* managedTarget, VmValue* arguments, uint32_t argumentCount, VmValue& returnValue, void* context);
    uint32_t (*managedCallArityCallback)(uint32_t metadataToken, void* context);
    bool (*fieldLoadCallback)(void* instance, uint32_t fieldToken, VmValue& value, void* context);
    bool (*fieldStoreCallback)(void* instance, uint32_t fieldToken, const VmValue& value, void* context);
    bool (*stringLiteralCallback)(uint32_t metadataToken, VmValue& value, void* context);
    bool (*typeCastCallback)(uint32_t metadataToken, VmValue& value, void* context);
    void* userContext;

    VmHostCallbacks()
        : logCallback(nullptr)
        , timerCallback(nullptr)
        , httpCallback(nullptr)
        , storageCallback(nullptr)
        , managedCallCallback(nullptr)
        , managedCtorCallback(nullptr)
        , managedCallArityCallback(nullptr)
        , fieldLoadCallback(nullptr)
        , fieldStoreCallback(nullptr)
        , stringLiteralCallback(nullptr)
        , typeCastCallback(nullptr)
        , userContext(nullptr) {}
};

// Information about call sites emitted into bytecode
struct VmCallSite {
    enum class TargetKind : uint8_t {
        None,
        ManagedMethod,
        Host
    } kind;

    union {
        void* managedTarget;   // MethodDesc* or function pointer
        VmHostCall hostTarget;
    } data;

    uint32_t metadataToken;
    uint32_t argumentCount;

    VmCallSite()
        : kind(TargetKind::None) {
        data.managedTarget = nullptr;
        metadataToken = 0;
        argumentCount = 0;
    }
};

// Compiled bytecode program
struct VmProgram {
    std::vector<VmInstruction> instructions;
    std::vector<VmCallSite> callSites;
    std::vector<std::pair<size_t, int32_t>> branchFixups;
    uint32_t localCount;
    uint32_t argumentCount;
    std::string cacheKey;

    VmProgram()
        : localCount(0)
        , argumentCount(0) {}
};

// Serialized instruction with operands
struct VmInstruction {
    VmOpcode opcode;
    int32_t operand0;
    int32_t operand1;
    int32_t operand2;

    VmInstruction()
        : opcode(VmOpcode::Nop)
        , operand0(0)
        , operand1(0)
        , operand2(0) {}

    VmInstruction(VmOpcode op, int32_t op0 = 0, int32_t op1 = 0, int32_t op2 = 0)
        : opcode(op)
        , operand0(op0)
        , operand1(op1)
        , operand2(op2) {}
};

// Virtual machine entry point
class ILVirtualMachine {
public:
    ILVirtualMachine();
    ~ILVirtualMachine();

    bool Initialize();
    void Shutdown();

    // Compile IL into VM bytecode, optionally retrieving a cached version
    std::shared_ptr<VmProgram> Compile(const void* ilCode, size_t ilSize, const std::string& cacheKey);

    // Execute a previously compiled program with the provided context
    bool Execute(const VmProgram& program, VmExecutionContext& context, VmExecutionResult& result);
    bool ExecuteHandle(void* handle, VmExecutionContext& context, VmExecutionResult& result);

    // Configure host callbacks for syscalls exposed to bytecode
    void SetHostCallbacks(const VmHostCallbacks& callbacks);

    // Cache helpers
    void FlushCache();
    bool ConfigureCallSite(void* handle, uint32_t callSiteIndex, void* managedTarget, uint32_t argumentCount, uint32_t metadataToken);

private:
    std::unique_ptr<BytecodeCompiler> m_compiler;
    std::unique_ptr<BytecodeCache> m_cache;
    VmHostCallbacks m_hostCallbacks;
    CRITICAL_SECTION m_lock;
    bool m_initialized;
    std::unordered_map<void*, std::shared_ptr<VmProgram>> m_livePrograms;

    bool ExecuteInstruction(const VmInstruction& instruction,
                            const VmProgram& program,
                            std::vector<VmValue>& stack,
                            std::vector<VmValue>& locals,
                            VmExecutionContext& context,
                            VmExecutionResult& result,
                            uint32_t& instructionPointer);

    void ReleaseHandle(void* handle);
};

// Helper exported functions for managed callers
extern "C" {
    __declspec(dllexport) HRESULT CLRNet_VM_CompileIL(const void* ilCode, DWORD ilSize, const char* cacheKey, void** outHandle);
    __declspec(dllexport) HRESULT CLRNet_VM_Execute(void* handle, VmExecutionContextNative* context, VmExecutionResultNative* result);
    __declspec(dllexport) HRESULT CLRNet_VM_Release(void* handle);
    __declspec(dllexport) HRESULT CLRNet_VM_RegisterHost(const VmHostCallbacks* callbacks);
    __declspec(dllexport) HRESULT CLRNet_VM_ConfigureCallSite(void* handle, uint32_t callSiteIndex, void* managedTarget, uint32_t argumentCount, uint32_t metadataToken);
}

} // namespace VM
} // namespace Phase1
} // namespace CLRNet

#endif // CLRNET_VM_VIRTUAL_MACHINE_H
