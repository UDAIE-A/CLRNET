#pragma once

// Garbage collector implementation for CLRNET Phase 1 runtime  
// Simple mark-and-sweep collector for sandboxed memory management

#ifndef GARBAGE_COLLECTOR_H
#define GARBAGE_COLLECTOR_H

#include <windows.h>
#include <memory>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <functional>

#include "TypeSystem.h"

namespace CLRNet {
namespace Phase1 {

// Forward declarations
struct ObjectHeader;
class TypeSystem;

// GC configuration
struct GCConfig {
    size_t heapInitialSize;     // Initial heap size in bytes
    size_t heapMaxSize;         // Maximum heap size in bytes
    size_t collectionThreshold; // Trigger collection when used memory exceeds this
    bool enableLogging;         // Enable GC event logging
    
    GCConfig() 
        : heapInitialSize(1024 * 1024)      // 1MB initial
        , heapMaxSize(50 * 1024 * 1024)     // 50MB max (WP8.1 constraint)
        , collectionThreshold(512 * 1024)    // 512KB threshold
        , enableLogging(false) {
    }
};

// Object allocation header (extends ObjectHeader)
struct GCObjectHeader : ObjectHeader {
    size_t size;                // Object size including header
    GCObjectHeader* next;       // Next object in allocation list
    DWORD gcFlags;              // GC-specific flags (marked, pinned, etc.)
    
    enum Flags : DWORD {
        GC_MARKED = 0x01,       // Marked during current collection
        GC_PINNED = 0x02,       // Pinned (cannot be moved)
        GC_FINALIZER = 0x04,    // Has finalizer
        GC_LARGE = 0x08         // Large object (>85KB)
    };
    
    void* GetObjectData() {
        return static_cast<char*>(static_cast<void*>(this)) + sizeof(GCObjectHeader);
    }
    
    bool IsMarked() const { return (gcFlags & GC_MARKED) != 0; }
    void SetMarked() { gcFlags |= GC_MARKED; }
    void ClearMarked() { gcFlags &= ~GC_MARKED; }
    
    bool IsPinned() const { return (gcFlags & GC_PINNED) != 0; }
    void SetPinned() { gcFlags |= GC_PINNED; }
};

// Memory heap management
class ManagedHeap {
public:
    ManagedHeap(size_t initialSize, size_t maxSize);
    ~ManagedHeap();
    
    bool Initialize();
    void Shutdown();
    
    void* Allocate(size_t size);
    bool Free(void* ptr);
    void* Resize(void* ptr, size_t newSize);
    
    size_t GetTotalSize() const { return m_heapSize; }
    size_t GetUsedSize() const { return m_usedSize; }
    size_t GetFreeSize() const { return m_heapSize - m_usedSize; }
    
    // Object enumeration for GC
    void EnumerateObjects(std::function<void(GCObjectHeader*)> callback);
    
private:
    void* m_heapStart;
    size_t m_heapSize;
    size_t m_maxSize;
    size_t m_usedSize;
    
    GCObjectHeader* m_firstObject;
    GCObjectHeader* m_lastObject;
    
    CRITICAL_SECTION m_heapLock;
    bool m_initialized;
    
    void* AllocateFromHeap(size_t size);
    void LinkObject(GCObjectHeader* obj);
    void UnlinkObject(GCObjectHeader* obj);

    friend class GarbageCollector;
};

// Statistics and monitoring
struct GCStats {
    DWORD collectionsCount;
    DWORD gen0Collections;
    DWORD totalBytesAllocated;
    DWORD totalBytesReclaimed;
    DWORD largestCollection;
    DWORD averageCollectionTime;
    
    GCStats() {
        memset(this, 0, sizeof(GCStats));
    }
};

// Main garbage collector class
class GarbageCollector {
public:
    GarbageCollector();
    ~GarbageCollector();
    
    // Initialization
    bool Initialize(const GCConfig& config = GCConfig());
    void Shutdown();
    
    // Memory allocation interface  
    void* AllocateObject(size_t size);
    void* AllocateArray(size_t elementSize, size_t count);
    void PinObject(void* obj);
    void UnpinObject(void* obj);
    
    // Collection control
    void Collect(const std::vector<void**>& roots);
    void ForceCollection();
    bool ShouldCollect() const;
    
    // Statistics and monitoring
    const GCStats& GetStatistics() const { return m_stats; }
    size_t GetTotalMemory() const;
    size_t GetUsedMemory() const;
    
    // Configuration
    void SetCollectionThreshold(size_t threshold);
    void EnableLogging(bool enable);
    
private:
    GCConfig m_config;
    std::unique_ptr<ManagedHeap> m_heap;
    GCStats m_stats;
    bool m_initialized;
    
    // Collection state
    bool m_inCollection;
    std::unordered_set<GCObjectHeader*> m_markedObjects;
    std::vector<GCObjectHeader*> m_pendingFinalization;
    
    CRITICAL_SECTION m_collectorLock;
    
    // Collection phases
    void MarkPhase(const std::vector<void**>& roots);
    void SweepPhase();
    void CompactPhase(); // Future enhancement
    
    // Marking helpers
    void MarkObject(void* obj);
    void MarkObjectRecursive(GCObjectHeader* obj);
    void ScanObjectReferences(GCObjectHeader* obj);
    
    // Memory management helpers
    GCObjectHeader* GetObjectHeader(void* obj);
    bool IsValidObject(void* obj);
    void UpdateStatistics(DWORD startTime, size_t reclaimedBytes);
    
    // Logging
    void LogCollection(const char* phase, DWORD duration = 0);
};

// Global garbage collector instance
extern GarbageCollector* g_garbageCollector;

// Helper functions
void* GCAlloc(size_t size);
void* GCAllocArray(size_t elementSize, size_t count);
void GCCollect();
size_t GCGetTotalMemory();

// GC Root management helpers
class GCRootManager {
public:
    static void RegisterRoot(void** root);
    static void UnregisterRoot(void** root);
    static std::vector<void**> GetAllRoots();
    
private:
    static std::vector<void**> s_roots;
    static CRITICAL_SECTION s_rootsLock;
    static bool s_initialized;
    
    static void Initialize();
    static void Shutdown();
    
    friend class GarbageCollector;
};

// RAII helper for pinning objects
class PinnedObject {
public:
    PinnedObject(void* obj) : m_object(obj) {
        if (g_garbageCollector && obj) {
            g_garbageCollector->PinObject(obj);
        }
    }
    
    ~PinnedObject() {
        if (g_garbageCollector && m_object) {
            g_garbageCollector->UnpinObject(m_object);
        }
    }
    
    PinnedObject(const PinnedObject&) = delete;
    PinnedObject& operator=(const PinnedObject&) = delete;
    
private:
    void* m_object;
};

} // namespace Phase1
} // namespace CLRNet

#endif // GARBAGE_COLLECTOR_H