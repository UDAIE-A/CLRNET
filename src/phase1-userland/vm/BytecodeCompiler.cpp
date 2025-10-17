#include "BytecodeCompiler.h"
#include "BytecodeCache.h"

#include "../core/SimpleJIT.h"

#include <corhdr.h>
#include <cassert>
#include <cstring>
#include <vector>

namespace {

constexpr uint16_t IL_NOP = 0x00;
constexpr uint16_t IL_LDARG_0 = 0x02;
constexpr uint16_t IL_LDARG_1 = 0x03;
constexpr uint16_t IL_LDARG_2 = 0x04;
constexpr uint16_t IL_LDARG_3 = 0x05;
constexpr uint16_t IL_LDLOC_0 = 0x06;
constexpr uint16_t IL_LDLOC_1 = 0x07;
constexpr uint16_t IL_LDLOC_2 = 0x08;
constexpr uint16_t IL_LDLOC_3 = 0x09;
constexpr uint16_t IL_STLOC_0 = 0x0A;
constexpr uint16_t IL_STLOC_1 = 0x0B;
constexpr uint16_t IL_STLOC_2 = 0x0C;
constexpr uint16_t IL_STLOC_3 = 0x0D;
constexpr uint16_t IL_LDARG_S = 0x0E;
constexpr uint16_t IL_STARG_S = 0x10;
constexpr uint16_t IL_LDLOC_S = 0x11;
constexpr uint16_t IL_STLOC_S = 0x13;
constexpr uint16_t IL_LDC_I4_M1 = 0x15;
constexpr uint16_t IL_LDC_I4_0 = 0x16;
constexpr uint16_t IL_LDC_I4_1 = 0x17;
constexpr uint16_t IL_LDC_I4_2 = 0x18;
constexpr uint16_t IL_LDC_I4_3 = 0x19;
constexpr uint16_t IL_LDC_I4_4 = 0x1A;
constexpr uint16_t IL_LDC_I4_5 = 0x1B;
constexpr uint16_t IL_LDC_I4_6 = 0x1C;
constexpr uint16_t IL_LDC_I4_7 = 0x1D;
constexpr uint16_t IL_LDC_I4_8 = 0x1E;
constexpr uint16_t IL_LDC_I4_S = 0x1F;
constexpr uint16_t IL_LDC_I4 = 0x20;
constexpr uint16_t IL_LDC_I8 = 0x21;
constexpr uint16_t IL_LDNULL = 0x14;
constexpr uint16_t IL_LDSTR = 0x72;
constexpr uint16_t IL_CALL = 0x28;
constexpr uint16_t IL_RET = 0x2A;
constexpr uint16_t IL_BR_S = 0x2B;
constexpr uint16_t IL_BRFALSE_S = 0x2C;
constexpr uint16_t IL_BRTRUE_S = 0x2D;
constexpr uint16_t IL_BR = 0x38;
constexpr uint16_t IL_BRFALSE = 0x39;
constexpr uint16_t IL_BRTRUE = 0x3A;
constexpr uint16_t IL_ADD = 0x58;
constexpr uint16_t IL_SUB = 0x59;
constexpr uint16_t IL_MUL = 0x5A;
constexpr uint16_t IL_DIV = 0x5B;
constexpr uint16_t IL_CALLVIRT = 0x6F;
constexpr uint16_t IL_NEWOBJ = 0x73;
constexpr uint16_t IL_CASTCLASS = 0x74;
constexpr uint16_t IL_LDFLD = 0x7B;
constexpr uint16_t IL_STFLD = 0x7D;
constexpr uint16_t IL_BOX = 0x8C;
constexpr uint16_t IL_UNBOX_ANY = 0xA5;

} // namespace

