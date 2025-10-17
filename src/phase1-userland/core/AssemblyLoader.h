#pragma once

// Assembly loader for CLRNET Phase 1 runtime
// Handles PE/COFF file parsing and .NET metadata extraction

#ifndef ASSEMBLY_LOADER_H
#define ASSEMBLY_LOADER_H

#include <windows.h>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include "TypeSystem.h"
#include "OverlayConfig.h"

namespace CLRNet {
namespace Phase1 {

// Forward declarations
class TypeSystem;
struct MethodDesc;

// PE/COFF structures (simplified)
#pragma pack(push, 1)

struct DOSHeader {
    WORD e_magic;      // Magic number
    WORD e_cblp;       // Bytes on last page
    WORD e_cp;         // Pages in file
    WORD e_crlc;       // Relocations
    WORD e_cparhdr;    // Size of header in paragraphs
    WORD e_minalloc;   // Minimum extra paragraphs needed
    WORD e_maxalloc;   // Maximum extra paragraphs needed
    WORD e_ss;         // Initial relative SS value
    WORD e_sp;         // Initial SP value
    WORD e_csum;       // Checksum
    WORD e_ip;         // Initial IP value
    WORD e_cs;         // Initial relative CS value
    WORD e_lfarlc;     // File address of relocation table
    WORD e_ovno;       // Overlay number
    WORD e_res[4];     // Reserved words
    WORD e_oemid;      // OEM identifier
    WORD e_oeminfo;    // OEM information
    WORD e_res2[10];   // Reserved words
    LONG e_lfanew;     // File address of new exe header
};

struct PEHeader {
    DWORD signature;           // PE signature
    WORD machine;             // Machine type
    WORD numberOfSections;    // Number of sections
    DWORD timeDateStamp;      // Time/date stamp
    DWORD pointerToSymbolTable; // Pointer to symbol table
    DWORD numberOfSymbols;    // Number of symbols
    WORD sizeOfOptionalHeader; // Size of optional header
    WORD characteristics;     // Characteristics
};

struct OptionalHeader {
    WORD magic;
    BYTE majorLinkerVersion;
    BYTE minorLinkerVersion;
    DWORD sizeOfCode;
    DWORD sizeOfInitializedData;
    DWORD sizeOfUninitializedData;
    DWORD addressOfEntryPoint;
    DWORD baseOfCode;
    DWORD baseOfData;           // PE32 only
    DWORD imageBase;
    DWORD sectionAlignment;
    DWORD fileAlignment;
    WORD majorOperatingSystemVersion;
    WORD minorOperatingSystemVersion;
    WORD majorImageVersion;
    WORD minorImageVersion;
    WORD majorSubsystemVersion;
    WORD minorSubsystemVersion;
    DWORD win32VersionValue;
    DWORD sizeOfImage;
    DWORD sizeOfHeaders;
    DWORD checkSum;
    WORD subsystem;
    WORD dllCharacteristics;
    DWORD sizeOfStackReserve;
    DWORD sizeOfStackCommit;
    DWORD sizeOfHeapReserve;
    DWORD sizeOfHeapCommit;
    DWORD loaderFlags;
    DWORD numberOfRvaAndSizes;
};

struct DataDirectory {
    DWORD virtualAddress;
    DWORD size;
};

struct SectionHeader {
    char name[8];
    DWORD virtualSize;
    DWORD virtualAddress;
    DWORD sizeOfRawData;
    DWORD pointerToRawData;
    DWORD pointerToRelocations;
    DWORD pointerToLinenumbers;
    WORD numberOfRelocations;
    WORD numberOfLinenumbers;
    DWORD characteristics;
};

#pragma pack(pop)

// .NET metadata structures (simplified)
struct CLIHeader {
    DWORD headerSize;
    WORD majorRuntimeVersion;
    WORD minorRuntimeVersion;
    DataDirectory metadata;
    DWORD flags;
    DWORD entryPointToken;
    DataDirectory resources;
    DataDirectory strongNameSignature;
    DataDirectory codeManagerTable;
    DataDirectory vtableFixups;
    DataDirectory exportAddressTableJumps;
    DataDirectory managedNativeHeader;
};

struct MetadataHeader {
    DWORD signature;
    WORD majorVersion;
    WORD minorVersion;
    DWORD reserved;
    DWORD versionLength;
    // Variable length version string follows
    // Stream headers follow after version string
};

struct StreamHeader {
    DWORD offset;
    DWORD size;
    char name[12]; // Variable length, null-terminated, padded to 4-byte boundary
};

// Metadata tables
enum MetadataTable : BYTE {
    TABLE_MODULE = 0x00,
    TABLE_TYPEREF = 0x01,
    TABLE_TYPEDEF = 0x02,
    TABLE_FIELD = 0x04,
    TABLE_METHODDEF = 0x06,
    TABLE_PARAM = 0x08,
    TABLE_INTERFACEIMPL = 0x09,
    TABLE_MEMBERREF = 0x0A,
    TABLE_CONSTANT = 0x0B,
    TABLE_CUSTOMATTRIBUTE = 0x0C,
    TABLE_FIELDMARSHAL = 0x0D,
    TABLE_DECLSECURITY = 0x0E,
    TABLE_CLASSLAYOUT = 0x0F,
    TABLE_FIELDLAYOUT = 0x10,
    TABLE_STANDALONESIG = 0x11,
    TABLE_EVENTMAP = 0x12,
    TABLE_EVENT = 0x14,
    TABLE_PROPERTYMAP = 0x15,
    TABLE_PROPERTY = 0x17,
    TABLE_METHODSEMANTICS = 0x18,
    TABLE_METHODIMPL = 0x19,
    TABLE_MODULEREF = 0x1A,
    TABLE_TYPESPEC = 0x1B,
    TABLE_IMPLMAP = 0x1C,
    TABLE_FIELDRVA = 0x1D,
    TABLE_ASSEMBLY = 0x20,
    TABLE_ASSEMBLYPROCESSOR = 0x21,
    TABLE_ASSEMBLYOS = 0x22,
    TABLE_ASSEMBLYREF = 0x23,
    TABLE_ASSEMBLYREFPROCESSOR = 0x24,
    TABLE_ASSEMBLYREFOS = 0x25,
    TABLE_FILE = 0x26,
    TABLE_EXPORTEDTYPE = 0x27,
    TABLE_MANIFESTRESOURCE = 0x28,
    TABLE_NESTEDCLASS = 0x29,
    TABLE_GENERICPARAM = 0x2A,
    TABLE_METHODSPEC = 0x2B,
    TABLE_GENERICPARAMCONSTRAINT = 0x2C
};

// Assembly information
struct AssemblyInfo {
    std::string name;
    std::string version;
    std::string culture;
    std::vector<BYTE> publicKey;
    DWORD flags;
    
