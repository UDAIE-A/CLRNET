#include "AssemblyLoader.h"
#include "OverlayConfig.h"
#include <cassert>
#include <algorithm>
#include <cwctype>

namespace {

std::wstring CombinePath(const std::wstring& base, const std::wstring& relative) {
    if (relative.empty()) {
        return base;
    }

    if (relative.size() >= 2 && (relative[1] == L':' || (relative[0] == L'\\' && relative[1] == L'\\'))) {
        return relative;
    }

    if (base.empty()) {
        return relative;
    }

    std::wstring result = base;
    if (!result.empty()) {
        wchar_t tail = result.back();
        if (tail != L'\\' && tail != L'/') {
            result.push_back(L'\\');
        }
    }

    result += relative;
    std::replace(result.begin(), result.end(), L'/', L'\\');
    return result;
}

bool LooksLikeAssemblyName(const std::wstring& value) {
    if (value.empty()) {
        return false;
    }

    for (wchar_t ch : value) {
        if (ch == L'\\' || ch == L'/' || ch == L':') {
            return false;
        }
    }

    return true;
}

std::wstring ToLowerInvariant(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), ::towlower);
    return value;
}

std::wstring GetLowercaseExtension(const std::wstring& value) {
    size_t dot = value.find_last_of(L'.');
    if (dot == std::wstring::npos) {
        return std::wstring();
    }

    return ToLowerInvariant(value.substr(dot));
}

std::wstring NormalizeAssemblySimpleName(const std::wstring& value) {
    std::wstring extension = GetLowercaseExtension(value);
    if (extension == L".dll") {
        return value.substr(0, value.length() - 4);
    }

    return value;
}

std::string WideToUtf8(const std::wstring& value) {
    if (value.empty()) {
        return std::string();
    }

    int required = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (required <= 0) {
        return std::string(value.begin(), value.end());
    }

    std::string result(static_cast<size_t>(required), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), &result[0], required, nullptr, nullptr);
    return result;
}

} // namespace