namespace CLRNet {
namespace Phase1 {
namespace VM {

BytecodeCompiler::BytecodeCompiler() {
}

BytecodeCompiler::~BytecodeCompiler() {
}

bool BytecodeCompiler::Initialize() {
    return true;
}

void BytecodeCompiler::Shutdown() {
}

std::shared_ptr<VmProgram> BytecodeCompiler::Compile(const void* ilCode, size_t ilSize, const std::string& cacheKey) {
    if (!ilCode || ilSize == 0) {
        return nullptr;
    }

    const uint8_t* il = static_cast<const uint8_t*>(ilCode);
    MethodHeader header{};
    if (!ParseMethodHeader(il, ilSize, header)) {
        return nullptr;
    }

    auto program = std::make_shared<VmProgram>();
    program->cacheKey = cacheKey;
    program->argumentCount = 0;
    program->localCount = 0;

    if (!DecodeIL(header, *program)) {
        return nullptr;
    }

    return program;
}

bool BytecodeCompiler::ParseMethodHeader(const uint8_t* il, size_t size, MethodHeader& header) {
    if (size < 1) {
        return false;
    }

    uint8_t first = il[0];
    if ((first & CorILMethod_FormatMask) == CorILMethod_TinyFormat) {
        header.isFat = false;
        header.flags = first & CorILMethod_FormatMask;
        header.maxStack = 8; // Tiny methods implicitly allocate 8 stack entries
        header.codeSize = first >> 2;
        header.localVarSigTok = 0;
        header.code = il + 1;
        return size >= (1 + header.codeSize);
    }

    if (size < sizeof(COR_ILMETHOD_FAT)) {
        return false;
    }

    const COR_ILMETHOD_FAT* fat = reinterpret_cast<const COR_ILMETHOD_FAT*>(il);
    header.isFat = true;
    header.flags = fat->Flags;
    header.maxStack = fat->MaxStack;
    header.codeSize = fat->CodeSize;
    header.localVarSigTok = fat->LocalVarSigTok;
    header.code = il + fat->Size * 4;

    return size >= (fat->Size * 4 + header.codeSize);
}

bool BytecodeCompiler::DecodeIL(const MethodHeader& header, VmProgram& program) {
    size_t offset = 0;
    std::vector<size_t> offsetToInstruction(header.codeSize + 1, SIZE_MAX);

    while (offset < header.codeSize) {
        offsetToInstruction[offset] = program.instructions.size();
        if (!DecodeInstruction(header.code, header.codeSize, offset, program)) {
            return false;
        }
    }

    for (auto& fixup : program.branchFixups) {
        size_t instructionIndex = fixup.first;
        int32_t targetOffset = fixup.second;
        if (instructionIndex >= program.instructions.size()) {
            return false;
        }
        if (targetOffset < 0 || static_cast<size_t>(targetOffset) >= offsetToInstruction.size()) {
            return false;
        }
        size_t targetInstruction = offsetToInstruction[static_cast<size_t>(targetOffset)];
        if (targetInstruction == SIZE_MAX) {
            return false;
        }
        program.instructions[instructionIndex].operand0 = static_cast<int32_t>(targetInstruction);
    }

    program.branchFixups.clear();
    return true;
}

bool BytecodeCompiler::DecodeInstruction(const uint8_t* il, size_t ilSize, size_t& offset, VmProgram& program) {
    if (offset >= ilSize) {
        return false;
    }

    size_t instructionOffset = offset;
    uint8_t opcode = il[offset++];
    uint16_t fullOpcode = opcode;

    if (opcode == 0xFE) {
        if (offset >= ilSize) {
            return false;
        }
        uint8_t second = il[offset++];
        fullOpcode = static_cast<uint16_t>((opcode << 8) | second);
    }

    switch (fullOpcode) {
    case IL_NOP:
        program.instructions.emplace_back(VmOpcode::Nop);
        break;
    case IL_LDARG_0:
    case IL_LDARG_1:
    case IL_LDARG_2:
    case IL_LDARG_3: {
        int index = fullOpcode - IL_LDARG_0;
        program.instructions.emplace_back(VmOpcode::LoadArgument, index);
        if (static_cast<uint32_t>(index + 1) > program.argumentCount) {
            program.argumentCount = index + 1;
        }
        break;
    }
    case IL_LDLOC_0:
    case IL_LDLOC_1:
    case IL_LDLOC_2:
    case IL_LDLOC_3: {
        int index = fullOpcode - IL_LDLOC_0;
        program.instructions.emplace_back(VmOpcode::LoadLocal, index);
        if (static_cast<uint32_t>(index + 1) > program.localCount) {
            program.localCount = index + 1;
        }
        break;
    }
    case IL_STLOC_0:
    case IL_STLOC_1:
    case IL_STLOC_2:
    case IL_STLOC_3: {
        int index = fullOpcode - IL_STLOC_0;
        program.instructions.emplace_back(VmOpcode::StoreLocal, index);
        if (static_cast<uint32_t>(index + 1) > program.localCount) {
            program.localCount = index + 1;
        }
        break;
    }
    case IL_LDARG_S: {
        int index = ReadInt8(il, ilSize, offset);
        offset += 1;
        program.instructions.emplace_back(VmOpcode::LoadArgument, index);
        if (static_cast<uint32_t>(index + 1) > program.argumentCount) {
            program.argumentCount = index + 1;
        }
        break;
    }
    case IL_STARG_S: {
        int index = ReadInt8(il, ilSize, offset);
        offset += 1;
        program.instructions.emplace_back(VmOpcode::StoreArgument, index);
        if (static_cast<uint32_t>(index + 1) > program.argumentCount) {
            program.argumentCount = index + 1;
        }
        break;
    }
    case IL_LDLOC_S: {
        int index = ReadInt8(il, ilSize, offset);
        offset += 1;
        program.instructions.emplace_back(VmOpcode::LoadLocal, index);
        if (static_cast<uint32_t>(index + 1) > program.localCount) {
            program.localCount = index + 1;
        }
        break;
    }
    case IL_STLOC_S: {
        int index = ReadInt8(il, ilSize, offset);
        offset += 1;
        program.instructions.emplace_back(VmOpcode::StoreLocal, index);
        if (static_cast<uint32_t>(index + 1) > program.localCount) {
            program.localCount = index + 1;
        }
        break;
    }
    case IL_LDC_I4_M1:
        program.instructions.emplace_back(VmOpcode::LoadConstantI4, -1);
        break;
    case IL_LDC_I4_0:
    case IL_LDC_I4_1:
    case IL_LDC_I4_2:
    case IL_LDC_I4_3:
    case IL_LDC_I4_4:
    case IL_LDC_I4_5:
    case IL_LDC_I4_6:
    case IL_LDC_I4_7:
    case IL_LDC_I4_8: {
        int value = fullOpcode - IL_LDC_I4_0;
        program.instructions.emplace_back(VmOpcode::LoadConstantI4, value);
        break;
    }
    case IL_LDC_I4_S: {
        int value = ReadInt8(il, ilSize, offset);
        offset += 1;
        program.instructions.emplace_back(VmOpcode::LoadConstantI4, value);
        break;
    }
    case IL_LDC_I4: {
        int value = ReadInt32(il, ilSize, offset);
        offset += 4;
        program.instructions.emplace_back(VmOpcode::LoadConstantI4, value);
        break;
    }
    case IL_LDC_I8: {
        if (offset + 8 > ilSize) {
            return false;
        }
        int64_t value = 0;
        std::memcpy(&value, il + offset, sizeof(int64_t));
        offset += 8;
        program.instructions.emplace_back(VmOpcode::LoadConstantI8,
                                           static_cast<int32_t>(value & 0xFFFFFFFF),
                                           static_cast<int32_t>((value >> 32) & 0xFFFFFFFF));
        break;
    }
    case IL_ADD:
        program.instructions.emplace_back(VmOpcode::Add);
        break;
    case IL_SUB:
        program.instructions.emplace_back(VmOpcode::Subtract);
        break;
    case IL_MUL:
        program.instructions.emplace_back(VmOpcode::Multiply);
        break;
    case IL_DIV:
        program.instructions.emplace_back(VmOpcode::Divide);
        break;
    case IL_BR_S: {
        int8_t delta = ReadInt8(il, ilSize, offset);
        offset += 1;
        int32_t target = static_cast<int32_t>(offset) + delta;
        size_t index = program.instructions.size();
        program.instructions.emplace_back(VmOpcode::Branch);
        program.branchFixups.emplace_back(index, target);
        break;
    }
    case IL_BR: {
        int32_t delta = ReadInt32(il, ilSize, offset);
        offset += 4;
        int32_t target = static_cast<int32_t>(offset) + delta;
        size_t index = program.instructions.size();
        program.instructions.emplace_back(VmOpcode::Branch);
        program.branchFixups.emplace_back(index, target);
        break;
    }
    case IL_BRTRUE_S: {
        int8_t delta = ReadInt8(il, ilSize, offset);
        offset += 1;
        int32_t target = static_cast<int32_t>(offset) + delta;
        size_t index = program.instructions.size();
        program.instructions.emplace_back(VmOpcode::BranchIfTrue);
        program.branchFixups.emplace_back(index, target);
        break;
    }
    case IL_BRTRUE: {
        int32_t delta = ReadInt32(il, ilSize, offset);
        offset += 4;
        int32_t target = static_cast<int32_t>(offset) + delta;
        size_t index = program.instructions.size();
        program.instructions.emplace_back(VmOpcode::BranchIfTrue);
        program.branchFixups.emplace_back(index, target);
        break;
    }
    case IL_BRFALSE_S: {
        int8_t delta = ReadInt8(il, ilSize, offset);
        offset += 1;
        int32_t target = static_cast<int32_t>(offset) + delta;
        size_t index = program.instructions.size();
        program.instructions.emplace_back(VmOpcode::BranchIfFalse);
        program.branchFixups.emplace_back(index, target);
        break;
    }
    case IL_BRFALSE: {
        int32_t delta = ReadInt32(il, ilSize, offset);
        offset += 4;
        int32_t target = static_cast<int32_t>(offset) + delta;
        size_t index = program.instructions.size();
        program.instructions.emplace_back(VmOpcode::BranchIfFalse);
        program.branchFixups.emplace_back(index, target);
        break;
    }
    case IL_CALL:
    case IL_CALLVIRT: {
        uint32_t token = static_cast<uint32_t>(ReadInt32(il, ilSize, offset));
        offset += 4;
        VmCallSite callSite;
        callSite.kind = VmCallSite::TargetKind::ManagedMethod;
        callSite.metadataToken = token;
        size_t callIndex = program.callSites.size();
        program.callSites.push_back(callSite);
        program.instructions.emplace_back(fullOpcode == IL_CALL ? VmOpcode::Call : VmOpcode::CallVirtual,
                                           static_cast<int32_t>(callIndex));
        break;
    }
    case IL_NEWOBJ: {
        uint32_t token = static_cast<uint32_t>(ReadInt32(il, ilSize, offset));
        offset += 4;
        VmCallSite callSite;
        callSite.kind = VmCallSite::TargetKind::ManagedMethod;
        callSite.metadataToken = token;
        size_t callIndex = program.callSites.size();
        program.callSites.push_back(callSite);
        program.instructions.emplace_back(VmOpcode::NewObject, static_cast<int32_t>(callIndex));
        break;
    }
    case IL_LDFLD:
        program.instructions.emplace_back(VmOpcode::LoadField, ReadInt32(il, ilSize, offset));
        offset += 4;
        break;
    case IL_STFLD:
        program.instructions.emplace_back(VmOpcode::StoreField, ReadInt32(il, ilSize, offset));
        offset += 4;
        break;
    case IL_BOX:
        program.instructions.emplace_back(VmOpcode::Box, ReadInt32(il, ilSize, offset));
        offset += 4;
        break;
    case IL_UNBOX_ANY:
        program.instructions.emplace_back(VmOpcode::UnboxAny, ReadInt32(il, ilSize, offset));
        offset += 4;
        break;
    case IL_CASTCLASS:
        program.instructions.emplace_back(VmOpcode::CastClass, ReadInt32(il, ilSize, offset));
        offset += 4;
        break;
    case IL_LDSTR:
        program.instructions.emplace_back(VmOpcode::LoadString, ReadInt32(il, ilSize, offset));
        offset += 4;
        break;
    case IL_LDNULL:
        program.instructions.emplace_back(VmOpcode::LoadNull);
        break;
    case IL_RET:
        program.instructions.emplace_back(VmOpcode::Return);
        break;
    default:
        // Unsupported opcode - abort compilation
        return false;
    }

    return true;
}

int32_t BytecodeCompiler::ReadInt32(const uint8_t* il, size_t size, size_t offset) {
    if (offset + 4 > size) {
        return 0;
    }

    return static_cast<int32_t>(il[offset] |
                                (il[offset + 1] << 8) |
                                (il[offset + 2] << 16) |
                                (il[offset + 3] << 24));
}

int16_t BytecodeCompiler::ReadInt16(const uint8_t* il, size_t size, size_t offset) {
    if (offset + 2 > size) {
        return 0;
    }

    return static_cast<int16_t>(il[offset] | (il[offset + 1] << 8));
}

int8_t BytecodeCompiler::ReadInt8(const uint8_t* il, size_t size, size_t offset) {
    if (offset >= size) {
        return 0;
    }
    return static_cast<int8_t>(il[offset]);
}

} // namespace VM
} // namespace Phase1
} // namespace CLRNet
