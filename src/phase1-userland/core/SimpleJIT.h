#pragma once

// Simple JIT compiler for CLRNET Phase 1 runtime
// Compiles IL bytecode to ARM32 native code

#ifndef SIMPLE_JIT_H
#define SIMPLE_JIT_H

#include <windows.h>
#include <memory>
#include <vector>
#include <unordered_map>
#include "TypeSystem.h"

namespace CLRNet {
namespace Phase1 {

// Forward declarations
struct MethodDesc;
class TypeSystem;

// ARM32 instruction encoding helpers
enum ARM32Register : BYTE {
    R0 = 0, R1 = 1, R2 = 2, R3 = 3,
    R4 = 4, R5 = 5, R6 = 6, R7 = 7,
    R8 = 8, R9 = 9, R10 = 10, R11 = 11,
    R12 = 12, R13 = 13, R14 = 14, R15 = 15,
    
    // Aliases
    SP = R13,   // Stack pointer
    LR = R14,   // Link register
    PC = R15    // Program counter
};

// IL opcodes (subset)
enum ILOpcode : BYTE {
    IL_NOP = 0x00,
    IL_LDARG_0 = 0x02,
    IL_LDARG_1 = 0x03,
    IL_LDARG_2 = 0x04,
    IL_LDARG_3 = 0x05,
    IL_LDLOC_0 = 0x06,
    IL_LDLOC_1 = 0x07,
    IL_LDLOC_2 = 0x08,
    IL_LDLOC_3 = 0x09,
    IL_STLOC_0 = 0x0A,
    IL_STLOC_1 = 0x0B,
    IL_STLOC_2 = 0x0C,
    IL_STLOC_3 = 0x0D,
    IL_LDSTR = 0x72,
    IL_CALL = 0x28,
    IL_RET = 0x2A,
    IL_LDC_I4_0 = 0x16,
    IL_LDC_I4_1 = 0x17,
    IL_LDC_I4_2 = 0x18,
    IL_LDC_I4_3 = 0x19,
    IL_LDC_I4_4 = 0x1A,
    IL_LDC_I4_5 = 0x1B,
    IL_LDC_I4_6 = 0x1C,
    IL_LDC_I4_7 = 0x1D,
    IL_LDC_I4_8 = 0x1E,
    IL_ADD = 0x58,
    IL_SUB = 0x59,
    IL_MUL = 0x5A,
    IL_DIV = 0x5B
};

// JIT compilation context
struct JITContext {
    const BYTE* ilCode;         // IL bytecode
    size_t ilSize;              // IL size in bytes
    MethodDesc* method;         // Method being compiled
    BYTE* nativeCode;           // Output native code buffer
    size_t nativeSize;          // Native code buffer size
    size_t nativeUsed;          // Bytes of native code generated
    
    // Compilation state
    std::vector<DWORD> labelOffsets;    // Branch target offsets
    std::unordered_map<size_t, size_t> ilToNativeMap;  // IL offset to native offset mapping
    
    JITContext() : ilCode(nullptr), ilSize(0), method(nullptr), 
                   nativeCode(nullptr), nativeSize(0), nativeUsed(0) {}
};

// Code generation helpers
class ARM32CodeGen {
public:
    // Basic ARM32 instruction encoding
    static DWORD EncodeMovImmediate(ARM32Register rd, WORD immediate);
    static DWORD EncodeAdd(ARM32Register rd, ARM32Register rn, ARM32Register rm);
    static DWORD EncodeSub(ARM32Register rd, ARM32Register rn, ARM32Register rm);
    static DWORD EncodeMul(ARM32Register rd, ARM32Register rn, ARM32Register rm);
    static DWORD EncodeLoad(ARM32Register rd, ARM32Register rn, int offset = 0);
    static DWORD EncodeStore(ARM32Register rd, ARM32Register rn, int offset = 0);
    static DWORD EncodeBranch(int offset);
    static DWORD EncodeBranchLink(int offset);
    static DWORD EncodeReturn();
    
    // Stack operations
    static DWORD EncodePush(WORD registerMask);
    static DWORD EncodePop(WORD registerMask);
    
    // Function prologue/epilogue
    static std::vector<DWORD> GeneratePrologue(int localSize);
    static std::vector<DWORD> GenerateEpilogue(int localSize);
};

// Evaluation stack simulation
class EvaluationStack {
public:
    EvaluationStack();
    ~EvaluationStack();
    
    void Push(ARM32Register reg);
    ARM32Register Pop();
    ARM32Register Peek(int depth = 0);
    bool IsEmpty() const;
    int GetDepth() const;
    void Clear();
    