namespace CLRNet {
namespace Phase1 {

// Global assembly loader instance  
AssemblyLoader* g_assemblyLoader = nullptr;

//=============================================================================
// LoadedAssembly Implementation
//=============================================================================

LoadedAssembly::LoadedAssembly(const std::wstring& path)
    : m_path(path)
    , m_loaded(false)
    , m_fileHandle(INVALID_HANDLE_VALUE)
    , m_mappingHandle(nullptr)
    , m_baseAddress(nullptr)
    , m_fileSize(0)
    , m_dosHeader(nullptr)
    , m_peHeader(nullptr)
    , m_optionalHeader(nullptr)
    , m_cliHeader(nullptr)
    , m_metadataRoot(nullptr)
    , m_metadataSize(0) {
}

LoadedAssembly::~LoadedAssembly() {
    Unload();
}

bool LoadedAssembly::Load() {
    if (m_loaded) return true;
    
    try {
        // Map the file into memory
        if (!MapFile()) {
            return false;
        }
        
        // Parse PE headers
        if (!ParsePEHeaders()) {
            UnmapFile();
            return false;
        }
        
        // Parse CLI header
        if (!ParseCLIHeader()) {
            UnmapFile();
            return false;
        }
        
        // Parse metadata
        if (!ParseMetadata()) {
            UnmapFile();
            return false;
        }
        
        m_loaded = true;
        return true;
    }
    catch (...) {
        UnmapFile();
        return false;
    }
}

void LoadedAssembly::Unload() {
    if (!m_loaded) return;
    
    m_typeCache.clear();
    m_streams.clear();
    UnmapFile();
    m_loaded = false;
}

bool LoadedAssembly::MapFile() {
    // Open file
    m_fileHandle = CreateFile(m_path.c_str(), GENERIC_READ, FILE_SHARE_READ, 
                              nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (m_fileHandle == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    // Get file size
    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(m_fileHandle, &fileSize)) {
        CloseHandle(m_fileHandle);
        m_fileHandle = INVALID_HANDLE_VALUE;
        return false;
    }
    m_fileSize = static_cast<size_t>(fileSize.QuadPart);
    
    // Create file mapping
    m_mappingHandle = CreateFileMapping(m_fileHandle, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!m_mappingHandle) {
        CloseHandle(m_fileHandle);
        m_fileHandle = INVALID_HANDLE_VALUE;
        return false;
    }
    
    // Map view of file
    m_baseAddress = MapViewOfFile(m_mappingHandle, FILE_MAP_READ, 0, 0, 0);
    if (!m_baseAddress) {
        CloseHandle(m_mappingHandle);
        CloseHandle(m_fileHandle);
        m_mappingHandle = nullptr;
        m_fileHandle = INVALID_HANDLE_VALUE;
        return false;
    }
    
    return true;
}

void LoadedAssembly::UnmapFile() {
    if (m_baseAddress) {
        UnmapViewOfFile(m_baseAddress);
        m_baseAddress = nullptr;
    }
    
    if (m_mappingHandle) {
        CloseHandle(m_mappingHandle);
        m_mappingHandle = nullptr;
    }
    
    if (m_fileHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(m_fileHandle);
        m_fileHandle = INVALID_HANDLE_VALUE;
    }
    
    m_fileSize = 0;
}

bool LoadedAssembly::ParsePEHeaders() {
    if (!m_baseAddress || m_fileSize < sizeof(DOSHeader)) {
        return false;
    }
    
    // Parse DOS header
    m_dosHeader = static_cast<DOSHeader*>(m_baseAddress);
    if (m_dosHeader->e_magic != 0x5A4D) { // "MZ"
        return false;
    }
    
    // Find PE header
    if (m_dosHeader->e_lfanew >= static_cast<LONG>(m_fileSize)) {
        return false;
    }
    
    m_peHeader = reinterpret_cast<PEHeader*>(
        static_cast<char*>(m_baseAddress) + m_dosHeader->e_lfanew
    );
    
    if (m_peHeader->signature != 0x00004550) { // "PE\0\0"
        return false;
    }
    
    // Parse optional header
    if (m_peHeader->sizeOfOptionalHeader < sizeof(OptionalHeader)) {
        return false;
    }
    
    m_optionalHeader = reinterpret_cast<OptionalHeader*>(m_peHeader + 1);
    
    // Parse section headers
    SectionHeader* firstSection = reinterpret_cast<SectionHeader*>(
        reinterpret_cast<char*>(m_optionalHeader) + m_peHeader->sizeOfOptionalHeader
    );
    
    for (int i = 0; i < m_peHeader->numberOfSections; i++) {
        m_sectionHeaders.push_back(&firstSection[i]);
    }
    
    return true;
}

bool LoadedAssembly::ParseCLIHeader() {
    if (!m_optionalHeader) return false;
    
    // Find CLI header in data directories (index 14 for .NET runtime)
    if (m_optionalHeader->numberOfRvaAndSizes <= 14) {
        return false; // Not a .NET assembly
    }
    
    DataDirectory* dataDirs = reinterpret_cast<DataDirectory*>(
        reinterpret_cast<char*>(m_optionalHeader) + sizeof(OptionalHeader)
    );
    
    DataDirectory& cliDir = dataDirs[14]; // CLI header directory
    if (cliDir.virtualAddress == 0 || cliDir.size == 0) {
        return false; // Not a .NET assembly
    }
    
    // Convert RVA to file pointer
    m_cliHeader = static_cast<CLIHeader*>(RVAToPointer(cliDir.virtualAddress));
    if (!m_cliHeader) {
        return false;
    }
    
    // Validate CLI header
    if (m_cliHeader->headerSize < sizeof(CLIHeader)) {
        return false;
    }
    
    return true;
}

bool LoadedAssembly::ParseMetadata() {
    if (!m_cliHeader) return false;
    
    // Get metadata root
    m_metadataRoot = RVAToPointer(m_cliHeader->metadata.virtualAddress);
    m_metadataSize = m_cliHeader->metadata.size;
    
    if (!m_metadataRoot || m_metadataSize == 0) {
        return false;
    }
    
    // Parse metadata header
    MetadataHeader* metaHeader = static_cast<MetadataHeader*>(m_metadataRoot);
    if (metaHeader->signature != 0x424A5342) { // "BSJB"
        return false;
    }
    
    // Parse streams
    return ParseStreams();
}

bool LoadedAssembly::ParseStreams() {
    if (!m_metadataRoot) return false;
    
    MetadataHeader* metaHeader = static_cast<MetadataHeader*>(m_metadataRoot);
    
    // Skip version string to get to stream headers
    char* versionStart = reinterpret_cast<char*>(metaHeader + 1);
    char* streamHeaderStart = versionStart + ((metaHeader->versionLength + 3) & ~3);
    
    // Parse stream headers
    char* current = streamHeaderStart;
    char* metadataEnd = static_cast<char*>(m_metadataRoot) + m_metadataSize;
    
    while (current < metadataEnd) {
        StreamHeader* streamHeader = reinterpret_cast<StreamHeader*>(current);
        
        // Extract stream name
        std::string streamName(streamHeader->name);
        m_streams[streamName] = streamHeader;
        
        // Move to next stream header
        size_t nameLength = streamName.length() + 1; // Include null terminator
        size_t alignedNameLength = (nameLength + 3) & ~3; // Align to 4-byte boundary
        current += sizeof(DWORD) + sizeof(DWORD) + alignedNameLength;
        
        // Break if we've read all streams (implementation dependent)
        if (streamName.empty()) break;
    }
    
    return true;
}

void* LoadedAssembly::RVAToPointer(DWORD rva) {
    if (!m_baseAddress || rva == 0) return nullptr;
    
    // Find section containing this RVA
    SectionHeader* section = FindSectionByRVA(rva);
    if (!section) return nullptr;
    
    // Calculate file offset
    DWORD fileOffset = rva - section->virtualAddress + section->pointerToRawData;
    
    if (fileOffset >= m_fileSize) return nullptr;
    
    return static_cast<char*>(m_baseAddress) + fileOffset;
}

SectionHeader* LoadedAssembly::FindSection(const char* name) {
    for (SectionHeader* section : m_sectionHeaders) {
        if (strncmp(reinterpret_cast<const char*>(section->name), name, 8) == 0) {
            return section;
        }
    }
    return nullptr;
}

SectionHeader* LoadedAssembly::FindSectionByRVA(DWORD rva) {
    for (SectionHeader* section : m_sectionHeaders) {
        DWORD sectionStart = section->virtualAddress;
        DWORD sectionEnd = sectionStart + section->virtualSize;
        
        if (rva >= sectionStart && rva < sectionEnd) {
            return section;
        }
    }
    return nullptr;
}

std::vector<std::string> LoadedAssembly::GetTypeNames() {
    std::vector<std::string> typeNames;
    
    // Simplified implementation - in real version would parse metadata tables
    // For now, return some placeholder types
    typeNames.push_back("TestApp.Program");
    
    return typeNames;
}

MethodTable* LoadedAssembly::GetMethodTable(const std::string& typeName) {
    // Check cache first
    auto it = m_typeCache.find(typeName);
    if (it != m_typeCache.end()) {
        return it->second;
    }
    
    // Create method table from metadata
    MethodTable* mt = CreateTypeFromMetadata(typeName);
    if (mt) {
        m_typeCache[typeName] = mt;
    }
    
    return mt;
}

MethodTable* LoadedAssembly::CreateTypeFromMetadata(const std::string& typeName) {
    // Simplified implementation - in real version would parse TypeDef table
    
    if (typeName == "TestApp.Program") {
        // Create a simple method table for the test program
        MethodTable* mt = new MethodTable();
        memset(mt, 0, sizeof(MethodTable));
        
        mt->flags = 0;
        mt->instanceSize = sizeof(ObjectHeader);
        mt->methodCount = 1; // Just Main method
        mt->fieldCount = 0;
        mt->typeToken = 0x02000002; // Typical TypeDef token
        mt->baseClass = nullptr; // Will be set to Object later
        strncpy_s(mt->typeName, sizeof(mt->typeName), typeName.c_str(), _TRUNCATE);
        
        return mt;
    }
    
    return nullptr;
}

MethodDesc* LoadedAssembly::FindMethod(const std::string& typeName, const std::string& methodName) {
    MethodTable* mt = GetMethodTable(typeName);
    if (!mt) return nullptr;
    
    return mt->FindMethod(methodName.c_str());
}

void* LoadedAssembly::GetMethodIL(MethodDesc* method) {
    // Simplified - in real implementation would extract IL from metadata
    return method ? method->ilCode : nullptr;
}

//=============================================================================
// AssemblyLoader Implementation
//=============================================================================

AssemblyLoader::AssemblyLoader(TypeSystem* typeSystem)
    : m_typeSystem(typeSystem)
    , m_initialized(false) {
    
    InitializeCriticalSection(&m_loaderLock);
}

AssemblyLoader::~AssemblyLoader() {
    Shutdown();
    DeleteCriticalSection(&m_loaderLock);
}

bool AssemblyLoader::Initialize() {
    if (m_initialized) return true;

    if (!m_typeSystem) return false;

    m_initialized = true;
    LoadOverlayConfiguration();
    return true;
}

void AssemblyLoader::Shutdown() {
    if (!m_initialized) return;
    
    EnterCriticalSection(&m_loaderLock);
    
    // Unload all assemblies
    m_assemblies.clear();
    m_assembliesByName.clear();
    
    m_initialized = false;
    
    LeaveCriticalSection(&m_loaderLock);
}

bool AssemblyLoader::LoadAssembly(const std::wstring& assemblyPath) {
    if (!m_initialized) return false;

    if (LooksLikeAssemblyName(assemblyPath)) {
        std::wstring simpleName = NormalizeAssemblySimpleName(assemblyPath);
        std::wstring extension = GetLowercaseExtension(assemblyPath);
        std::string canonicalName = WideToUtf8(simpleName);

        if (TryEnsureAssemblyByName(canonicalName)) {
            return true;
        }

        std::wstring candidate = extension.empty() ? (simpleName + L".dll") : assemblyPath;
        return LoadAssemblyInternal(candidate);
    }

    return LoadAssemblyInternal(assemblyPath);
}

bool AssemblyLoader::LoadAssemblyInternal(const std::wstring& assemblyPath) {
    EnterCriticalSection(&m_loaderLock);

    try {
        // Check if already loaded
        auto it = m_assemblies.find(assemblyPath);
        if (it != m_assemblies.end()) {
            LeaveCriticalSection(&m_loaderLock);
            return true;
        }

        // Validate file exists and is valid PE
        if (!IsValidPEFile(assemblyPath)) {
            LeaveCriticalSection(&m_loaderLock);
            return false;
        }

        // Create and load assembly
        auto assembly = std::make_unique<LoadedAssembly>(assemblyPath);
        if (!assembly->Load()) {
            LeaveCriticalSection(&m_loaderLock);
            return false;
        }

        // Register assembly
        LoadedAssembly* assemblyPtr = assembly.get();
        m_assemblies[assemblyPath] = std::move(assembly);

        std::string assemblyName = ExtractAssemblyName(assemblyPath);
        m_assembliesByName[assemblyName] = assemblyPtr;

        // Update type system with new types
        UpdateTypeSystem(assemblyPtr);

        LeaveCriticalSection(&m_loaderLock);
        return true;
    }
    catch (...) {
        LeaveCriticalSection(&m_loaderLock);
        return false;
    }
}


bool AssemblyLoader::UnloadAssembly(const std::wstring& assemblyPath) {
    if (!m_initialized) return false;
    
    EnterCriticalSection(&m_loaderLock);
    
    auto it = m_assemblies.find(assemblyPath);
    if (it != m_assemblies.end()) {
        std::string assemblyName = ExtractAssemblyName(assemblyPath);
        m_assembliesByName.erase(assemblyName);
        m_assemblies.erase(it);
    }
    
    LeaveCriticalSection(&m_loaderLock);
    return true;
}

LoadedAssembly* AssemblyLoader::FindAssembly(const std::wstring& assemblyPath) {
    EnterCriticalSection(&m_loaderLock);
    
    auto it = m_assemblies.find(assemblyPath);
    LoadedAssembly* result = (it != m_assemblies.end()) ? it->second.get() : nullptr;
    
    LeaveCriticalSection(&m_loaderLock);
    return result;
}

LoadedAssembly* AssemblyLoader::FindAssemblyByName(const std::string& assemblyName) {
    EnterCriticalSection(&m_loaderLock);

    auto it = m_assembliesByName.find(assemblyName);
    if (it != m_assembliesByName.end()) {
        LoadedAssembly* result = it->second;
        LeaveCriticalSection(&m_loaderLock);
        return result;
    }

    LeaveCriticalSection(&m_loaderLock);

    if (!TryEnsureAssemblyByName(assemblyName)) {
        return nullptr;
    }

    EnterCriticalSection(&m_loaderLock);
    it = m_assembliesByName.find(assemblyName);
    LoadedAssembly* result = (it != m_assembliesByName.end()) ? it->second : nullptr;
    LeaveCriticalSection(&m_loaderLock);
    return result;
}

MethodTable* AssemblyLoader::ResolveType(const std::string& typeName) {
    TryEnsureOverlayAssemblyForType(typeName);

    // Search all loaded assemblies for the type
    EnterCriticalSection(&m_loaderLock);
    
    for (auto& pair : m_assemblies) {
        MethodTable* mt = pair.second->GetMethodTable(typeName);
        if (mt) {
            LeaveCriticalSection(&m_loaderLock);
            return mt;
        }
    }
    
    LeaveCriticalSection(&m_loaderLock);
    return nullptr;
}

MethodTable* AssemblyLoader::ResolveType(const std::string& typeName, const std::string& assemblyName) {
    TryEnsureAssemblyByName(assemblyName);
    LoadedAssembly* assembly = FindAssemblyByName(assemblyName);
    return assembly ? assembly->GetMethodTable(typeName) : nullptr;
}

MethodDesc* AssemblyLoader::ResolveMethod(const std::string& typeName, const std::string& methodName) {
    // Search all loaded assemblies
    EnterCriticalSection(&m_loaderLock);
    
    for (auto& pair : m_assemblies) {
        MethodDesc* method = pair.second->FindMethod(typeName, methodName);
        if (method) {
            LeaveCriticalSection(&m_loaderLock);
            return method;
        }
    }
    
    LeaveCriticalSection(&m_loaderLock);
    return nullptr;
}

void* AssemblyLoader::GetMethodIL(const std::string& typeName, const std::string& methodName) {
    MethodDesc* method = ResolveMethod(typeName, methodName);
    if (!method) return nullptr;
    
    // Find assembly containing this method and get IL
    EnterCriticalSection(&m_loaderLock);
    
    for (auto& pair : m_assemblies) {
        void* il = pair.second->GetMethodIL(method);
        if (il) {
            LeaveCriticalSection(&m_loaderLock);
            return il;
        }
    }
    
    LeaveCriticalSection(&m_loaderLock);
    return nullptr;
}

std::vector<LoadedAssembly*> AssemblyLoader::GetLoadedAssemblies() {
    std::vector<LoadedAssembly*> result;
    
    EnterCriticalSection(&m_loaderLock);
    
    for (auto& pair : m_assemblies) {
        result.push_back(pair.second.get());
    }
    
    LeaveCriticalSection(&m_loaderLock);
    return result;
}

bool AssemblyLoader::ValidateAssembly(const std::wstring& assemblyPath) {
    return IsValidPEFile(assemblyPath);
}

bool AssemblyLoader::IsAssemblyLoaded(const std::wstring& assemblyPath) {
    return FindAssembly(assemblyPath) != nullptr;
}

void AssemblyLoader::RefreshOverlayConfiguration() {
    LoadOverlayConfiguration();
}

bool AssemblyLoader::TryEnsureOverlayAssemblyForType(const std::string& typeName) {
    if (!m_overlayConfig.enabled) {
        return false;
    }

    auto it = m_overlayConfig.typeForwardMap.find(typeName);
    if (it == m_overlayConfig.typeForwardMap.end()) {
        return false;
    }

    return TryEnsureAssemblyByName(it->second);
}

bool AssemblyLoader::TryEnsureAssemblyByName(const std::string& assemblyName) {
    if (assemblyName.empty()) {
        return false;
    }

    EnterCriticalSection(&m_loaderLock);
    bool alreadyLoaded = m_assembliesByName.find(assemblyName) != m_assembliesByName.end();
    LeaveCriticalSection(&m_loaderLock);

    if (alreadyLoaded) {
        return true;
    }

    if (!m_overlayConfig.enabled) {
        return false;
    }

    return LoadAssemblyFromSearchPaths(assemblyName);
}

bool AssemblyLoader::LoadAssemblyFromSearchPaths(const std::string& assemblyName) {
    if (assemblyName.empty()) {
        return false;
    }

    std::wstring assemblyFile = Utf8ToWide(assemblyName);
    if (assemblyFile.find(L'.') == std::wstring::npos) {
        assemblyFile += L".dll";
    }

    bool loaded = false;

    for (const auto& searchPath : m_overlayConfig.searchPaths) {
        std::wstring candidate = CombinePath(searchPath, assemblyFile);
        if (LoadAssembly(candidate)) {
            loaded = true;
            break;
        }
    }

    return loaded;
}

void AssemblyLoader::LoadOverlayConfiguration() {
    m_overlayConfig = OverlayConfigLoader::Load();
}

std::string AssemblyLoader::ExtractAssemblyName(const std::wstring& path) {
    // Extract filename without extension
    size_t lastSlash = path.find_last_of(L"\\/");
    size_t lastDot = path.find_last_of(L'.');
    
    std::wstring filename = path.substr(
        lastSlash == std::wstring::npos ? 0 : lastSlash + 1,
        lastDot == std::wstring::npos ? std::wstring::npos : lastDot - (lastSlash + 1)
    );
    
    // Convert to narrow string
    std::string result;
    result.assign(filename.begin(), filename.end());
    
    return result;
}

bool AssemblyLoader::IsValidPEFile(const std::wstring& path) {
    // Simple validation - check if file exists and has PE signature
    HANDLE file = CreateFile(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    // Read DOS header
    DOSHeader dosHeader;
    DWORD bytesRead;
    bool valid = ReadFile(file, &dosHeader, sizeof(dosHeader), &bytesRead, nullptr) &&
                 bytesRead == sizeof(dosHeader) &&
                 dosHeader.e_magic == 0x5A4D;
    
    CloseHandle(file);
    return valid;
}

void AssemblyLoader::UpdateTypeSystem(LoadedAssembly* assembly) {
    if (!assembly || !m_typeSystem) return;
    
    // Register all types from assembly with type system
    std::vector<std::string> typeNames = assembly->GetTypeNames();
    
    for (const std::string& typeName : typeNames) {
        MethodTable* mt = assembly->GetMethodTable(typeName);
        if (mt) {
            m_typeSystem->RegisterMethodTable(typeName, mt);
        }
    }
}

//=============================================================================
// Helper Functions
//=============================================================================

DWORD TokenFromRID(DWORD rid, MetadataTable table) {
    return (static_cast<DWORD>(table) << 24) | rid;
}

DWORD RIDFromToken(DWORD token) {
    return token & 0x00FFFFFF;
}

MetadataTable TableFromToken(DWORD token) {
    return static_cast<MetadataTable>((token & 0xFF000000) >> 24);
}

std::string ReadString(const void* stringHeap, DWORD offset) {
    if (!stringHeap || offset == 0) return "";
    
    const char* str = static_cast<const char*>(stringHeap) + offset;
    return std::string(str);
}

std::vector<BYTE> ReadBlob(const void* blobHeap, DWORD offset) {
    std::vector<BYTE> result;
    
    if (!blobHeap || offset == 0) return result;
    
    const BYTE* blob = static_cast<const BYTE*>(blobHeap) + offset;
    
    // First bytes indicate size (simplified)
    DWORD size = blob[0];
    if (size & 0x80) {
        // Multi-byte size encoding
        size = ((size & 0x7F) << 8) | blob[1];
        blob += 2;
    } else {
        blob += 1;
    }
    
    result.assign(blob, blob + size);
    return result;
}

//=============================================================================
// ILParser Implementation
//=============================================================================

bool ILParser::IsValidIL(const void* ilCode, size_t size) {
    if (!ilCode || size == 0) return false;
    
    // Basic validation - check for valid IL header
    const BYTE* code = static_cast<const BYTE*>(ilCode);
    
    // Simple heuristic - IL code should start with valid opcodes
    return size > 0 && code[0] < 0xFF; // Simplified check
}

std::vector<BYTE> ILParser::ParseMethodIL(const void* ilCode, size_t size) {
    std::vector<BYTE> result;
    
    if (IsValidIL(ilCode, size)) {
        const BYTE* code = static_cast<const BYTE*>(ilCode);
        result.assign(code, code + size);
    }
    
    return result;
}

size_t ILParser::GetMethodILSize(const void* ilCode) {
    if (!ilCode) return 0;
    
    // Simplified - in real implementation would parse method header
    // For now, assume 32-byte methods
    return 32;
}

} // namespace Phase1
} // namespace CLRNet