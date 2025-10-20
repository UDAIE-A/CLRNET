# Quick start

This guide walks through building the Windows Phone 8.1 runtime and host from a
fresh clone.

## 1. Install prerequisites

- Windows 10 or 11
- Visual Studio 2019/2022 with the *Windows Phone 8.1* components
- Windows Phone 8.1 SDK (`C:\Program Files (x86)\Windows Phone Kits\8.1`)
- PowerShell 5.0+

## 2. Build everything with a single command

```powershell
pwsh .\build-wp81.ps1 -Configuration Release -Platform ARM
```

The script validates the SDK, locates MSBuild, and drives `CLRNet.proj` to build
all native libraries, the phone host, and the smoke-test executable. Logs are
written to `build/logs/` for troubleshooting.

## 3. Package binaries

```powershell
pwsh .\build-wp81.ps1 -Target Package -Configuration Release -Platform ARM
```

Packaging copies the runtime, interop, system, host, and test artifacts into the
`build/bin/ARM/Release/packages/` folder ready to be merged into an AppX payload.

## 4. Inspect outputs

- `build/bin/ARM/<Config>/CLRNetCore.dll`
- `build/bin/ARM/<Config>/CLRNetInterop.dll`
- `build/bin/ARM/<Config>/CLRNetSystem.dll`
- `build/bin/ARM/<Config>/CLRNetHost.exe`
- `build/bin/ARM/<Config>/CLRNetTests.exe`

## 5. Run the smoke tests

Copy the runtime folder to a Windows Phone 8.1 device or emulator and launch
`CLRNetTests.exe`. The test harness verifies that the execution engine, security
layer, and interop managers all initialize correctly in the phone environment.

## 6. Integrate into your AppX project

1. Copy the binaries from `build/bin/ARM/Release/` into your app's `CLRNet/`
   directory.
2. Include the host executable in the AppX manifest and add the required phone
   capabilities.
3. Bundle the `packages/CLRNet-Complete/` contents alongside your managed
   assemblies before creating the final XAP/AppX package.

For a deeper dive into deployment practices review `build/README.md` and the
`WP81_*` summaries in the repository root.
