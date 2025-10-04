# Build Configuration for CLRNET

## Overview
This directory contains build scripts and configuration for cross-compiling the CLRNET runtime to target Windows Phone 8.1 ARM architecture.

## Build Requirements

### Development Environment
- **Host OS:** Windows 10/11
- **Visual Studio:** 2015 or later with ARM support
- **Windows Phone SDK:** 8.1 SDK installed
- **Build Tools:** MSBuild, CMake (optional)

### Target Platform
- **Architecture:** ARM32 (ARMv7)
- **OS:** Windows Phone 8.1
- **Runtime:** Windows CE 8.0 based

## Build Scripts

### build.ps1 - Main Build Script
PowerShell script that orchestrates the entire build process:
1. Environment validation
2. Dependency checking  
3. Cross-compilation for ARM
4. Package generation
5. Deployment preparation

### cmake/ - CMake Configuration
Alternative build system using CMake for cross-platform development:
- CMakeLists.txt for each component
- ARM toolchain configuration
- Dependency management

### msbuild/ - MSBuild Configuration  
Native Windows build system integration:
- Solution and project files
- ARM platform configuration
- NuGet package management

## Build Process

### Phase 1: Environment Setup
```powershell
# Validate development environment
.\build.ps1 -Target ValidateEnvironment

# Install required dependencies
.\build.ps1 -Target InstallDependencies
```

### Phase 2: Core Runtime Build
```powershell
# Build core CLR components
.\build.ps1 -Target BuildRuntime -Platform ARM

# Build base class library
.\build.ps1 -Target BuildBCL -Platform ARM
```

### Phase 3: Test Applications
```powershell
# Build test applications
.\build.ps1 -Target BuildTests -Platform ARM

# Package for deployment
.\build.ps1 -Target Package -Platform ARM
```

### Phase 4: Deployment
```powershell
# Deploy to device or emulator
.\build.ps1 -Target Deploy -Device "Device Name"
```

## Platform Configuration

### ARM Cross-Compilation Settings
```xml
<PropertyGroup>
  <Platform>ARM</Platform>
  <PlatformToolset>v140</PlatformToolset>
  <WindowsTargetPlatformVersion>8.1</WindowsTargetPlatformVersion>
  <ConfigurationType>DynamicLibrary</ConfigurationType>
</PropertyGroup>
```

### Linker Configuration
- Target Windows CE subsystem
- ARM calling conventions
- Position-independent code
- Compact executable format

## Output Structure
```
build/
├─ output/
│  ├─ arm-release/      # Release builds
│  ├─ arm-debug/        # Debug builds  
│  └─ packages/         # Deployment packages
├─ intermediate/        # Build temporaries
├─ logs/               # Build logs
└─ cache/              # Build cache
```

## Deployment Packages

### Runtime Package (clrnet-runtime.wpx)
- Core CLR implementation
- Base class library
- JIT compiler
- Garbage collector
- Platform abstraction layer

### Test Package (clrnet-tests.wpx)  
- Test applications
- Validation utilities
- Performance benchmarks
- Sample code

### Developer Package (clrnet-devtools.wpx)
- Debugging utilities
- Profiling tools
- Deployment helpers
- Documentation

## Build Validation

### Automated Tests
- Compilation verification
- Link-time validation  
- Package integrity checks
- Basic functionality tests

### Manual Verification
- Deploy to device/emulator
- Execute test applications
- Verify runtime behavior
- Performance validation