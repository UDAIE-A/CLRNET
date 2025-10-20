# CLRNet

CLRNet is a Windows Phone 8.1 hosting layer that embeds a lightweight managed
runtime, security boundary, and interop bridge so existing CLR assemblies can run
on Lumia-era hardware. The project focuses on producing production-ready ARM
binaries and a host executable that can be dropped directly into an app package.

## Highlights

- **Phone-ready binaries** – MSBuild definitions target the `v120_wp81`
  toolset and generate ARM Release libraries plus a phone-friendly host.
- **System integration** – the runtime ships with compatibility shims,
  replacement hooks, and interop managers that understand WinRT, P/Invoke,
  and hardware capability gating.
- **Security aware** – the security manager models manifest capabilities,
  sandbox levels, and emergency lockdown flows so packaged apps stay within
  Windows Phone policy constraints.
- **One-step build** – run a single PowerShell script and the repository emits
  libraries, the host executable, smoke tests, and pre-staged deployment
  packages under `build/bin/<platform>/<configuration>/`.

## Quick start

1. **Install prerequisites**
   - Windows 10 or 11
   - Visual Studio 2019/2022 with the *Windows Phone 8.1* toolset
   - Windows Phone 8.1 SDK (`C:\Program Files (x86)\Windows Phone Kits\8.1`)
   - PowerShell 5.0+

2. **Clone the repository and open an elevated Developer Command Prompt**

3. **Build the runtime and host**

   ```powershell
   pwsh .\build-wp81.ps1 -Configuration Release -Platform ARM
   ```

   The script validates the environment, locates MSBuild, invokes the
   aggregated `CLRNet.proj`, and writes a binary log to `build/logs/`.

4. **Package for deployment**

   ```powershell
   pwsh .\build-wp81.ps1 -Target Package -Configuration Release -Platform ARM
   ```

   Packaging stages the runtime, interop, system libraries, host executable, and
   smoke-test harness into `build/bin/ARM/Release/packages/`.

## Outputs

After a successful build you will find:

- `build/bin/ARM/<Config>/CLRNetCore.dll` – the execution engine
- `build/bin/ARM/<Config>/CLRNetInterop.dll` – WinRT/PInvoke bridge
- `build/bin/ARM/<Config>/CLRNetSystem.dll` – compatibility and hook surface
- `build/bin/ARM/<Config>/CLRNetHost.exe` – phone host executable
- `build/bin/ARM/<Config>/CLRNetTests.exe` – smoke test harness
- `build/bin/ARM/<Config>/packages/` – runtime, interop, system, host, and
  combined deployment bundles

## Manual MSBuild usage

If you prefer direct MSBuild invocations you can target the umbrella project:

```powershell
msbuild CLRNet.proj /t:Build /p:Configuration=Release /p:Platform=ARM /m
msbuild CLRNet.proj /t:Package /p:Configuration=Release /p:Platform=ARM
```

## Tests

`CLRNetTests.exe` exercises the runtime bootstrap, security shim creation, and
interop manager initialization. Launch it on a Windows Phone emulator or device
after copying the runtime binaries into your AppX payload.

## Additional documentation

- `build/README.md` – deep dive into the build system and packaging layout
- `WP81_*` reports – historical design notes, compatibility matrices, and
  validation playbooks for Windows Phone 8.1 deployments
