# Build Configuration for CLRNET

## Overview
The `build/` directory contains the tooling we use to assemble CLRNET for Windows Phone 8.1.  It now drives three parallel outputs:

1. **Native runtime binaries** (core engine, host, interop, system glue, and test harness) under `build/bin/`.
2. **Modern overlay facades** that expose the Track A API surface, staged under `build/output/<platform>-<configuration>/CLRNetOverlay`.
3. **Distributable packages** in `build/bin/<platform>/<configuration>/packages/`, combining the native runtime and overlay payload for shipping with applications.

## Prerequisites
- Windows 10/11 with the Windows Phone 8.1 SDK installed
- Visual Studio 2015 or later with ARM toolset support
- PowerShell 5.0+ (used by `build.ps1` and helper scripts)
- MSBuild available on `%PATH%` (the script auto-detects common install paths)

## Directory layout
```
build/
├─ bin/                       # Staged native binaries (per platform/config)
│  └─ ARM/Release/
│     ├─ CLRNetCore.dll
│     ├─ CLRNetHost.exe
│     ├─ CLRNetInterop.dll
│     ├─ CLRNetSystem.dll
│     ├─ CLRNetTests.exe
│     └─ packages/            # Runtime + overlay delivery bundles
├─ output/                    # Non-native assets (overlay facades, caches)
│  └─ ARM-Release/
│     └─ CLRNetOverlay/       # App-local facade bundle with manifest + forwards
├─ logs/                      # Build transcript + diagnostics
├─ scripts/                   # Batch/PowerShell convenience wrappers
├─ build.ps1                  # Main orchestrator (validate, build, package)
└─ BUILD_SYSTEM.md            # High-level description of binary outputs
```

## Key scripts
| Script | When to use it |
| --- | --- |
| `build/build.ps1` | Primary entry point. Supports validation, native build, overlay compilation, packaging, and deployment stubs. |
| `build/scripts/build-all.bat` | One-shot wrapper for Windows command prompts. Calls the PowerShell build and now also generates the overlay bundle automatically. |
| `build/scripts/package-overlay.ps1` | Lightweight helper that just builds and stages the Track A overlay facades (invokes `build.ps1 -Target PackageOverlay`). |
| `build/scripts/build-demo.bat` | Simulated build for environments without the full toolchain; now emits placeholder overlay assets so verification scripts still succeed. |
| `build/scripts/verify-binaries.bat` | Validates native binaries **and** the CLRNet overlay bundle to ensure every facade is present before packaging. |

## Typical workflow
1. **Validate environment**
   ```powershell
   pwsh build/build.ps1 -Target ValidateEnvironment
   ```
2. **Build native runtime + tests and compile overlay facades**
   ```powershell
   pwsh build/build.ps1 -Target Build -Platform ARM -Configuration Release
   ```
   This target now performs four steps in sequence: native runtime compilation, test harness copy, Track A overlay compilation, and overlay packaging.
3. **Generate distributable packages**
   ```powershell
   pwsh build/build.ps1 -Target Package -Platform ARM -Configuration Release
   ```
4. **(Optional) Regenerate overlay bundle only**
   ```powershell
   pwsh build/scripts/package-overlay.ps1 -Platform ARM -Configuration Release
   ```
   Useful after tweaking facade code or the type forward map without rebuilding the native runtime.
5. **Verify outputs**
   ```cmd
   build\scripts\verify-binaries.bat
   ```
   Confirms that binaries, overlay facades, the forward map, and the generated manifest all exist.

## Overlay bundle contents
Running either `build.ps1 -Target Build` or `package-overlay.ps1` produces:
```
build/output/ARM-Release/CLRNetOverlay/
├─ CLRNet.Core.OverlaySupport.dll
├─ overlay.manifest.json
├─ type-forward-map.txt
└─ facades/
   ├─ CLRNet.Facade.System.Runtime.dll
   ├─ CLRNet.Facade.System.ValueTuple.dll
   ├─ CLRNet.Facade.System.Threading.Tasks.Extensions.dll
   ├─ CLRNet.Facade.System.Text.Json.dll
   ├─ CLRNet.Facade.System.Buffers.dll
   ├─ CLRNet.Facade.System.Net.Http.dll
   └─ CLRNet.Facade.System.IO.dll
```
When `build/scripts/build-all.bat` runs, it copies this directory into `build/bin/ARM/Release/packages/CLRNet-Overlay/` so deployment artifacts already include the Track A facades beside the native runtime packages.

## Logs and diagnostics
- `build/logs/build.log` gathers all messages from `build.ps1`, including overlay compilation and packaging status.
- Batch scripts emit progress to the console; failures drop into the `:error` section and preserve the last exit code for CI detection.
- Overlay manifests include a timestamp and configuration to help compare payloads across builds.

## Updating the overlay forward map
The packaging step copies `examples/overlay/type-forward-map.txt` into the bundle.  Adjust that file when you add new facades or forward additional types, then rerun `package-overlay.ps1` so the updated manifest ships with your app.

## Next steps
After a successful build you will find:
- Runtime binaries under `build/bin/ARM/Release/`
- Overlay bundle under `build/output/ARM-Release/CLRNetOverlay/`
- Zipped/structured packages under `build/bin/ARM/Release/packages/`

From there follow the CLRNET Application Integration Playbook to drop the overlay folder into your AppX and sideload the runtime onto Windows Phone devices.
