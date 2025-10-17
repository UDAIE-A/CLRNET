#include "GarbageCollector.h"
#include <cassert>
#include <algorithm>
#include <functional>
#include <cstring> // For memcpy

namespace CLRNet {
namespace Phase1 {

// Global instances
GarbageCollector* g_garbageCollector = nullptr;
std::vector<void**> GCRootManager::s_roots;
CRITICAL_SECTION GCRootManager::s_rootsLock;
bool GCRootManager::s_initialized = false;

//=============================================================================
// ManagedHeap Implementation
//=============================================================================

ManagedHeap::ManagedHeap(size_t initialSize, size_t maxSize)
    : m_heapStart(nullptr)
    , m_heapSize(0)
    , m_maxSize(maxSize)
    , m_usedSize(0)
    , m_firstObject(nullptr)
    , m_lastObject(nullptr)
    , m_initialized(false) {
    
    InitializeCriticalSection(&m_heapLock);
}

ManagedHeap::~ManagedHeap() {
    Shutdown();
    DeleteCriticalSection(&m_heapLock);
}

bool ManagedHeap::Initialize() {
    if (m_initialized) return true;
    
    EnterCriticalSection(&m_heapLock);
    
    // Allocate initial heap using VirtualAlloc for better control
    m_heapStart = VirtualAlloc(nullptr, m_maxSize, MEM_RESERVE, PAGE_NOACCESS);
    if (!m_heapStart) {
        LeaveCriticalSection(&m_heapLock);
        return false;
    }
    
    // Commit initial portion
    size_t initialSize = 1024 * 1024; // 1MB initial
    if (!VirtualAlloc(m_heapStart, initialSize, MEM_COMMIT, PAGE_READWRITE)) {
        VirtualFree(m_heapStart, 0, MEM_RELEASE);
        m_heapStart = nullptr;
        LeaveCriticalSection(&m_heapLock);
        return false;
    }
    
    m_heapSize = initialSize;
    m_usedSize = 0;
    m_initialized = true;
    
    LeaveCriticalSection(&m_heapLock);
    return true;
}

void ManagedHeap::Shutdown() {
    if (!m_initialized) return;
    
    EnterCriticalSection(&m_heapLock);
    
    if (m_heapStart) {
        VirtualFree(m_heapStart, 0, MEM_RELEASE);
        m_heapStart = nullptr;
    }
    
    m_heapSize = 0;
    m_usedSize = 0;
    m_firstObject = nullptr;
    m_lastObject = nullptr;
    m_initialized = false;
    
    LeaveCriticalSection(&m_heapLock);
}

void* ManagedHeap::Allocate(size_t size) {
    if (!m_initialized || size == 0) return nullptr;
    
    // Align size to pointer boundary
    size = (size + sizeof(void*) - 1) & ~(sizeof(void*) - 1);
    
    EnterCriticalSection(&m_heapLock);
    
    void* result = AllocateFromHeap(size);
    
    LeaveCriticalSection(&m_heapLock);
    return result;
}

void* ManagedHeap::AllocateFromHeap(size_t size) {
    // Check if we have enough space
    if (m_usedSize + size > m_heapSize) {
        // Try to grow the heap
        size_t newSize = m_heapSize * 2;
        if (newSize > m_maxSize) newSize = m_maxSize;
        
        if (m_usedSize + size > newSize) {
            return nullptr; // Not enough space even after growing
        }
        
        // Commit more memory
        void* newCommit = static_cast<char*>(m_heapStart) + m_heapSize;
        size_t commitSize = newSize - m_heapSize;
        
        if (!VirtualAlloc(newCommit, commitSize, MEM_COMMIT, PAGE_READWRITE)) {
            return nullptr; // Failed to commit more memory
        }
        
        m_heapSize = newSize;
    }
    
    // Allocate from the top of used space
    void* result = static_cast<char*>(m_heapStart) + m_usedSize;
    m_usedSize += size;
    
    return result;
}

bool ManagedHeap::Free(void* ptr) {
    // In mark-and-sweep GC, we don't actually free individual objects
    // Memory is reclaimed during the sweep phase
    return true;
}

void ManagedHeap::EnumerateObjects(std::function<void(GCObjectHeader*)> callback) {
    EnterCriticalSection(&m_heapLock);
    
    GCObjectHeader* current = m_firstObject;
    while (current) {
        GCObjectHeader* next = current->next;
        callback(current);
        current = next;
    }
    
    LeaveCriticalSection(&m_heapLock);
}

void ManagedHeap::LinkObject(GCObjectHeader* obj) {
    obj->next = nullptr;
    
    if (!m_firstObject) {
        m_firstObject = obj;
        m_lastObject = obj;
    } else {
        m_lastObject->next = obj;
        m_lastObject = obj;
    }
}

void ManagedHeap::UnlinkObject(GCObjectHeader* obj) {
    // Simple implementation - in production would use doubly-linked list
    if (obj == m_firstObject) {
        m_firstObject = obj->next;
        if (!m_firstObject) {
            m_lastObject = nullptr;
        }
        return;
    }
    
    GCObjectHeader* prev = m_firstObject;
    while (prev && prev->next != obj) {
        prev = prev->next;
    }
    
    if (prev) {
        prev->next = obj->next;
        if (obj == m_lastObject) {
            m_lastObject = prev;
        }
    }
}

//=============================================================================
// GarbageCollector Implementation
//=============================================================================

GarbageCollector::GarbageCollector()
    : m_initialized(false)
    , m_inCollection(false) {
    
    InitializeCriticalSection(&m_collectorLock);
}

GarbageCollector::~GarbageCollector() {
    Shutdown();
    DeleteCriticalSection(&m_collectorLock);
}

bool GarbageCollector::Initialize(const GCConfig& config) {
    if (m_initialized) return true;
    
    m_config = config;
    
    // Initialize managed heap
    m_heap = std::make_unique<ManagedHeap>(m_config.heapInitialSize, m_config.heapMaxSize);
    if (!m_heap->Initialize()) {
        return false;
    }
    
    // Initialize root manager
    GCRootManager::Initialize();
    
    m_initialized = true;
    
    if (m_config.enableLogging) {
        LogCollection("GC Initialized");
    }
    
    return true;
}

void GarbageCollector::Shutdown() {
    if (!m_initialized) return;
    
    EnterCriticalSection(&m_collectorLock);
    
    if (m_config.enableLogging) {
        LogCollection("GC Shutdown");
    }
    
    m_heap.reset();
    GCRootManager::Shutdown();
    m_initialized = false;
    
    LeaveCriticalSection(&m_collectorLock);
}

void* GarbageCollector::AllocateObject(size_t size) {
    if (!m_initialized) return nullptr;
    
    // Calculate total size including GC header
    size_t totalSize = sizeof(GCObjectHeader) + size;
    
    // Check if we should collect before allocation
    if (ShouldCollect()) {
        std::vector<void**> roots = GCRootManager::GetAllRoots();
        Collect(roots);
    }
    
    // Allocate from heap
    void* memory = m_heap->Allocate(totalSize);
    if (!memory) return nullptr;
    
    // Initialize GC header
    GCObjectHeader* header = static_cast<GCObjectHeader*>(memory);
    header->methodTable = nullptr; // Will be set by caller
    header->syncBlock = 0;
    header->size = totalSize;
    header->next = nullptr;
    header->gcFlags = 0;
    
    // Link into object list
    EnterCriticalSection(&m_collectorLock);
    m_heap->LinkObject(header);
    LeaveCriticalSection(&m_collectorLock);
    
    // Update statistics
    m_stats.totalBytesAllocated += static_cast<DWORD>(totalSize);
    
    // Return user data portion
    return header->GetObjectData();
}

void* GarbageCollector::AllocateArray(size_t elementSize, size_t count) {
    // Simple array allocation - in full implementation would have array header
    return AllocateObject(elementSize * count);
}

void GarbageCollector::PinObject(void* obj) {
    if (!obj) return;
    
    GCObjectHeader* header = GetObjectHeader(obj);
    if (header && IsValidObject(obj)) {
        header->SetPinned();
    }
}

void GarbageCollector::UnpinObject(void* obj) {
    if (!obj) return;
    
    GCObjectHeader* header = GetObjectHeader(obj);
    if (header && IsValidObject(obj)) {
        header->gcFlags &= ~GCObjectHeader::GC_PINNED;
    }
}

void GarbageCollector::Collect(const std::vector<void**>& roots) {
    if (!m_initialized || m_inCollection) return;
    
    DWORD startTime = GetTickCount();
    
    EnterCriticalSection(&m_collectorLock);
    m_inCollection = true;
    
    if (m_config.enableLogging) {
        LogCollection("GC Start");
    }
    
    try {
        // Clear previous marking state
        m_markedObjects.clear();
        
        // Mark phase - mark all reachable objects
        MarkPhase(roots);
        
        // Sweep phase - reclaim unmarked objects  
        SweepPhase();
        
        // Update statistics
        DWORD duration = GetTickCount() - startTime;
        UpdateStatistics(startTime, 0); // TODO: Calculate reclaimed bytes
        
        if (m_config.enableLogging) {
            LogCollection("GC Complete", duration);
        }
    }
    catch (...) {
        if (m_config.enableLogging) {
            LogCollection("GC Error");
        }
    }
    
    m_inCollection = false;
    LeaveCriticalSection(&m_collectorLock);
}

void GarbageCollector::MarkPhase(const std::vector<void**>& roots) {
    if (m_config.enableLogging) {
        LogCollection("Mark Phase Start");
    }
    
    // Mark all objects reachable from roots
    for (void** root : roots) {
        if (root && *root) {
            MarkObject(*root);
        }
    }
    
    if (m_config.enableLogging) {
        LogCollection("Mark Phase Complete");
    }
}

void GarbageCollector::SweepPhase() {
    if (m_config.enableLogging) {
        LogCollection("Sweep Phase Start");
    }
    
    size_t reclaimedBytes = 0;
    std::vector<GCObjectHeader*> toDelete;
    
    // Enumerate all objects and collect unmarked ones
    m_heap->EnumerateObjects([&](GCObjectHeader* obj) {
        if (!obj->IsMarked() && !obj->IsPinned()) {
            toDelete.push_back(obj);
            reclaimedBytes += obj->size;
        } else {
            // Clear mark for next collection
            obj->ClearMarked();
        }
    });
    
    // Remove unmarked objects (simplified - in real GC would compact heap)
    for (GCObjectHeader* obj : toDelete) {
        m_heap->UnlinkObject(obj);
    }
    
    m_stats.totalBytesReclaimed += static_cast<DWORD>(reclaimedBytes);
    
    if (m_config.enableLogging) {
        LogCollection("Sweep Phase Complete");
    }
}

void GarbageCollector::MarkObject(void* obj) {
    if (!obj) return;
    
    GCObjectHeader* header = GetObjectHeader(obj);
    if (!header || !IsValidObject(obj)) return;
    
    // Avoid marking the same object multiple times
    if (header->IsMarked()) return;
    
    header->SetMarked();
    m_markedObjects.insert(header);
    
    // Mark referenced objects recursively
    MarkObjectRecursive(header);
}

void GarbageCollector::MarkObjectRecursive(GCObjectHeader* obj) {
    // Scan object for references to other managed objects
    ScanObjectReferences(obj);
}

void GarbageCollector::ScanObjectReferences(GCObjectHeader* obj) {
    // Simplified reference scanning
    // In real implementation would use type metadata to find reference fields
    
    if (!obj->methodTable) return;
    
    // For now, just scan the object data as potential pointers
    void** data = static_cast<void**>(obj->GetObjectData());
    size_t dataSize = obj->size - sizeof(GCObjectHeader);
    size_t pointerCount = dataSize / sizeof(void*);
    
    for (size_t i = 0; i < pointerCount; i++) {
        void* potentialRef = data[i];
        if (IsValidObject(potentialRef)) {
            MarkObject(potentialRef);
        }
    }
}

GCObjectHeader* GarbageCollector::GetObjectHeader(void* obj) {
    if (!obj) return nullptr;
    
    // Object header is stored before the user data
    return reinterpret_cast<GCObjectHeader*>(
        static_cast<char*>(obj) - sizeof(GCObjectHeader)
    );
}

bool GarbageCollector::IsValidObject(void* obj) {
    if (!obj || !m_heap) return false;
    
    // Check if pointer is within heap bounds
    char* ptr = static_cast<char*>(obj);
    char* heapStart = static_cast<char*>(m_heap->GetTotalSize() > 0 ? 
                                        static_cast<char*>(obj) - m_heap->GetTotalSize() : nullptr);
    char* heapEnd = heapStart + m_heap->GetTotalSize();
    
    // Simplified validation - in real implementation would be more thorough
    return ptr >= heapStart && ptr < heapEnd;
}

void GarbageCollector::ForceCollection() {
    std::vector<void**> roots = GCRootManager::GetAllRoots();
    Collect(roots);
}

bool GarbageCollector::ShouldCollect() const {
    if (!m_heap) return false;
    
    return m_heap->GetUsedSize() >= m_config.collectionThreshold;
}

size_t GarbageCollector::GetTotalMemory() const {
    return m_heap ? m_heap->GetTotalSize() : 0;
}

size_t GarbageCollector::GetUsedMemory() const {
    return m_heap ? m_heap->GetUsedSize() : 0;
}

void GarbageCollector::SetCollectionThreshold(size_t threshold) {
    m_config.collectionThreshold = threshold;
}

void GarbageCollector::EnableLogging(bool enable) {
    m_config.enableLogging = enable;
}

void GarbageCollector::UpdateStatistics(DWORD startTime, size_t reclaimedBytes) {
    m_stats.collectionsCount++;
    m_stats.gen0Collections++; // Simplified - only gen 0 for now
    
    DWORD duration = GetTickCount() - startTime;
    m_stats.averageCollectionTime = 
        (m_stats.averageCollectionTime + duration) / 2;
    
    if (reclaimedBytes > m_stats.largestCollection) {
        m_stats.largestCollection = static_cast<DWORD>(reclaimedBytes);
    }
}

void GarbageCollector::LogCollection(const char* phase, DWORD duration) {
    // Simple logging - in real implementation would use proper logging system
    char buffer[256];
    if (duration > 0) {
        sprintf_s(buffer, sizeof(buffer), "[GC] %s (%u ms)", phase, duration);
    } else {
        sprintf_s(buffer, sizeof(buffer), "[GC] %s", phase);
    }
    
    OutputDebugStringA(buffer);
}

//=============================================================================
// GCRootManager Implementation  
//=============================================================================

void GCRootManager::Initialize() {
    if (s_initialized) return;
    
    InitializeCriticalSection(&s_rootsLock);
    s_initialized = true;
}

void GCRootManager::Shutdown() {
    if (!s_initialized) return;
    
    EnterCriticalSection(&s_rootsLock);
    s_roots.clear();
    LeaveCriticalSection(&s_rootsLock);
    
    DeleteCriticalSection(&s_rootsLock);
    s_initialized = false;
}

void GCRootManager::RegisterRoot(void** root) {
    if (!s_initialized || !root) return;
    
    EnterCriticalSection(&s_rootsLock);
    s_roots.push_back(root);
    LeaveCriticalSection(&s_rootsLock);
}

void GCRootManager::UnregisterRoot(void** root) {
    if (!s_initialized || !root) return;
    
    EnterCriticalSection(&s_rootsLock);
    auto it = std::find(s_roots.begin(), s_roots.end(), root);
    if (it != s_roots.end()) {
        s_roots.erase(it);
    }
    LeaveCriticalSection(&s_rootsLock);
}

std::vector<void**> GCRootManager::GetAllRoots() {
    std::vector<void**> result;
    
    if (!s_initialized) return result;
    
    EnterCriticalSection(&s_rootsLock);
    result = s_roots;
    LeaveCriticalSection(&s_rootsLock);
    
    return result;
}

//=============================================================================
// Helper Functions
//=============================================================================

void* GCAlloc(size_t size) {
    return g_garbageCollector ? g_garbageCollector->AllocateObject(size) : nullptr;
}

void* GCAllocArray(size_t elementSize, size_t count) {
    return g_garbageCollector ? g_garbageCollector->AllocateArray(elementSize, count) : nullptr;
}

void GCCollect() {
    if (g_garbageCollector) {
        g_garbageCollector->ForceCollection();
    }
}

size_t GCGetTotalMemory() {
    return g_garbageCollector ? g_garbageCollector->GetTotalMemory() : 0;
}

} // namespace Phase1
} // namespace CLRNet