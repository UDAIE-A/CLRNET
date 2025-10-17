# CLRNET

**Modern .NET tooling for Windows Phone 8.1 — without touching the sealed Microsoft CLR.** CLRNET ships a drop-in runtime, overlay libraries, and build tooling so you can write with the latest C#, run modern libraries, and keep legacy devices productive.

---

## 📌 TL;DR
* Ship an app-local "mini .NET" (`CLRNetOverlay/`) so your apps resolve modern APIs like `System.Text.Json`, `ValueTask`, async streams, and hardened HTTP helpers on Windows Phone 8.1.
* Execute dynamic code safely with the userspace IL virtual machine that handles `Expression.Compile`, `DynamicMethod`, and other scenarios the stock runtime blocks.
* Build with Roslyn `/langversion:latest`, automatic facade references, and a post-build rewrite pipeline that targets CLRNET-friendly APIs.
* Follow the integration playbook to package, deploy, and validate everything in minutes.

---

## 📚 Table of Contents
1. [Why CLRNET?](#why-clrnet)
2. [What’s Included](#whats-included)
3. [Quick Start](#quick-start)
4. [Pick Your Integration Track](#pick-your-integration-track)
5. [Packaging & Distribution](#packaging--distribution)
6. [Developing with Modern C#](#developing-with-modern-c)
7. [Runtime Architecture](#runtime-architecture)
8. [Diagnostics & Verification](#diagnostics--verification)
9. [Further Reading](#further-reading)
10. [License](#license)

---

## Why CLRNET?
Windows Phone 8.1 apps are locked to an outdated CoreCLR snapshot. CLRNET swaps in a fully managed runtime stack that:
* **Unlocks modern APIs** without modifying the OS by resolving CLRNET facades before system binaries.
* **Restores runtime code generation** through a sandboxed IL VM, keeping dynamic libraries and expression trees working.
* **Brings the latest C# language features** to legacy devices by wiring Roslyn, overlay references, and IL rewriting together.
* **Preserves safety** with rollback paths, sandboxing, and app-local deployment so you can test incrementally.

If you need to keep WP 8.1 hardware in the field or ship new apps to it, CLRNET is the toolkit that makes it feel like a modern .NET target again.

---

## What’s Included
| Area | Highlights | Docs |
| --- | --- | --- |
| **Track A – App-local overlay** | Facade assemblies for `System.Runtime`, `System.ValueTuple`, `System.Threading.Tasks.Extensions`, `System.Buffers`, `System.Text.Json`, `System.Net.Http`, `System.IO` helpers, plus implementations for ValueTask, async iterators, JSON primitives, buffer pooling, WinRT IO, and a secure HTTP handler. | [Track A Roadmap](docs/TRACK-A-APP-LOCAL-BCL-OVERLAY.md), [Overlay Guide](docs/APP_LOCAL_OVERLAY_GUIDE.md) |
| **Track B – Userspace IL VM** | IL interpreter, bytecode compiler, SHA-1 cache, host syscall surface, sandboxing, and exports that redirect `Expression.Compile`/`DynamicMethod`. | [Track B Roadmap](docs/TRACK-B-USERSPACE-IL-ENGINE.md) |
| **Track C – Modern C# pipeline** | Repository-wide Roslyn configuration, Mono.Cecil post-pass, expression rerouting, feature matrix (green/yellow/red), and a working sample app. | [Track C Roadmap](docs/TRACK-C-MODERN-CS-PIPELINE.md), [Modern Pipeline Sample](examples/ModernCSharpPipeline/) |
| **Integration playbook** | End-to-end checklist for configuring builds, packaging overlays, priming the VM cache, deploying, and verifying on-device. | [Application Integration Playbook](docs/CLRNET_APP_INTEGRATION_PLAYBOOK.md) |
| **Stakeholder overview** | Plain-language comparison and capability coverage numbers vs. stock WP 8.1. | [CLRNET vs. WP 8.1 Overview](docs/CLRNET_VS_WP81_OVERVIEW.md) |

---

## Quick Start
1. **Clone & restore tooling**
   ```powershell
   git clone https://github.com/your-org/CLRNET.git
   cd CLRNET
   ```
2. **Confirm runtime health**
   ```powershell
   .\scripts\simple-check.ps1
   ```
   Expect `Runtime is FULLY OPERATIONAL!` with all phases green.
3. **Enable overlay references** in your app project by importing the repo’s `Directory.Build.props` and setting:
   ```xml
   <PropertyGroup>
     <CLRNET_AddOverlayFacades>true</CLRNET_AddOverlayFacades>
   </PropertyGroup>
   ```
4. **Package the overlay** by copying the prepared `examples/overlay/` payload (facades, manifests, support assemblies) into your AppX under `CLRNetOverlay/`.
5. **Build with the modern pipeline** using the Roslyn toolchain or the `ModernPipelineApp` sample as a template.
6. **Deploy to device/emulator** with your usual WP 8.1 deployment tooling.
7. **Verify on-device** by running your app and checking overlay + VM diagnostics (see below).

---

## Pick Your Integration Track
You can adopt the tracks independently or all together:

### 🟢 Track A – Ship modern APIs alongside your app
* Drop the `CLRNetOverlay/` folder beside your binaries.
* Ensure your package includes `type-forward-map.txt` so the loader knows which assembly owns each forwarded type.
* Follow the [Overlay Guide](docs/APP_LOCAL_OVERLAY_GUIDE.md) for manifest samples, WinRT storage helpers, and automation tips.

### 🟡 Track B – Enable runtime code generation
* Add references to the `CLRNet.Core.OverlaySupport` VM APIs or call the native exports documented in the Track B roadmap.
* Cache compiled bytecode under `LocalCache/VmBytecode` (built-in helpers handle hashing).
* Use the sandbox knobs (time, memory, namespace allow-list) to keep plugins constrained.

### 🔵 Track C – Compile with the latest C#
* Consume the provided `Directory.Build.props` to set `/langversion:latest` and nullable context.
* Wire the Mono.Cecil post-pass (see `tools/modern-pipeline/`) into your build’s `AfterBuild` target.
* Rely on the green/yellow/red table to decide when to lean on overlays vs. the IL VM.

---

## Packaging & Distribution
* **Bundle once, reuse everywhere.** Treat `CLRNetOverlay/` as a reusable payload checked into source control or published as a `.nupkg`. Build scripts can extract it during packaging so you don’t copy files manually.
* **Documented surface area.** Keep the overlay manifest in sync with the API set you ship so the loader doesn’t expose types you don’t implement.
* **VM warm start.** Populate `LocalCache/VmBytecode` with precompiled bytecode if your app relies heavily on dynamic features.
* **Configuration overrides.** The `OverlayConfig` subsystem honors environment overrides and package manifests so enterprise deployments can retarget overlay paths without rebuilding the app.

---

## Developing with Modern C#
* **Repository-wide props.** Import `Directory.Build.props` to inherit Roslyn tooling, nullable analysis, and facade references.
* **Sample project.** Explore `examples/ModernCSharpPipeline/` for a reference implementation showcasing records, pattern matching, ValueTask, async streams, JSON serialization, and VM-backed expression trees.
* **IL rewrites.** The `CLRNet.ModernCSharpPipeline` tool (under `tools/modern-pipeline/`) rewrites API calls that need to route through CLRNET implementations and strips problematic attributes before packaging.
* **Fallback guidance.** Use the Track C feature matrix to know when to expect native support, overlay polyfills, or VM execution.

---

## Runtime Architecture
CLRNET is delivered in three completed phases:

| Phase | Goal | Status | Source |
| --- | --- | --- | --- |
| **Phase 1 – Userland runtime** | Core execution engine, type system, garbage collector, assembly loader, ARM32 JIT. | ✅ Complete | `src/phase1-userland/`
| **Phase 2 – System interop** | WinRT bridge, P/Invoke, hardware access, security manager. | ✅ Complete | `src/phase2-interop/`
| **Phase 3 – System integration** | CLR replacement engine, deep system hooks, rollback system. | ✅ Complete | `src/phase3-integration/`

Each phase can operate independently so you can keep deployments in userland or opt into system-wide replacement when you’re ready.

---

## Diagnostics & Verification
```powershell
# Quick health check (30 seconds)
.\scripts\simple-check.ps1

# Full regression suite
.\scripts\run-all-tests.ps1

# Inspect detailed status & phase reports
Get-Content docs\RUNTIME_STATUS.md
Get-Content docs\PHASE1-STATUS.md
```

For runtime overlays, enable verbose logging via `OverlayConfig` to confirm which assemblies are resolved from the app package versus the OS.

---

## Further Reading
* [CLRNET Application Integration Playbook](docs/CLRNET_APP_INTEGRATION_PLAYBOOK.md)
* [App-Local Overlay Guide](docs/APP_LOCAL_OVERLAY_GUIDE.md)
* [Userspace IL Engine Roadmap](docs/TRACK-B-USERSPACE-IL-ENGINE.md)
* [Modern C# Pipeline Roadmap](docs/TRACK-C-MODERN-CS-PIPELINE.md)
* [Stakeholder Overview](docs/CLRNET_VS_WP81_OVERVIEW.md)
* [Deployment scripts](scripts/)

---

## License
This project is licensed under the [MIT License](LICENSE).
