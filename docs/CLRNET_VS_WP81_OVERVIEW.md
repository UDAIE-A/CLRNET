# CLRNET vs. Stock Windows Phone 8.1 Runtime

This note captures the high-level differences between CLRNET and the default Windows Phone 8.1 runtime so the team can quickly explain the value proposition to non-technical stakeholders.

## Simple Story
- **Old runtime** – Think of the phone as a toy box that never changes. You cannot add new toys, swap parts, or invent new moves at runtime.
- **CLRNET** – Adds a backpack of new toys that still fit the old toy box: modern APIs, safer HTTP handlers, WinRT-aware IO helpers, and more.
- **Userspace IL VM** – A pretend machine that lets the app perform new tricks (Expression.Compile, lightweight code generation) even when the old runtime would refuse.
- **Modern C# toolchain** – A new instruction manual that teaches the compiler the latest language words and directs exotic scenarios to the IL VM.

## Capability Coverage Snapshot
The table below compares how much of the modern capability set is satisfied by the default phone runtime versus the upgraded CLRNET stack.

| Capability bucket | Features considered | WP 8.1 coverage | CLRNET coverage | Notes |
| --- | --- | --- | --- | --- |
| Modern API surface (Track A) | Overlay facades: System.Runtime, System.ValueTuple, System.Threading.Tasks.Extensions, System.Buffers, System.Text.Json, System.Net.Http, System.IO helpers | ~15% | 100% | Stock WP 8.1 ships only fragments of the classic System.Runtime; CLRNET overlays provide the full modern set with forwarding to CLRNet.Core implementations where required. |
| Dynamic code execution (Track B) | IL interpreter, bytecode compiler, cache, host callbacks, sandboxing | 0% | 100% | Windows Phone blocks runtime code generation; CLRNET’s VM enables Expression.Compile, DynamicMethod, and similar patterns by running them in-process. |
| Modern C# pipeline (Track C) | Latest Roslyn toolchain, IL post-pass, expression rerouting, green/yellow feature list | ~25% | ~90% | The default toolchain is frozen at C# 5-era features; CLRNET enables almost all modern constructs and routes unsupported ones to the VM. |

**Overall uplift:** Averaging the buckets, the stock runtime achieves roughly 13% of the desired capability set, while CLRNET delivers about 97%, yielding an estimated 7.5× improvement.

## Packaging Guidance
- Ship the full `CLRNetOverlay` directory (facade DLLs + `type-forward-map.txt`) with each app package. Trimming the bundle will break type resolution.
- Use the repository-wide `Directory.Build.props` and set `<CLRNET_AddOverlayFacades>true</CLRNET_AddOverlayFacades>` to automatically reference the overlay assemblies during builds.
- Optionally wrap the overlay directory in a NuGet-style `.nupkg` so build scripts can unpack it into the app package as a single artifact.

## Talking Points for Stakeholders
- “We can now build with the latest C# features even though the phone OS is frozen.”
- “Runtime code generation scenarios are supported via our own IL VM, so libraries that depend on Expression.Compile just work.”
- “Packaging the overlay is a copy-and-go step; no manual DLL chasing each release.”

