#pragma once

#ifndef CLRNET_VM_BYTECODE_COMPILER_H
#define CLRNET_VM_BYTECODE_COMPILER_H

#include <windows.h>
#include <memory>
#include <vector>

#include "VirtualMachine.h"

namespace CLRNet {
namespace Phase1 {
namespace VM {

struct VmProgram;
struct VmInstruction;

// Parses MSIL and emits CLRNET VM bytecode
class BytecodeCompiler {
public:
    BytecodeCompiler();
    ~BytecodeCompiler();

    bool Initialize();
    void Shutdown();

    std::shared_ptr<VmProgram> Compile(const void* ilCode, size_t ilSize, const std::string& cacheKey);

private:
    struct MethodHeader {
        bool isFat;
        uint16_t flags;
        uint16_t maxStack;
        uint32_t codeSize;
        uint32_t localVarSigTok;
        const uint8_t* code;
    };

    bool ParseMethodHeader(const uint8_t* il, size_t size, MethodHeader& header);
    bool DecodeIL(const MethodHeader& header, VmProgram& program);
    bool DecodeInstruction(const uint8_t* il, size_t ilSize, size_t& offset, VmProgram& program);
    int32_t ReadInt32(const uint8_t* il, size_t size, size_t offset);
    int16_t ReadInt16(const uint8_t* il, size_t size, size_t offset);
    int8_t ReadInt8(const uint8_t* il, size_t size, size_t offset);
};

} // namespace VM
} // namespace Phase1
} // namespace CLRNet

#endif // CLRNET_VM_BYTECODE_COMPILER_H