    std::string GetFullName() const {
        std::string fullName = name;
        if (!version.empty()) {
            fullName += ", Version=" + version;
        }
        if (!culture.empty()) {
            fullName += ", Culture=" + culture;
        }
        return fullName;
    }
};

// Loaded assembly representation
class LoadedAssembly {
public:
    LoadedAssembly(const std::wstring& path);
    ~LoadedAssembly();
    
    bool Load();
    void Unload();
    
    // Assembly information
    const AssemblyInfo& GetInfo() const { return m_info; }
    const std::wstring& GetPath() const { return m_path; }
    bool IsLoaded() const { return m_loaded; }
    
    // Type enumeration
    std::vector<std::string> GetTypeNames();
    MethodTable* GetMethodTable(const std::string& typeName);
    
    // Method resolution
    MethodDesc* FindMethod(const std::string& typeName, const std::string& methodName);
    void* GetMethodIL(MethodDesc* method);
    
    // Metadata access
    const void* GetMetadataRoot() const { return m_metadataRoot; }
    size_t GetMetadataSize() const { return m_metadataSize; }
    
private:
    std::wstring m_path;
    AssemblyInfo m_info;
    bool m_loaded;
    
    // File mapping
    HANDLE m_fileHandle;
    HANDLE m_mappingHandle;
    void* m_baseAddress;
    size_t m_fileSize;
    
