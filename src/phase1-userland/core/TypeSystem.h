#pragma once

// Type system implementation for CLRNET Phase 1 runtime
// Manages method tables, type metadata, and object layout

#ifndef TYPE_SYSTEM_H
#define TYPE_SYSTEM_H

#include <windows.h>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

#include "RuntimeTypes.h"

namespace CLRNet {
namespace Phase1 {

// Method table flags
enum MethodTableFlags : DWORD {
    MTF_INTERFACE = 0x00000001,
    MTF_ABSTRACT = 0x00000002,
    MTF_SEALED = 0x00000004,
    MTF_VALUETYPE = 0x00000008,
    MTF_STRING = 0x00000010,
    MTF_ARRAY = 0x00000020
};

// Method flags
enum MethodFlags : DWORD {
    MF_STATIC = 0x00000001,
    MF_VIRTUAL = 0x00000002,
    MF_ABSTRACT = 0x00000004,
    MF_FINAL = 0x00000008,
    MF_PINVOKE = 0x00000010,
    MF_COMPILED = 0x00000020
};

// Field descriptor for type fields
struct FieldDesc {
    DWORD offset;           // Offset within object
    DWORD flags;            // Field attributes
    WORD fieldType;         // Field type token
    char name[64];          // Field name
};

// Method table structure - core of type system
struct MethodTable {
    DWORD flags;            // Type flags (MTF_*)
    DWORD instanceSize;     // Size of instances in bytes
    WORD methodCount;       // Number of methods
    WORD fieldCount;        // Number of fields
    DWORD typeToken;        // Metadata token
    MethodTable* baseClass; // Parent class method table
    char typeName[128];     // Fully qualified type name
    
    // Variable-length arrays (allocated after this struct)
    // MethodDesc methods[methodCount];
    // FieldDesc fields[fieldCount];
    
    // Helper methods
    MethodDesc* GetMethods() {
        return reinterpret_cast<MethodDesc*>(this + 1);
    }
    
    FieldDesc* GetFields() {
        MethodDesc* methods = GetMethods();
        return reinterpret_cast<FieldDesc*>(methods + methodCount);
    }
    
    MethodDesc* FindMethod(const char* name);
    FieldDesc* FindField(const char* name);
    bool IsSubclassOf(MethodTable* other);
    DWORD GetTotalSize();
};

// Basic type information
struct TypeInfo {
    std::string name;
    std::string assembly;
    MethodTable* methodTable;
    bool isLoaded;
    
    TypeInfo() : methodTable(nullptr), isLoaded(false) {}
};

// Core type system class
class TypeSystem {
public:
    TypeSystem();
    ~TypeSystem();
    
    // Initialization
    bool Initialize();
    void Shutdown();
    
    // Type management
    MethodTable* CreateMethodTable(const std::string& typeName, 
                                   DWORD instanceSize, 
                                   WORD methodCount, 
                                   WORD fieldCount);
    
    MethodTable* FindMethodTable(const std::string& typeName);
    bool RegisterMethodTable(const std::string& typeName, MethodTable* methodTable);
    
    // Built-in type support
    MethodTable* GetObjectMethodTable();
    MethodTable* GetStringMethodTable();
    MethodTable* GetInt32MethodTable();
    MethodTable* GetBooleanMethodTable();
    
    // Object creation helpers
    void* AllocateObject(MethodTable* methodTable);
    void InitializeObject(void* obj, MethodTable* methodTable);
    
    // Method resolution
    MethodDesc* ResolveMethod(MethodTable* methodTable, const std::string& methodName);
    MethodDesc* ResolveVirtualMethod(MethodTable* methodTable, WORD slot);
    
    // Type validation
    bool IsValidType(MethodTable* methodTable);
    bool CanCastTo(MethodTable* from, MethodTable* to);
    
private:
    // Core types registry
    std::unordered_map<std::string, MethodTable*> m_methodTables;
    std::unordered_map<std::string, TypeInfo> m_typeInfo;
    
    // Built-in types
    MethodTable* m_objectMT;
    MethodTable* m_stringMT;
    MethodTable* m_int32MT;
    MethodTable* m_booleanMT;
    
    // Memory management
    std::vector<void*> m_allocatedTables;
    
    // Initialization helpers
    bool InitializeBuiltinTypes();
    MethodTable* CreateBuiltinType(const char* name, DWORD size, bool isValueType = false);
    void CleanupAllocations();
    
    // Method table allocation
    MethodTable* AllocateMethodTable(DWORD baseSize, WORD methodCount, WORD fieldCount);
};

// Global type system instance
extern TypeSystem* g_typeSystem;

// Helper functions
const char* GetTypeNameFromToken(DWORD token);
DWORD GetTokenFromTypeName(const char* typeName);
bool IsValueType(MethodTable* methodTable);
bool IsReferenceType(MethodTable* methodTable);

} // namespace Phase1
} // namespace CLRNet

#endif // TYPE_SYSTEM_H