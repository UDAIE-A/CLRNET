#include "TypeSystem.h"
#include <cstring>
#include <cassert>

namespace CLRNet {
namespace Phase1 {

// Global type system instance
TypeSystem* g_typeSystem = nullptr;

TypeSystem::TypeSystem() 
    : m_objectMT(nullptr)
    , m_stringMT(nullptr)  
    , m_int32MT(nullptr)
    , m_booleanMT(nullptr) {
}

TypeSystem::~TypeSystem() {
    Shutdown();
}

bool TypeSystem::Initialize() {
    // Initialize built-in types first
    if (!InitializeBuiltinTypes()) {
        return false;
    }
    
    return true;
}

void TypeSystem::Shutdown() {
    CleanupAllocations();
    m_methodTables.clear();
    m_typeInfo.clear();
}

bool TypeSystem::InitializeBuiltinTypes() {
    // Create System.Object - root of all types
    m_objectMT = CreateBuiltinType("System.Object", sizeof(ObjectHeader), false);
    if (!m_objectMT) return false;
    
    // Create System.String
    m_stringMT = CreateBuiltinType("System.String", sizeof(ObjectHeader) + sizeof(wchar_t*), false);
    if (!m_stringMT) return false;
    m_stringMT->flags |= MTF_STRING;
    m_stringMT->baseClass = m_objectMT;
    
    // Create System.Int32 (value type)
    m_int32MT = CreateBuiltinType("System.Int32", sizeof(int), true);
    if (!m_int32MT) return false;
    m_int32MT->baseClass = m_objectMT; // In reality would be ValueType
    
    // Create System.Boolean (value type)
    m_booleanMT = CreateBuiltinType("System.Boolean", sizeof(bool), true);
    if (!m_booleanMT) return false;
    m_booleanMT->baseClass = m_objectMT; // In reality would be ValueType
    
    // Register built-in types
    RegisterMethodTable("System.Object", m_objectMT);
    RegisterMethodTable("System.String", m_stringMT);
    RegisterMethodTable("System.Int32", m_int32MT);
    RegisterMethodTable("System.Boolean", m_booleanMT);
    
    return true;
}

MethodTable* TypeSystem::CreateBuiltinType(const char* name, DWORD size, bool isValueType) {
    // Allocate method table with minimal methods (just constructor for now)
    MethodTable* mt = AllocateMethodTable(sizeof(MethodTable), 1, 0);
    if (!mt) return nullptr;
    
    // Initialize method table
    mt->flags = isValueType ? MTF_VALUETYPE : 0;
    mt->instanceSize = size;
    mt->methodCount = 1;
    mt->fieldCount = 0;
    mt->typeToken = GetTokenFromTypeName(name);
    mt->baseClass = nullptr; // Will be set later for inheritance
    strncpy_s(mt->typeName, sizeof(mt->typeName), name, _TRUNCATE);
    
    // Initialize default constructor method
    MethodDesc* methods = mt->GetMethods();
    methods[0].flags = MF_STATIC; // Static constructor
    methods[0].slotNumber = 0;
    methods[0].tokenRemainder = 0;
    methods[0].nativeCode = nullptr;
    methods[0].ilCode = nullptr;
    methods[0].ilCodeSize = 0;
    
    return mt;
}

MethodTable* TypeSystem::CreateMethodTable(const std::string& typeName, 
                                           DWORD instanceSize, 
                                           WORD methodCount, 
                                           WORD fieldCount) {
    
    MethodTable* mt = AllocateMethodTable(sizeof(MethodTable), methodCount, fieldCount);
    if (!mt) return nullptr;
    
    // Initialize method table
    mt->flags = 0;
    mt->instanceSize = instanceSize;
    mt->methodCount = methodCount;
    mt->fieldCount = fieldCount;
    mt->typeToken = GetTokenFromTypeName(typeName.c_str());
    mt->baseClass = m_objectMT; // Default to Object base class
    strncpy_s(mt->typeName, sizeof(mt->typeName), typeName.c_str(), _TRUNCATE);
    
    // Initialize methods and fields to default values
    MethodDesc* methods = mt->GetMethods();
    for (WORD i = 0; i < methodCount; i++) {
        methods[i].flags = 0;
        methods[i].slotNumber = i;
        methods[i].tokenRemainder = 0;
        methods[i].nativeCode = nullptr;
        methods[i].ilCode = nullptr;
        methods[i].ilCodeSize = 0;
    }
    
    FieldDesc* fields = mt->GetFields();
    for (WORD i = 0; i < fieldCount; i++) {
        fields[i].offset = 0;
        fields[i].flags = 0;
        fields[i].fieldType = 0;
        memset(fields[i].name, 0, sizeof(fields[i].name));
    }
    
    return mt;
}

MethodTable* TypeSystem::AllocateMethodTable(DWORD baseSize, WORD methodCount, WORD fieldCount) {
    // Calculate total size needed
    DWORD totalSize = baseSize + 
                     (methodCount * sizeof(MethodDesc)) + 
                     (fieldCount * sizeof(FieldDesc));
    
    // Allocate memory
    void* memory = malloc(totalSize);
    if (!memory) return nullptr;
    
    memset(memory, 0, totalSize);
    m_allocatedTables.push_back(memory);
    
    return static_cast<MethodTable*>(memory);
}

MethodTable* TypeSystem::FindMethodTable(const std::string& typeName) {
    auto it = m_methodTables.find(typeName);
    return (it != m_methodTables.end()) ? it->second : nullptr;
}

bool TypeSystem::RegisterMethodTable(const std::string& typeName, MethodTable* methodTable) {
    if (!methodTable) return false;
    
    m_methodTables[typeName] = methodTable;
    
    // Update type info
    TypeInfo& info = m_typeInfo[typeName];
    info.name = typeName;
    info.methodTable = methodTable;
    info.isLoaded = true;
    
    return true;
}

MethodTable* TypeSystem::GetObjectMethodTable() {
    return m_objectMT;
}

MethodTable* TypeSystem::GetStringMethodTable() {
    return m_stringMT;
}

MethodTable* TypeSystem::GetInt32MethodTable() {
    return m_int32MT;
}

MethodTable* TypeSystem::GetBooleanMethodTable() {
    return m_booleanMT;
}

void* TypeSystem::AllocateObject(MethodTable* methodTable) {
    if (!methodTable) return nullptr;
    
    // Allocate object memory
    void* obj = malloc(methodTable->instanceSize);
    if (!obj) return nullptr;
    
    InitializeObject(obj, methodTable);
    return obj;
}

void TypeSystem::InitializeObject(void* obj, MethodTable* methodTable) {
    if (!obj || !methodTable) return;
    
    // Initialize object header
    ObjectHeader* header = static_cast<ObjectHeader*>(obj);
    header->methodTable = methodTable;
    header->syncBlock = 0;
    
    // Zero out the rest of the object
    if (methodTable->instanceSize > sizeof(ObjectHeader)) {
        memset(static_cast<char*>(obj) + sizeof(ObjectHeader), 0, 
               methodTable->instanceSize - sizeof(ObjectHeader));
    }
}

MethodDesc* TypeSystem::ResolveMethod(MethodTable* methodTable, const std::string& methodName) {
    if (!methodTable) return nullptr;
    
    return methodTable->FindMethod(methodName.c_str());
}

MethodDesc* TypeSystem::ResolveVirtualMethod(MethodTable* methodTable, WORD slot) {
    if (!methodTable || slot >= methodTable->methodCount) {
        return nullptr;
    }
    
    MethodDesc* methods = methodTable->GetMethods();
    return &methods[slot];
}

bool TypeSystem::IsValidType(MethodTable* methodTable) {
    if (!methodTable) return false;
    
    // Basic validation
    if (methodTable->instanceSize == 0) return false;
    if (methodTable->instanceSize < sizeof(ObjectHeader) && 
        !(methodTable->flags & MTF_VALUETYPE)) {
        return false;
    }
    
    return true;
}

bool TypeSystem::CanCastTo(MethodTable* from, MethodTable* to) {
    if (!from || !to) return false;
    if (from == to) return true;
    
    // Check inheritance chain
    return from->IsSubclassOf(to);
}

void TypeSystem::CleanupAllocations() {
    for (void* ptr : m_allocatedTables) {
        free(ptr);
    }
    m_allocatedTables.clear();
}

// MethodTable member function implementations
MethodDesc* MethodTable::FindMethod(const char* name) {
    // Simple linear search for now - could be optimized with hash table
    MethodDesc* methods = GetMethods();
    for (WORD i = 0; i < methodCount; i++) {
        // Note: In real implementation, would compare actual method names
        // For now, just return first method as placeholder
        if (i == 0) return &methods[i];
    }
    return nullptr;
}

FieldDesc* MethodTable::FindField(const char* name) {
    FieldDesc* fields = GetFields();
    for (WORD i = 0; i < fieldCount; i++) {
        if (strcmp(fields[i].name, name) == 0) {
            return &fields[i];
        }
    }
    return nullptr;
}

bool MethodTable::IsSubclassOf(MethodTable* other) {
    if (!other) return false;
    
    MethodTable* current = this->baseClass;
    while (current) {
        if (current == other) return true;
        current = current->baseClass;
    }
    
    return false;
}

DWORD MethodTable::GetTotalSize() {
    return sizeof(MethodTable) + 
           (methodCount * sizeof(MethodDesc)) +
           (fieldCount * sizeof(FieldDesc));
}

// Helper function implementations
const char* GetTypeNameFromToken(DWORD token) {
    // Simplified token to name mapping
    switch (token) {
        case 1: return "System.Object";
        case 2: return "System.String"; 
        case 3: return "System.Int32";
        case 4: return "System.Boolean";
        default: return "Unknown";
    }
}

DWORD GetTokenFromTypeName(const char* typeName) {
    // Simplified name to token mapping
    if (strcmp(typeName, "System.Object") == 0) return 1;
    if (strcmp(typeName, "System.String") == 0) return 2;
    if (strcmp(typeName, "System.Int32") == 0) return 3;
    if (strcmp(typeName, "System.Boolean") == 0) return 4;
    
    // Generate simple hash for unknown types
    DWORD hash = 0;
    for (const char* p = typeName; *p; p++) {
        hash = hash * 31 + *p;
    }
    return hash;
}

bool IsValueType(MethodTable* methodTable) {
    return methodTable && (methodTable->flags & MTF_VALUETYPE);
}

bool IsReferenceType(MethodTable* methodTable) {
    return methodTable && !(methodTable->flags & MTF_VALUETYPE);
}

} // namespace Phase1
} // namespace CLRNet