    // Register allocation helpers
    ARM32Register GetNextAvailableRegister();
    void ReleaseRegister(ARM32Register reg);
    
private:
    std::vector<ARM32Register> m_stack;
    std::vector<bool> m_registerUsed;  // R0-R12 usage tracking
    int m_nextStackSlot;
};

// Main JIT compiler class
class SimpleJIT {
public:
    SimpleJIT(TypeSystem* typeSystem);
    ~SimpleJIT();
    
    // Initialization
    bool Initialize();
    void Shutdown();
    
    // Compilation interface
    void* CompileMethod(MethodDesc* method, const void* ilCode, size_t ilSize);
    void* GetCompiledMethod(MethodDesc* method);
    
    // Code cache management
    void FlushCodeCache();
    size_t GetCodeCacheSize() const;
    size_t GetCodeCacheUsed() const;
    
    // Statistics
    DWORD GetCompilationCount() const { return m_compilationCount; }
    DWORD GetTotalCompileTime() const { return m_totalCompileTime; }
    
private:
    TypeSystem* m_typeSystem;
    bool m_initialized;
    
    // Code cache
    void* m_codeCache;
    size_t m_codeCacheSize;
    size_t m_codeCacheUsed;
    
    // Compiled method tracking
    std::unordered_map<MethodDesc*, void*> m_compiledMethods;
    
    // Statistics
    DWORD m_compilationCount;
    DWORD m_totalCompileTime;
    
    CRITICAL_SECTION m_jitLock;
    
    // Compilation phases
    bool CompileILToNative(JITContext& context);
    bool EmitMethodPrologue(JITContext& context);
    bool EmitMethodEpilogue(JITContext& context);
    bool CompileILInstruction(JITContext& context, size_t& ilOffset, EvaluationStack& stack);
    
    // IL instruction handlers
    bool EmitLoadConstant(JITContext& context, int value, EvaluationStack& stack);
    bool EmitLoadArgument(JITContext& context, int argIndex, EvaluationStack& stack);
    bool EmitStoreLocal(JITContext& context, int localIndex, EvaluationStack& stack);
    bool EmitArithmetic(JITContext& context, ILOpcode opcode, EvaluationStack& stack);
    bool EmitCall(JITContext& context, DWORD token, EvaluationStack& stack);
    bool EmitReturn(JITContext& context, EvaluationStack& stack);
    
    // Code generation helpers
    void EmitInstruction(JITContext& context, DWORD instruction);
    void EmitInstructions(JITContext& context, const std::vector<DWORD>& instructions);
    void* AllocateCodeMemory(size_t size);
    void FreeCodeMemory(void* ptr, size_t size);
    
    // Method resolution
    void* ResolveMethodCall(DWORD token);
    
    // Validation
    bool ValidateIL(const void* ilCode, size_t ilSize);
};

// Global JIT compiler instance
extern SimpleJIT* g_jitCompiler;

// JIT helper functions (runtime support)
extern "C" {
    // String operations
    __declspec(dllexport) void* JIT_AllocateString(int length);
    __declspec(dllexport) void JIT_InitializeString(void* str, const wchar_t* data);
    
    // Method calls
    __declspec(dllexport) void JIT_CallStaticMethod(void* methodPtr);
    __declspec(dllexport) void* JIT_CallInstanceMethod(void* obj, void* methodPtr);
    
    // Exception handling
    __declspec(dllexport) void JIT_TerminateException(); // Renamed to match the updated function in `SimpleJIT.cpp`
    __declspec(dllexport) void JIT_HandleException(void* exceptionObj);
    
    // GC interaction
    __declspec(dllexport) void* JIT_AllocateObject(MethodTable* methodTable);
    __declspec(dllexport) void JIT_CollectGarbage();
}

// Debugging and diagnostics
class JITDiagnostics {
public:
    static void DumpMethod(MethodDesc* method, const void* nativeCode, size_t size);
    static void DumpIL(const void* ilCode, size_t size);
    static void DumpNativeCode(const void* nativeCode, size_t size);
    static bool ValidateNativeCode(const void* nativeCode, size_t size);
};

// Optimization hints for future enhancement
enum OptimizationLevel {
    OPT_NONE,           // No optimizations
    OPT_BASIC,          // Basic optimizations (constant folding, etc.)
    OPT_AGGRESSIVE      // Advanced optimizations (future)
};

struct JITOptions {
    OptimizationLevel optimization;
    bool enableDebugging;
    bool enableProfiling;
    bool verifyIL;
    
    JITOptions() 
        : optimization(OPT_BASIC)
        , enableDebugging(true)
        , enableProfiling(false)
        , verifyIL(true) {
    }
};

} // namespace Phase1
} // namespace CLRNet

#endif // SIMPLE_JIT_H