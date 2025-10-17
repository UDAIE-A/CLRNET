#pragma once

// Shared runtime type definitions for the Phase 1 CLRNET runtime.
// Provides the fundamental object and method descriptors used by
// the execution engine, type system, and garbage collector.

#ifndef CLRNET_RUNTIME_TYPES_H
#define CLRNET_RUNTIME_TYPES_H

#include <windows.h>

namespace CLRNet {
namespace Phase1 {

struct MethodTable;

// Basic object header for managed objects.
struct ObjectHeader {
    MethodTable* methodTable;
    DWORD syncBlock;
};

// Method descriptor for executable methods.
struct MethodDesc {
    DWORD flags;
    WORD slotNumber;
    WORD tokenRemainder;
    void* nativeCode;
    void* ilCode;
    DWORD ilCodeSize;
};

} // namespace Phase1
} // namespace CLRNet

#endif // CLRNET_RUNTIME_TYPES_H
