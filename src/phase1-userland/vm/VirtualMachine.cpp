#include "VirtualMachine.h"
#include "BytecodeCache.h"
#include "BytecodeCompiler.h"

#include <algorithm>
#include <sstream>
#include <vector>

namespace CLRNet {
namespace Phase1 {
namespace VM {

namespace {

uint64_t GetCurrentTicks() {
    return static_cast<uint64_t>(GetTickCount64());
}

void LogMessage(const VmHostCallbacks& callbacks, const std::wstring& message) {
    if (callbacks.logCallback) {
        callbacks.logCallback(message.c_str(), callbacks.userContext);
    }
}

bool EnsureStackMemory(const VmExecutionContext& context, const std::vector<VmValue>& stack) {
    if (context.memoryBudgetBytes == 0) {
        return true;
    }

    size_t estimated = stack.size() * sizeof(VmValue);
    return estimated <= context.memoryBudgetBytes;
}

thread_local std::wstring g_lastVmFailure;

VmExecutionContext ConvertContext(const VmExecutionContextNative& native) {
    VmExecutionContext context;
    if (native.arguments && native.argumentCount) {
        context.arguments.assign(native.arguments, native.arguments + native.argumentCount);
    }
    if (native.locals && native.localCount) {
        context.locals.assign(native.locals, native.locals + native.localCount);
    }
    context.timeBudgetTicks = native.timeBudgetTicks;
    context.memoryBudgetBytes = native.memoryBudgetBytes;
    if (native.sandboxNamespace) {
        context.sandboxNamespace = native.sandboxNamespace;
    }
    context.userData = native.userData;
    return context;
}

void CopyContextBack(const VmExecutionContext& context, VmExecutionContextNative& native) {
    if (native.arguments && native.argumentCount) {
        size_t count = std::min(static_cast<size_t>(native.argumentCount), context.arguments.size());
        std::copy_n(context.arguments.begin(), count, native.arguments);
    }
    if (native.locals && native.localCount) {
        size_t count = std::min(static_cast<size_t>(native.localCount), context.locals.size());
        std::copy_n(context.locals.begin(), count, native.locals);
    }
}

void ConvertResult(const VmExecutionResult& source, VmExecutionResultNative& dest) {
    dest.success = source.success ? TRUE : FALSE;
    dest.stepsExecuted = source.stepsExecuted;
    dest.returnValue = source.returnValue;
    g_lastVmFailure = source.failureReason;
    dest.failureReason = g_lastVmFailure.empty() ? nullptr : g_lastVmFailure.c_str();
}

} // namespace

ILVirtualMachine::ILVirtualMachine()
    : m_initialized(false) {
    InitializeCriticalSection(&m_lock);
}

ILVirtualMachine::~ILVirtualMachine() {
    Shutdown();
    DeleteCriticalSection(&m_lock);
}

bool ILVirtualMachine::Initialize() {
    EnterCriticalSection(&m_lock);

    if (m_initialized) {
        LeaveCriticalSection(&m_lock);
        return true;
    }

    m_compiler = std::make_unique<BytecodeCompiler>();
    m_cache = std::make_unique<BytecodeCache>();

    if (!m_compiler->Initialize() || !m_cache->Initialize()) {
        m_compiler.reset();
        m_cache.reset();
        LeaveCriticalSection(&m_lock);
        return false;
    }

    m_initialized = true;
    LeaveCriticalSection(&m_lock);
    return true;
}

void ILVirtualMachine::Shutdown() {
    EnterCriticalSection(&m_lock);
    if (!m_initialized) {
        LeaveCriticalSection(&m_lock);
        return;
    }

    if (m_compiler) {
        m_compiler->Shutdown();
    }
    if (m_cache) {
        m_cache->Flush();
        m_cache->Shutdown();
    }

    m_cache.reset();
    m_compiler.reset();
    m_livePrograms.clear();
    m_initialized = false;
    LeaveCriticalSection(&m_lock);
}

std::shared_ptr<VmProgram> ILVirtualMachine::Compile(const void* ilCode, size_t ilSize, const std::string& cacheKey) {
    if (!m_initialized || !ilCode || ilSize == 0) {
        return nullptr;
    }

    std::string effectiveKey = cacheKey;
    if (effectiveKey.empty()) {
        effectiveKey = ComputeSha1(ilCode, ilSize);
    }

    std::shared_ptr<VmProgram> program;

    if (!effectiveKey.empty()) {
        program = m_cache->Get(effectiveKey);
        if (program) {
            EnterCriticalSection(&m_lock);
            m_livePrograms[program.get()] = program;
            LeaveCriticalSection(&m_lock);
            return program;
        }
    }

    program = m_compiler->Compile(ilCode, ilSize, effectiveKey);
    if (!program) {
        return nullptr;
    }

    if (!effectiveKey.empty()) {
        m_cache->Put(effectiveKey, *program);
    }

    EnterCriticalSection(&m_lock);
    m_livePrograms[program.get()] = program;
    LeaveCriticalSection(&m_lock);

    return program;
}

bool ILVirtualMachine::Execute(const VmProgram& program, VmExecutionContext& context, VmExecutionResult& result) {
    if (!m_initialized) {
        result.success = false;
        result.failureReason = L"VM not initialized";
        return false;
    }

    std::vector<VmValue> locals = context.locals;
    if (locals.size() < program.localCount) {
        locals.resize(program.localCount);
    }

    if (context.arguments.size() < program.argumentCount) {
        context.arguments.resize(program.argumentCount);
    }

    std::vector<VmValue> stack;
    stack.reserve(program.instructions.size());

    uint64_t startTicks = GetCurrentTicks();
    result.stepsExecuted = 0;
    result.returnValue = nullptr;

    uint32_t instructionPointer = 0;
    while (instructionPointer < program.instructions.size()) {
        if (context.timeBudgetTicks > 0) {
            uint64_t elapsed = GetCurrentTicks() - startTicks;
            if (elapsed > context.timeBudgetTicks) {
                result.success = false;
                result.failureReason = L"VM execution exceeded time budget";
                LogMessage(m_hostCallbacks, result.failureReason);
                return false;
            }
        }

        if (!EnsureStackMemory(context, stack)) {
            result.success = false;
            result.failureReason = L"VM execution exceeded memory budget";
            LogMessage(m_hostCallbacks, result.failureReason);
            return false;
        }

        const VmInstruction& instruction = program.instructions[instructionPointer];
        uint32_t previousInstruction = instructionPointer;
        if (!ExecuteInstruction(instruction, program, stack, locals, context, result, instructionPointer)) {
            return false;
        }

        result.stepsExecuted++;

        if (instructionPointer == previousInstruction) {
            instructionPointer++;
        }
    }

    if (!stack.empty()) {
        result.returnValue = stack.back().data.object;
    }

    context.locals = locals;
    result.success = true;
    return true;
}

bool ILVirtualMachine::ExecuteHandle(void* handle, VmExecutionContext& context, VmExecutionResult& result) {
    if (!handle) {
        result.success = false;
        result.failureReason = L"Invalid VM handle";
        return false;
    }

    std::shared_ptr<VmProgram> program;
    EnterCriticalSection(&m_lock);
    auto it = m_livePrograms.find(handle);
    if (it != m_livePrograms.end()) {
        program = it->second;
    }
    LeaveCriticalSection(&m_lock);

    if (!program) {
        result.success = false;
        result.failureReason = L"VM program handle not registered";
        return false;
    }

    return Execute(*program, context, result);
}

void ILVirtualMachine::SetHostCallbacks(const VmHostCallbacks& callbacks) {
    m_hostCallbacks = callbacks;
}

void ILVirtualMachine::FlushCache() {
    if (m_cache) {
        m_cache->Flush();
    }
}

bool ILVirtualMachine::ConfigureCallSite(void* handle, uint32_t callSiteIndex, void* managedTarget, uint32_t argumentCount, uint32_t metadataToken) {
    if (!handle) {
        return false;
    }

    std::shared_ptr<VmProgram> program;
    EnterCriticalSection(&m_lock);
    auto it = m_livePrograms.find(handle);
    if (it != m_livePrograms.end()) {
        program = it->second;
    }
    LeaveCriticalSection(&m_lock);

    if (!program) {
        return false;
    }

    if (callSiteIndex >= program->callSites.size()) {
        return false;
    }

    VmCallSite& callSite = program->callSites[callSiteIndex];
    callSite.data.managedTarget = managedTarget;
    callSite.argumentCount = argumentCount;
    if (metadataToken != 0) {
        callSite.metadataToken = metadataToken;
    }
    return true;
}

void ILVirtualMachine::ReleaseHandle(void* handle) {
    if (!handle) {
        return;
    }

    EnterCriticalSection(&m_lock);
    m_livePrograms.erase(handle);
    LeaveCriticalSection(&m_lock);
}

bool ILVirtualMachine::ExecuteInstruction(const VmInstruction& instruction,
                                          const VmProgram& program,
                                          std::vector<VmValue>& stack,
                                          std::vector<VmValue>& locals,
                                          VmExecutionContext& context,
                                          VmExecutionResult& result,
                                          uint32_t& instructionPointer) {
    auto requireStack = [&](size_t count) -> bool {
        if (stack.size() < count) {
            result.success = false;
            result.failureReason = L"VM stack underflow";
            LogMessage(m_hostCallbacks, result.failureReason);
            return false;
        }
        return true;
    };

    auto ensureLocalIndex = [&](size_t index) {
        if (index >= locals.size()) {
            locals.resize(index + 1);
        }
    };

    auto ensureArgumentIndex = [&](size_t index) {
        if (index >= context.arguments.size()) {
            context.arguments.resize(index + 1);
        }
    };

    switch (instruction.opcode) {
    case VmOpcode::Nop:
        return true;
    case VmOpcode::LoadArgument: {
        ensureArgumentIndex(instruction.operand0);
        stack.push_back(context.arguments[instruction.operand0]);
        return true;
    }
    case VmOpcode::StoreArgument: {
        if (!requireStack(1)) {
            return false;
        }
        ensureArgumentIndex(instruction.operand0);
        context.arguments[instruction.operand0] = stack.back();
        stack.pop_back();
        return true;
    }
    case VmOpcode::LoadLocal: {
        ensureLocalIndex(instruction.operand0);
        stack.push_back(locals[instruction.operand0]);
        return true;
    }
    case VmOpcode::StoreLocal: {
        if (!requireStack(1)) {
            return false;
        }
        ensureLocalIndex(instruction.operand0);
        locals[instruction.operand0] = stack.back();
        stack.pop_back();
        return true;
    }
    case VmOpcode::LoadConstantI4:
        stack.emplace_back(instruction.operand0);
        return true;
    case VmOpcode::LoadConstantI8: {
        uint64_t lower = static_cast<uint32_t>(instruction.operand0);
        uint64_t upper = static_cast<uint32_t>(instruction.operand1);
        int64_t value = static_cast<int64_t>((upper << 32) | lower);
        stack.emplace_back(value);
        return true;
    }
    case VmOpcode::LoadNull:
        stack.emplace_back(nullptr, VmValue::Kind::Null);
        return true;
    case VmOpcode::LoadString: {
        if (!m_hostCallbacks.stringLiteralCallback) {
            result.success = false;
            result.failureReason = L"No string literal callback registered";
            LogMessage(m_hostCallbacks, result.failureReason);
            return false;
        }
        VmValue value;
        if (!m_hostCallbacks.stringLiteralCallback(static_cast<uint32_t>(instruction.operand0), value,
                                                   m_hostCallbacks.userContext)) {
            result.success = false;
            result.failureReason = L"String literal callback failed";
            LogMessage(m_hostCallbacks, result.failureReason);
            return false;
        }
        stack.push_back(value);
        return true;
    }
    case VmOpcode::Add:
    case VmOpcode::Subtract:
    case VmOpcode::Multiply:
    case VmOpcode::Divide: {
        if (!requireStack(2)) {
            return false;
        }
        VmValue right = stack.back();
        stack.pop_back();
        VmValue left = stack.back();
        stack.pop_back();

        if (left.kind != VmValue::Kind::Int32 || right.kind != VmValue::Kind::Int32) {
            result.success = false;
            result.failureReason = L"Arithmetic currently supports Int32 only";
            LogMessage(m_hostCallbacks, result.failureReason);
            return false;
        }

        int32_t computed = 0;
        switch (instruction.opcode) {
        case VmOpcode::Add: computed = left.data.i32 + right.data.i32; break;
        case VmOpcode::Subtract: computed = left.data.i32 - right.data.i32; break;
        case VmOpcode::Multiply: computed = left.data.i32 * right.data.i32; break;
        case VmOpcode::Divide:
            if (right.data.i32 == 0) {
                result.success = false;
                result.failureReason = L"Division by zero";
                LogMessage(m_hostCallbacks, result.failureReason);
                return false;
            }
            computed = left.data.i32 / right.data.i32;
            break;
        default:
            break;
        }

        stack.emplace_back(computed);
        return true;
    }
    case VmOpcode::Branch:
    case VmOpcode::BranchIfTrue:
    case VmOpcode::BranchIfFalse: {
        if (instruction.opcode != VmOpcode::Branch) {
            if (!requireStack(1)) {
                return false;
            }
            VmValue condition = stack.back();
            stack.pop_back();
            bool truthy = false;
            if (condition.kind == VmValue::Kind::Int32) {
                truthy = condition.data.i32 != 0;
            } else if (condition.kind == VmValue::Kind::Object || condition.kind == VmValue::Kind::ManagedPointer) {
                truthy = condition.data.object != nullptr;
            }

            if ((instruction.opcode == VmOpcode::BranchIfTrue && !truthy) ||
                (instruction.opcode == VmOpcode::BranchIfFalse && truthy)) {
                return true;
            }
        }

        if (instruction.operand0 < 0 || static_cast<size_t>(instruction.operand0) >= program.instructions.size()) {
            result.success = false;
            result.failureReason = L"Branch target out of range";
            LogMessage(m_hostCallbacks, result.failureReason);
            return false;
        }

        instructionPointer = static_cast<uint32_t>(instruction.operand0);
        return true;
    }
    case VmOpcode::Call:
    case VmOpcode::CallVirtual:
    case VmOpcode::HostCall:
    case VmOpcode::NewObject: {
        int callIndex = instruction.operand0;
        if (callIndex < 0 || static_cast<size_t>(callIndex) >= program.callSites.size()) {
            result.success = false;
            result.failureReason = L"Invalid call site index";
            LogMessage(m_hostCallbacks, result.failureReason);
            return false;
        }

        const VmCallSite& callSite = program.callSites[callIndex];
        uint32_t token = callSite.metadataToken;

        uint32_t argumentCount = callSite.argumentCount;
        if (argumentCount == 0 && m_hostCallbacks.managedCallArityCallback) {
            argumentCount = m_hostCallbacks.managedCallArityCallback(token, m_hostCallbacks.userContext);
        }

        std::vector<VmValue> arguments(argumentCount);
        for (uint32_t i = 0; i < argumentCount; ++i) {
            if (!requireStack(1)) {
                return false;
            }
            arguments[argumentCount - 1 - i] = stack.back();
            stack.pop_back();
        }

        VmValue returnValue;
        bool success = false;

        if (instruction.opcode == VmOpcode::NewObject) {
            if (!m_hostCallbacks.managedCtorCallback) {
                result.success = false;
                result.failureReason = L"No constructor callback registered";
                LogMessage(m_hostCallbacks, result.failureReason);
                return false;
            }
            success = m_hostCallbacks.managedCtorCallback(token, callSite.data.managedTarget, arguments.data(), argumentCount,
                                                          returnValue, m_hostCallbacks.userContext);
        } else if (instruction.opcode == VmOpcode::HostCall) {
            // Host calls use the managedCallCallback entry point to give host full control
            if (!m_hostCallbacks.managedCallCallback) {
                result.success = false;
                result.failureReason = L"No host call callback registered";
                LogMessage(m_hostCallbacks, result.failureReason);
                return false;
            }
            success = m_hostCallbacks.managedCallCallback(token, callSite.data.managedTarget, arguments.data(), argumentCount,
                                                          returnValue, m_hostCallbacks.userContext);
        } else {
            if (!m_hostCallbacks.managedCallCallback) {
                result.success = false;
                result.failureReason = L"No managed call callback registered";
                LogMessage(m_hostCallbacks, result.failureReason);
                return false;
            }
            success = m_hostCallbacks.managedCallCallback(token, callSite.data.managedTarget, arguments.data(), argumentCount,
                                                          returnValue, m_hostCallbacks.userContext);
        }

        if (!success) {
            result.success = false;
            result.failureReason = L"Managed call dispatch failed";
            LogMessage(m_hostCallbacks, result.failureReason);
            return false;
        }

        if (returnValue.kind != VmValue::Kind::Uninitialized) {
            stack.push_back(returnValue);
        }
        return true;
    }
    case VmOpcode::LoadField: {
        if (!requireStack(1)) {
            return false;
        }
        if (!m_hostCallbacks.fieldLoadCallback) {
            result.success = false;
            result.failureReason = L"No field load callback registered";
            LogMessage(m_hostCallbacks, result.failureReason);
            return false;
        }
        VmValue instance = stack.back();
        stack.pop_back();
        VmValue value;
        if (!m_hostCallbacks.fieldLoadCallback(instance.data.object, static_cast<uint32_t>(instruction.operand0),
                                               value, m_hostCallbacks.userContext)) {
            result.success = false;
            result.failureReason = L"Field load callback failed";
            LogMessage(m_hostCallbacks, result.failureReason);
            return false;
        }
        stack.push_back(value);
        return true;
    }
    case VmOpcode::StoreField: {
        if (!requireStack(2)) {
            return false;
        }
        if (!m_hostCallbacks.fieldStoreCallback) {
            result.success = false;
            result.failureReason = L"No field store callback registered";
            LogMessage(m_hostCallbacks, result.failureReason);
            return false;
        }
        VmValue value = stack.back();
        stack.pop_back();
        VmValue instance = stack.back();
        stack.pop_back();
        if (!m_hostCallbacks.fieldStoreCallback(instance.data.object, static_cast<uint32_t>(instruction.operand0),
                                                value, m_hostCallbacks.userContext)) {
            result.success = false;
            result.failureReason = L"Field store callback failed";
            LogMessage(m_hostCallbacks, result.failureReason);
            return false;
        }
        return true;
    }
    case VmOpcode::Box:
    case VmOpcode::UnboxAny:
    case VmOpcode::CastClass: {
        if (!requireStack(1)) {
            return false;
        }
        if (!m_hostCallbacks.typeCastCallback) {
            result.success = false;
            result.failureReason = L"No type cast callback registered";
            LogMessage(m_hostCallbacks, result.failureReason);
            return false;
        }
        VmValue value = stack.back();
        stack.pop_back();
        if (!m_hostCallbacks.typeCastCallback(static_cast<uint32_t>(instruction.operand0), value,
                                              m_hostCallbacks.userContext)) {
            result.success = false;
            result.failureReason = L"Type cast callback failed";
            LogMessage(m_hostCallbacks, result.failureReason);
            return false;
        }
        stack.push_back(value);
        return true;
    }
    case VmOpcode::Return: {
        if (!stack.empty()) {
            result.returnValue = stack.back().data.object;
        }
        instructionPointer = static_cast<uint32_t>(-1);
        return true;
    }
    default:
        result.success = false;
        result.failureReason = L"Unsupported VM opcode";
        LogMessage(m_hostCallbacks, result.failureReason);
        return false;
    }
}

// Exported C-style entry points ------------------------------------------------

extern "C" {

static ILVirtualMachine g_vmInstance;

__declspec(dllexport) HRESULT CLRNet_VM_CompileIL(const void* ilCode, DWORD ilSize, const char* cacheKey, void** outHandle) {
    if (!outHandle) {
        return E_POINTER;
    }

    if (!g_vmInstance.Initialize()) {
        return E_FAIL;
    }

    std::string key = cacheKey ? std::string(cacheKey) : std::string();
    auto program = g_vmInstance.Compile(ilCode, ilSize, key);
    if (!program) {
        return E_FAIL;
    }

    *outHandle = program.get();
    return S_OK;
}

__declspec(dllexport) HRESULT CLRNet_VM_Execute(void* handle, VmExecutionContextNative* context, VmExecutionResultNative* result) {
    if (!handle || !context || !result) {
        return E_POINTER;
    }

    VmExecutionContext state = ConvertContext(*context);
    VmExecutionResult executionResult;

    if (!g_vmInstance.ExecuteHandle(handle, state, executionResult)) {
        ConvertResult(executionResult, *result);
        return E_FAIL;
    }

    CopyContextBack(state, *context);
    ConvertResult(executionResult, *result);
    if (!executionResult.success) {
        return E_FAIL;
    }

    return S_OK;
}

__declspec(dllexport) HRESULT CLRNet_VM_Release(void* handle) {
    g_vmInstance.ReleaseHandle(handle);
    return S_OK;
}

__declspec(dllexport) HRESULT CLRNet_VM_RegisterHost(const VmHostCallbacks* callbacks) {
    if (!callbacks) {
        return E_POINTER;
    }

    if (!g_vmInstance.Initialize()) {
        return E_FAIL;
    }

    g_vmInstance.SetHostCallbacks(*callbacks);
    return S_OK;
}

__declspec(dllexport) HRESULT CLRNet_VM_ConfigureCallSite(void* handle, uint32_t callSiteIndex, void* managedTarget, uint32_t argumentCount, uint32_t metadataToken) {
    if (!g_vmInstance.ConfigureCallSite(handle, callSiteIndex, managedTarget, argumentCount, metadataToken)) {
        return E_FAIL;
    }
    return S_OK;
}

} // extern "C"

} // namespace VM
} // namespace Phase1
} // namespace CLRNet