    // PE structure pointers
    DOSHeader* m_dosHeader;
    PEHeader* m_peHeader;
    OptionalHeader* m_optionalHeader;
    std::vector<SectionHeader*> m_sectionHeaders;
    CLIHeader* m_cliHeader;
    
    // Metadata
    void* m_metadataRoot;
    size_t m_metadataSize;
    std::unordered_map<std::string, StreamHeader*> m_streams;
    
    // Type cache
    std::unordered_map<std::string, MethodTable*> m_typeCache;
    
    // Loading helpers
    bool MapFile();
    void UnmapFile();
    bool ParsePEHeaders();
    bool ParseCLIHeader();
    bool ParseMetadata();
    bool ParseStreams();
    
    // Metadata parsing
    void* RVAToPointer(DWORD rva);
    SectionHeader* FindSection(const char* name);
    SectionHeader* FindSectionByRVA(DWORD rva);
    
    // Type creation
    MethodTable* CreateTypeFromMetadata(const std::string& typeName);
    void PopulateMethodTable(MethodTable* mt, const std::string& typeName);
};

// Main assembly loader class
class AssemblyLoader {
public:
    AssemblyLoader(TypeSystem* typeSystem);
    ~AssemblyLoader();

    // Initialization
    bool Initialize();
    void Shutdown();

    // Overlay configuration
    void RefreshOverlayConfiguration();
    const OverlayConfig& GetOverlayConfiguration() const { return m_overlayConfig; }
    
    // Assembly loading
    bool LoadAssembly(const std::wstring& assemblyPath);
    bool UnloadAssembly(const std::wstring& assemblyPath);
    LoadedAssembly* FindAssembly(const std::wstring& assemblyPath);
    LoadedAssembly* FindAssemblyByName(const std::string& assemblyName);
    
    // Type resolution across assemblies
    MethodTable* ResolveType(const std::string& typeName);
    MethodTable* ResolveType(const std::string& typeName, const std::string& assemblyName);
    
    // Method resolution
    MethodDesc* ResolveMethod(const std::string& typeName, const std::string& methodName);
    void* GetMethodIL(const std::string& typeName, const std::string& methodName);
    
    // Assembly enumeration
    std::vector<LoadedAssembly*> GetLoadedAssemblies();
    std::vector<std::string> GetLoadedTypeNames();

    // Security and validation
    bool ValidateAssembly(const std::wstring& assemblyPath);
    bool IsAssemblyLoaded(const std::wstring& assemblyPath);

private:
    TypeSystem* m_typeSystem;
    bool m_initialized;

    // Loaded assemblies
    std::unordered_map<std::wstring, std::unique_ptr<LoadedAssembly>> m_assemblies;
    std::unordered_map<std::string, LoadedAssembly*> m_assembliesByName;

    // Security and validation
    CRITICAL_SECTION m_loaderLock;

    // Overlay support
    OverlayConfig m_overlayConfig;

    bool LoadAssemblyInternal(const std::wstring& assemblyPath);
    bool TryEnsureOverlayAssemblyForType(const std::string& typeName);
    bool TryEnsureAssemblyByName(const std::string& assemblyName);
    bool LoadAssemblyFromSearchPaths(const std::string& assemblyName);
    void LoadOverlayConfiguration();

    // Helper methods
    std::string ExtractAssemblyName(const std::wstring& path);
    bool IsValidPEFile(const std::wstring& path);
    void UpdateTypeSystem(LoadedAssembly* assembly);
};

// Global assembly loader instance
extern AssemblyLoader* g_assemblyLoader;

// Helper functions for IL bytecode parsing
class ILParser {
public:
    static bool IsValidIL(const void* ilCode, size_t size);
    static std::vector<BYTE> ParseMethodIL(const void* ilCode, size_t size);
    static size_t GetMethodILSize(const void* ilCode);
};

// Assembly metadata helper functions
DWORD TokenFromRID(DWORD rid, MetadataTable table);
DWORD RIDFromToken(DWORD token);
MetadataTable TableFromToken(DWORD token);
std::string ReadString(const void* stringHeap, DWORD offset);
std::vector<BYTE> ReadBlob(const void* blobHeap, DWORD offset);

} // namespace Phase1
} // namespace CLRNet

#endif // ASSEMBLY_LOADER_H
