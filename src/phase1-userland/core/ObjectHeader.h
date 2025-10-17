#ifndef OBJECT_HEADER_H
#define OBJECT_HEADER_H

// Object header structure for all managed objects
struct ObjectHeader {
    class MethodTable; // Forward declaration to resolve circular dependency
    MethodTable* methodTable; // Pointer to method table
    DWORD syncBlock;         // Synchronization block index
};

#endif // OBJECT_HEADER_H