#include "SimpleJIT.h"
#include <cassert>

namespace CLRNet {
namespace Phase1 {

// Global JIT compiler instance
SimpleJIT* g_jitCompiler = nullptr;

//=============================================================================
// ARM32CodeGen Implementation
//=============================================================================

DWORD ARM32CodeGen::EncodeMovImmediate(ARM32Register rd, WORD immediate) {
    // MOV rd, #immediate (simplified encoding)
    // ARM32: 1110 00 1 1101 0 0000 rd immediate12
    return 0xE3A00000 | (rd << 12) | (immediate & 0xFFF);
}

DWORD ARM32CodeGen::EncodeAdd(ARM32Register rd, ARM32Register rn, ARM32Register rm) {
    // ADD rd, rn, rm
    // ARM32: 1110 00 0 0100 0 rn rd 00000000 rm
    return 0xE0800000 | (rn << 16) | (rd << 12) | rm;
}

DWORD ARM32CodeGen::EncodeSub(ARM32Register rd, ARM32Register rn, ARM32Register rm) {
    // SUB rd, rn, rm
    // ARM32: 1110 00 0 0010 0 rn rd 00000000 rm
    return 0xE0400000 | (rn << 16) | (rd << 12) | rm;
}

DWORD ARM32CodeGen::EncodeMul(ARM32Register rd, ARM32Register rn, ARM32Register rm) {
    // MUL rd, rn, rm
    // ARM32: 1110 000000 0 0 rd 0000 rm 1001 rn
    return 0xE0000090 | (rd << 16) | (rm << 8) | rn;
}

DWORD ARM32CodeGen::EncodeLoad(ARM32Register rd, ARM32Register rn, int offset) {
    // LDR rd, [rn, #offset]
    // ARM32: 1110 01 0 1100 1 rn rd offset12
    DWORD instruction = 0xE5900000 | (rn << 16) | (rd << 12);
    
    if (offset >= 0) {
        instruction |= 0x00800000 | (offset & 0xFFF);  // Positive offset
    } else {
        instruction |= (-offset & 0xFFF);              // Negative offset
    }
    
    return instruction;
}

DWORD ARM32CodeGen::EncodeStore(ARM32Register rd, ARM32Register rn, int offset) {
    // STR rd, [rn, #offset]
    // ARM32: 1110 01 0 1100 0 rn rd offset12
    DWORD instruction = 0xE5800000 | (rn << 16) | (rd << 12);
    
    if (offset >= 0) {
        instruction |= 0x00800000 | (offset & 0xFFF);  // Positive offset
    } else {
        instruction |= (-offset & 0xFFF);              // Negative offset
    }
    
    return instruction;
}

DWORD ARM32CodeGen::EncodeBranch(int offset) {
    // B offset (branch)
    // ARM32: 1110 1010 offset24
    // Offset is in words, needs to be encoded properly
    int wordOffset = offset / 4 - 2;  // PC is 2 instructions ahead
    return 0xEA000000 | (wordOffset & 0xFFFFFF);
}

DWORD ARM32CodeGen::EncodeBranchLink(int offset) {
    // BL offset (branch with link)
    // ARM32: 1110 1011 offset24
    int wordOffset = offset / 4 - 2;
    return 0xEB000000 | (wordOffset & 0xFFFFFF);
}

DWORD ARM32CodeGen::EncodeReturn() {
    // BX LR (return)
    // ARM32: 1110 000100101111111111110001 LR
    return 0xE12FFF1E;
}

DWORD ARM32CodeGen::EncodePush(WORD registerMask) {
    // PUSH {registers}
    // ARM32: 1110 100100101101 registerMask
    return 0xE92D0000 | registerMask;
}

DWORD ARM32CodeGen::EncodePop(WORD registerMask) {
    // POP {registers}  
    // ARM32: 1110 100010111101 registerMask
    return 0xE8BD0000 | registerMask;
}

std::vector<DWORD> ARM32CodeGen::GeneratePrologue(int localSize) {
    std::vector<DWORD> prologue;
    
    // PUSH {R4-R11, LR} - Save callee-saved registers and return address
    prologue.push_back(EncodePush(0x4FF0));
    
    // SUB SP, SP, #localSize - Allocate local variable space
    if (localSize > 0) {
        if (localSize <= 4095) {
            // SUB SP, SP, #localSize
            prologue.push_back(0xE24DD000 | (localSize & 0xFFF));
        } else {
            // For larger local sizes, use MOV + SUB
            prologue.push_back(EncodeMovImmediate(R12, localSize & 0xFFFF));
            prologue.push_back(EncodeSub(SP, SP, R12));
        }
    }
    
    return prologue;
}

std::vector<DWORD> ARM32CodeGen::GenerateEpilogue(int localSize) {
    std::vector<DWORD> epilogue;
    
    // ADD SP, SP, #localSize - Deallocate local variable space
    if (localSize > 0) {
        if (localSize <= 4095) {
            epilogue.push_back(0xE28DD000 | (localSize & 0xFFF));
        } else {
            epilogue.push_back(EncodeMovImmediate(R12, localSize & 0xFFFF));
            epilogue.push_back(EncodeAdd(SP, SP, R12));
        }
    }
    
    // POP {R4-R11, PC} - Restore registers and return
    epilogue.push_back(EncodePop(0x8FF0));
    
    return epilogue;
}

//=============================================================================
// EvaluationStack Implementation
//=============================================================================

EvaluationStack::EvaluationStack() : m_nextStackSlot(0) {
    m_registerUsed.resize(13, false);  // R0-R12
}

EvaluationStack::~EvaluationStack() {
    Clear();
}

void EvaluationStack::Push(ARM32Register reg) {
    m_stack.push_back(reg);
    if (reg <= R12) {
        m_registerUsed[reg] = true;
    }
}

ARM32Register EvaluationStack::Pop() {
    if (m_stack.empty()) {
        return R0; // Error condition
    }
    
    ARM32Register reg = m_stack.back();
    m_stack.pop_back();
    
    if (reg <= R12) {
        m_registerUsed[reg] = false;
    }
    
    return reg;
}

ARM32Register EvaluationStack::Peek(int depth) {
    if (depth >= static_cast<int>(m_stack.size())) {
        return R0; // Error condition
    }
    
    return m_stack[m_stack.size() - 1 - depth];
}

bool EvaluationStack::IsEmpty() const {
    return m_stack.empty();
}

int EvaluationStack::GetDepth() const {
    return static_cast<int>(m_stack.size());
}

void EvaluationStack::Clear() {
    m_stack.clear();
    std::fill(m_registerUsed.begin(), m_registerUsed.end(), false);
    m_nextStackSlot = 0;
}

ARM32Register EvaluationStack::GetNextAvailableRegister() {
    // Find first unused register R0-R12
    for (int i = 0; i < 13; i++) {
        if (!m_registerUsed[i]) {
            m_registerUsed[i] = true;
            return static_cast<ARM32Register>(i);
        }
    }
    
    // All registers used - would need stack spilling in real implementation
    return R0;
}

void EvaluationStack::ReleaseRegister(ARM32Register reg) {
    if (reg <= R12) {
        m_registerUsed[reg] = false;
    }
}

//=============================================================================
// SimpleJIT Implementation
//=============================================================================

SimpleJIT::SimpleJIT(TypeSystem* typeSystem)
    : m_typeSystem(typeSystem)
    , m_initialized(false)
    , m_codeCache(nullptr)
    , m_codeCacheSize(0)
    , m_codeCacheUsed(0)
    , m_compilationCount(0)
    , m_totalCompileTime(0) {
    
    InitializeCriticalSection(&m_jitLock);
}

SimpleJIT::~SimpleJIT() {
    Shutdown();
    DeleteCriticalSection(&m_jitLock);
}

bool SimpleJIT::Initialize() {
    if (m_initialized) return true;
    
    // Allocate code cache (1MB for Phase 1)
    m_codeCacheSize = 1024 * 1024;
    m_codeCache = VirtualAlloc(nullptr, m_codeCacheSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    
    if (!m_codeCache) {
        return false;
    }
    
    m_codeCacheUsed = 0;
    m_initialized = true;
    
    return true;
}

void SimpleJIT::Shutdown() {
    if (!m_initialized) return;
    
    EnterCriticalSection(&m_jitLock);
    
    m_compiledMethods.clear();
    
    if (m_codeCache) {
        VirtualFree(m_codeCache, 0, MEM_RELEASE);
        m_codeCache = nullptr;
    }
    
    m_codeCacheSize = 0;
    m_codeCacheUsed = 0;
    m_initialized = false;
    
    LeaveCriticalSection(&m_jitLock);
}

void* SimpleJIT::CompileMethod(MethodDesc* method, const void* ilCode, size_t ilSize) {
    if (!m_initialized || !method || !ilCode || ilSize == 0) {
        return nullptr;
    }
    
    EnterCriticalSection(&m_jitLock);
    
    DWORD startTime = GetTickCount();
    
    try {
        // Check if already compiled
        auto it = m_compiledMethods.find(method);
        if (it != m_compiledMethods.end()) {
            LeaveCriticalSection(&m_jitLock);
            return it->second;
        }
        
        // Validate IL
        if (!ValidateIL(ilCode, ilSize)) {
            LeaveCriticalSection(&m_jitLock);
            return nullptr;
        }
        
        // Set up compilation context
        JITContext context;
        context.ilCode = static_cast<const BYTE*>(ilCode);
        context.ilSize = ilSize;
        context.method = method;
        context.nativeSize = ilSize * 8;  // Rough estimate: 8 native bytes per IL byte
        context.nativeCode = static_cast<BYTE*>(AllocateCodeMemory(context.nativeSize));
        
        if (!context.nativeCode) {
            LeaveCriticalSection(&m_jitLock);
            return nullptr;
        }
        
        // Compile IL to native code
        if (!CompileILToNative(context)) {
            FreeCodeMemory(context.nativeCode, context.nativeSize);
            LeaveCriticalSection(&m_jitLock);
            return nullptr;
        }
        
        // Register compiled method
        void* compiledCode = context.nativeCode;
        m_compiledMethods[method] = compiledCode;
        method->nativeCode = compiledCode;
        method->flags |= MF_COMPILED;
        
        // Update statistics
        m_compilationCount++;
        m_totalCompileTime += GetTickCount() - startTime;
        
        LeaveCriticalSection(&m_jitLock);
        return compiledCode;
    }
    catch (...) {
        LeaveCriticalSection(&m_jitLock);
        return nullptr;
    }
}

void* SimpleJIT::GetCompiledMethod(MethodDesc* method) {
    if (!method) return nullptr;
    
    EnterCriticalSection(&m_jitLock);
    
    auto it = m_compiledMethods.find(method);
    void* result = (it != m_compiledMethods.end()) ? it->second : nullptr;
    
    LeaveCriticalSection(&m_jitLock);
    return result;
}

bool SimpleJIT::CompileILToNative(JITContext& context) {
    EvaluationStack stack;
    
    // Emit method prologue
    if (!EmitMethodPrologue(context)) {
        return false;
    }
    
    // Compile each IL instruction
    size_t ilOffset = 0;
    while (ilOffset < context.ilSize) {
        if (!CompileILInstruction(context, ilOffset, stack)) {
            return false;
        }
    }
    
    // Emit method epilogue
    if (!EmitMethodEpilogue(context)) {
        return false;
    }
    
    return true;
}

bool SimpleJIT::EmitMethodPrologue(JITContext& context) {
    // Generate standard function prologue
    std::vector<DWORD> prologue = ARM32CodeGen::GeneratePrologue(32); // 32 bytes local space
    
    EmitInstructions(context, prologue);
    return true;
}

bool SimpleJIT::EmitMethodEpilogue(JITContext& context) {
    // Generate standard function epilogue  
    std::vector<DWORD> epilogue = ARM32CodeGen::GenerateEpilogue(32);
    
    EmitInstructions(context, epilogue);
    return true;
}

bool SimpleJIT::CompileILInstruction(JITContext& context, size_t& ilOffset, EvaluationStack& stack) {
    if (ilOffset >= context.ilSize) return false;
    
    ILOpcode opcode = static_cast<ILOpcode>(context.ilCode[ilOffset]);
    ilOffset++;
    
    switch (opcode) {
        case IL_NOP:
            // No operation - emit NOP instruction
            EmitInstruction(context, 0xE1A00000);  // MOV R0, R0 (NOP)
            break;
            
        case IL_LDC_I4_0:
        case IL_LDC_I4_1:
        case IL_LDC_I4_2:
        case IL_LDC_I4_3:
        case IL_LDC_I4_4:
        case IL_LDC_I4_5:
        case IL_LDC_I4_6:
        case IL_LDC_I4_7:
        case IL_LDC_I4_8:
            // Load integer constant 0-8
            return EmitLoadConstant(context, opcode - IL_LDC_I4_0, stack);
            
        case IL_LDARG_0:
        case IL_LDARG_1:
        case IL_LDARG_2:
        case IL_LDARG_3:
            // Load argument 0-3
            return EmitLoadArgument(context, opcode - IL_LDARG_0, stack);
            
        case IL_STLOC_0:
        case IL_STLOC_1:
        case IL_STLOC_2:
        case IL_STLOC_3:
            // Store to local variable 0-3
            return EmitStoreLocal(context, opcode - IL_STLOC_0, stack);
            
        case IL_ADD:
        case IL_SUB:
        case IL_MUL:
        case IL_DIV:
            // Arithmetic operations
            return EmitArithmetic(context, opcode, stack);
            
        case IL_CALL:
            // Method call - read 4-byte token
            if (ilOffset + 4 > context.ilSize) return false;
            {
                DWORD token = *reinterpret_cast<const DWORD*>(&context.ilCode[ilOffset]);
                ilOffset += 4;
                return EmitCall(context, token, stack);
            }
            
        case IL_RET:
            // Return from method
            return EmitReturn(context, stack);
            
        default:
            // Unsupported opcode
            return false;
    }
    
    return true;
}

bool SimpleJIT::EmitLoadConstant(JITContext& context, int value, EvaluationStack& stack) {
    ARM32Register reg = stack.GetNextAvailableRegister();
    
    // MOV reg, #value
    EmitInstruction(context, ARM32CodeGen::EncodeMovImmediate(reg, value));
    
    stack.Push(reg);
    return true;
}

bool SimpleJIT::EmitLoadArgument(JITContext& context, int argIndex, EvaluationStack& stack) {
    ARM32Register reg = stack.GetNextAvailableRegister();
    
    // Arguments are passed in R0-R3, then on stack
    if (argIndex < 4) {
        // MOV reg, Rargindex
        EmitInstruction(context, 0xE1A00000 | (reg << 12) | argIndex);
    } else {
        // Load from stack - LDR reg, [SP, #offset]
        int offset = (argIndex - 4) * 4;
        EmitInstruction(context, ARM32CodeGen::EncodeLoad(reg, SP, offset));
    }
    
    stack.Push(reg);
    return true;
}

bool SimpleJIT::EmitStoreLocal(JITContext& context, int localIndex, EvaluationStack& stack) {
    if (stack.IsEmpty()) return false;
    
    ARM32Register valueReg = stack.Pop();
    
    // Store to local variable - STR valueReg, [SP, #offset]
    int offset = -(localIndex + 1) * 4;  // Negative offset from SP
    EmitInstruction(context, ARM32CodeGen::EncodeStore(valueReg, SP, offset));
    
    stack.ReleaseRegister(valueReg);
    return true;
}

bool SimpleJIT::EmitArithmetic(JITContext& context, ILOpcode opcode, EvaluationStack& stack) {
    if (stack.GetDepth() < 2) return false;
    
    ARM32Register rhs = stack.Pop();
    ARM32Register lhs = stack.Pop();
    ARM32Register result = stack.GetNextAvailableRegister();
    
    switch (opcode) {
        case IL_ADD:
            EmitInstruction(context, ARM32CodeGen::EncodeAdd(result, lhs, rhs));
            break;
        case IL_SUB:
            EmitInstruction(context, ARM32CodeGen::EncodeSub(result, lhs, rhs));
            break;
        case IL_MUL:
            EmitInstruction(context, ARM32CodeGen::EncodeMul(result, lhs, rhs));
            break;
        case IL_DIV:
            // Division is complex on ARM - simplified to use runtime call
            // In real implementation would call division helper
            EmitInstruction(context, ARM32CodeGen::EncodeMovImmediate(result, 1)); // Placeholder
            break;
        default:
            return false;
    }
    
    stack.ReleaseRegister(lhs);
    stack.ReleaseRegister(rhs);
    stack.Push(result);
    
    return true;
}

bool SimpleJIT::EmitCall(JITContext& context, DWORD token, EvaluationStack& stack) {
    // Resolve method from token
    void* methodPtr = ResolveMethodCall(token);
    if (!methodPtr) return false;
    
    // Simple direct call - BL methodPtr (would need proper address calculation)
    // For now, emit a placeholder
    EmitInstruction(context, 0xEBFFFFFE);  // BL -8 (infinite loop placeholder)
    
    return true;
}

bool SimpleJIT::EmitReturn(JITContext& context, EvaluationStack& stack) {
    // If there's a return value on stack, move it to R0
    if (!stack.IsEmpty()) {
        ARM32Register returnReg = stack.Pop();
        if (returnReg != R0) {
            EmitInstruction(context, 0xE1A00000 | returnReg);  // MOV R0, returnReg
        }
        stack.ReleaseRegister(returnReg);
    }
    
    // Return handled by epilogue
    return true;
}

void SimpleJIT::EmitInstruction(JITContext& context, DWORD instruction) {
    if (context.nativeUsed + 4 <= context.nativeSize) {
        *reinterpret_cast<DWORD*>(context.nativeCode + context.nativeUsed) = instruction;
        context.nativeUsed += 4;
    }
}

void SimpleJIT::EmitInstructions(JITContext& context, const std::vector<DWORD>& instructions) {
    for (DWORD instruction : instructions) {
        EmitInstruction(context, instruction);
    }
}

void* SimpleJIT::AllocateCodeMemory(size_t size) {
    if (m_codeCacheUsed + size > m_codeCacheSize) {
        return nullptr;  // Code cache full
    }
    
    void* result = static_cast<char*>(m_codeCache) + m_codeCacheUsed;
    m_codeCacheUsed += size;
    
    return result;
}

void SimpleJIT::FreeCodeMemory(void* ptr, size_t size) {
    // Simple allocator doesn't support individual free operations
    // In real implementation would use proper memory management
}

void* SimpleJIT::ResolveMethodCall(DWORD token) {
    // Simplified method resolution - in real implementation would use metadata
    // For now, return placeholder
    return nullptr;
}

bool SimpleJIT::ValidateIL(const void* ilCode, size_t ilSize) {
    if (!ilCode || ilSize == 0) return false;
    
    // Basic validation - check for valid opcodes
    const BYTE* code = static_cast<const BYTE*>(ilCode);
    
    for (size_t i = 0; i < ilSize; ) {
        BYTE opcode = code[i];
        i++;
        
        // Check for valid opcode ranges
        if (opcode > 0xFE) return false;
        
        // Skip operands for opcodes that have them
        switch (opcode) {
            case IL_CALL:
                i += 4;  // 4-byte token
                break;
            case IL_LDSTR:
                i += 4;  // 4-byte string token
                break;
            // Add other opcodes with operands as needed
        }
        
        if (i > ilSize) return false;
    }
    
    return true;
}

void SimpleJIT::FlushCodeCache() {
    EnterCriticalSection(&m_jitLock);
    
    m_compiledMethods.clear();
    m_codeCacheUsed = 0;
    
    LeaveCriticalSection(&m_jitLock);
}

size_t SimpleJIT::GetCodeCacheSize() const {
    return m_codeCacheSize;
}

size_t SimpleJIT::GetCodeCacheUsed() const {
    return m_codeCacheUsed;
}

//=============================================================================
// JIT Helper Functions (Runtime Support)
//=============================================================================

extern "C" {

__declspec(dllexport) void* JIT_AllocateString(int length) {
    // Simplified string allocation
    if (g_garbageCollector && g_typeSystem) {
        MethodTable* stringMT = g_typeSystem->GetStringMethodTable();
        size_t size = sizeof(ObjectHeader) + (length + 1) * sizeof(wchar_t);
        return g_garbageCollector->AllocateObject(size);
    }
    return nullptr;
}

__declspec(dllexport) void JIT_InitializeString(void* str, const wchar_t* data) {
    // Initialize string object with data
    if (str && data) {
        auto* stringData = reinterpret_cast<wchar_t*>(
            static_cast<char*>(str) + sizeof(ObjectHeader)
        );
        wcscpy_s(stringData, wcslen(data) + 1, data);
    }
}

__declspec(dllexport) void JIT_CallStaticMethod(void* methodPtr) {
    // Call static method - simplified
    if (methodPtr) {
        typedef void (*StaticMethodFunc)();
        StaticMethodFunc func = reinterpret_cast<StaticMethodFunc>(methodPtr);
        func();
    }
}

__declspec(dllexport) void* JIT_CallInstanceMethod(void* obj, void* methodPtr) {
    // Call instance method - simplified
    if (obj && methodPtr) {
        typedef void* (*InstanceMethodFunc)(void*);
        InstanceMethodFunc func = reinterpret_cast<InstanceMethodFunc>(methodPtr);
        return func(obj);
    }
    return nullptr;
}

__declspec(dllexport) void JIT_ThrowException(void* exceptionObj) {
    // Throw managed exception - simplified
    if (exceptionObj) {
        // In real implementation would unwind stack and find exception handlers
        throw std::runtime_error("Managed exception thrown");
    }
}

__declspec(dllexport) void JIT_HandleException(void* exceptionObj) {
    // Handle exception - simplified
}

__declspec(dllexport) void* JIT_AllocateObject(MethodTable* methodTable) {
    return g_garbageCollector ? g_garbageCollector->AllocateObject(methodTable->instanceSize) : nullptr;
}

__declspec(dllexport) void JIT_CollectGarbage() {
    if (g_garbageCollector) {
        g_garbageCollector->ForceCollection();
    }
}

} // extern "C"

//=============================================================================
// JITDiagnostics Implementation
//=============================================================================

void JITDiagnostics::DumpMethod(MethodDesc* method, const void* nativeCode, size_t size) {
    if (!method || !nativeCode) return;
    
    char buffer[256];
    sprintf_s(buffer, sizeof(buffer), "[JIT] Method compiled: %p, Size: %zu bytes\n", 
              nativeCode, size);
    OutputDebugStringA(buffer);
}

void JITDiagnostics::DumpIL(const void* ilCode, size_t size) {
    if (!ilCode) return;
    
    const BYTE* code = static_cast<const BYTE*>(ilCode);
    char buffer[1024];
    char* ptr = buffer;
    
    ptr += sprintf_s(ptr, sizeof(buffer) - (ptr - buffer), "[JIT] IL Code (%zu bytes): ", size);
    
    for (size_t i = 0; i < size && ptr < buffer + sizeof(buffer) - 4; i++) {
        ptr += sprintf_s(ptr, sizeof(buffer) - (ptr - buffer), "%02X ", code[i]);
    }
    
    strcat_s(buffer, sizeof(buffer), "\n");
    OutputDebugStringA(buffer);
}

void JITDiagnostics::DumpNativeCode(const void* nativeCode, size_t size) {
    if (!nativeCode) return;
    
    const DWORD* code = static_cast<const DWORD*>(nativeCode);
    size_t instructionCount = size / 4;
    
    char buffer[256];
    sprintf_s(buffer, sizeof(buffer), "[JIT] Native Code (%zu instructions):\n", instructionCount);
    OutputDebugStringA(buffer);
    
    for (size_t i = 0; i < instructionCount; i++) {
        sprintf_s(buffer, sizeof(buffer), "[JIT]   %04zX: %08X\n", i * 4, code[i]);
        OutputDebugStringA(buffer);
    }
}

bool JITDiagnostics::ValidateNativeCode(const void* nativeCode, size_t size) {
    if (!nativeCode || size == 0) return false;
    
    // Basic validation - ensure size is multiple of 4 (ARM instruction size)
    if (size % 4 != 0) return false;
    
    // Could add more sophisticated validation here
    return true;
}

} // namespace Phase1
} // namespace CLRNet