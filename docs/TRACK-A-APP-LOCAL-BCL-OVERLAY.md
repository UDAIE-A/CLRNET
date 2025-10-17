# Track A — App-Local BCL Overlay Roadmap

## Objective
Deliver a shippable set of app-local facade assemblies that expose modern .NET APIs while reusing the existing Windows Phone 8.1 base class library (BCL) and CLRNET runtime components. The overlay lets developers compile against newer namespaces (e.g., `System.Runtime`, `System.Text.Json`) and have those calls redirected to CLRNET or stock WP8.1 implementations at runtime without replacing the system BCL.

## Guiding Principles
- **Add, never regress.** Existing working functionality in CLRNET or the WP8.1 BCL must keep working. New overlays sit alongside existing binaries and can be disabled by simply removing the app-local packages.
- **Strict API surface.** Only expose members that have implementations. Each facade carries a manifest describing feature coverage so developers know what is available.
- **Self-contained deployment.** Facades, shims, and helper libraries live inside the consuming app package. No device-wide installation is required.
- **Layered fallback.** Prefer stock WP8.1 APIs when they match the modern contract; fall back to CLRNET.Core or new helper implementations when gaps exist.

## High-Level Architecture
1. **Facade Assemblies (`CLRNet.Facade.*`)**
   - Metadata-only assemblies that declare modern namespaces and forward types.
   - Each facade targets ARM/AnyCPU with `netstandard1.x`-compatible metadata.
   - Example packages: `CLRNet.Facade.System.Runtime.dll`, `CLRNet.Facade.System.Threading.Tasks.Extensions.dll`, `CLRNet.Facade.System.Text.Json.dll` (subset).

2. **Overlay Loader Enhancements**
   - Modify the CLRNET assembly loader to probe the application package for `CLRNet.Facade.*` before falling back to platform facades.
   - Honor `TypeForwardedTo` records during type resolution so forwarded types point to either WP8.1 BCL or CLRNET.Core.

3. **Support Libraries**
   - **CLRNet.Core Augments:** New namespaces or members implemented directly in managed code when WP8.1 lacks equivalents (e.g., `System.ValueTuple`, `System.Buffers` primitives).
   - **Interop Helpers:** Safe HTTP handlers, WinRT-backed I/O adapters, and task primitives that sit behind forwarded APIs.

4. **Binding Policy**
   - `appxmanifest` declares the overlay assemblies as content.
   - Application config (where supported) or runtime initialization informs CLRNET to enable overlay probing.
   - Runtime now scans `CLRNetOverlay/` (and `CLRNetOverlay/facades`) within the package, loading `type-forward-map.txt` to
     discover type/assembly forwards before falling back to system facades.

## Assembly Packaging Plan
| Facade | Purpose | Implementation Notes |
| --- | --- | --- |
| `CLRNet.Facade.System.Runtime` | Expose core types used by modern libraries (reflection, numerics, intrinsics subset). | Forward to WP8.1 `mscorlib` where types exist. Forward to `CLRNet.Core.SystemRuntime` for new primitives (e.g., `System.Runtime.CompilerServices.Unsafe`). |
| `CLRNet.Facade.System.ValueTuple` | Provide `ValueTuple` support for language features. | Implement tuples directly in managed code with existing CLRNET GC/JIT support. |
| `CLRNet.Facade.System.Threading.Tasks.Extensions` | Enable `ValueTask`, `IAsyncEnumerable`, and async streams. | Implement `ValueTask` and async infrastructure in `CLRNet.Core.Async`. Map to WP8.1 Task APIs when possible. |
| `CLRNet.Facade.System.Buffers` | Offer `ArrayPool<T>` and `IBufferWriter<T>`. | Lightweight array pooling implemented with CLRNET GC; use WinRT buffers via interop helpers. |
| `CLRNet.Facade.System.Text.Json` (subset) | Provide modern JSON APIs. | Ship trimmed serializer focused on UTF-8 reader/writer and reflection-based serialization using CLRNET metadata services. |
| `CLRNet.Facade.System.Net.Http` (handlers) | Modern `HttpClient` surface. | Wrap WP8.1 HTTP stack, adding cookie/redirect handling and TLS policy bridging. |
| `CLRNet.Facade.System.IO` (helpers) | Modern file APIs. | Wrap WinRT StorageFile/Folder with .NET-style helpers; ensure capability checks via security manager. |

## Implementation Phases

### Phase 0 — Research & Tooling (1 week)
- Inventory WP8.1 BCL to identify compatible surface areas.
- Build metadata diff tooling to compare against reference assemblies from .NET Standard 2.0.
- Define API coverage manifests (`facade-manifest.json`) for each assembly.

### Phase 1 — Core Facades (3 weeks)
- ✅ Generated `System.Runtime` and `System.ValueTuple` facades with type forwards.
- ✅ Implemented missing primitives in `CLRNet.Core.OverlaySupport` (ValueTuple, Unsafe, ValueTask builders).
- ✅ Extended loader to prioritize app-local overlays.
- Add CI validation ensuring all forwarded types resolve at runtime.

### Phase 2 — Async & Buffers (3 weeks)
- ✅ Delivered `System.Threading.Tasks.Extensions` and `System.Buffers` facades.
- ✅ Implemented `ValueTask`, async iterators, and array pool infrastructure in `CLRNet.Core.OverlaySupport`.
- Build conformance tests mirroring .NET reference tests for the supported subset.

### Phase 3 — Modern I/O & Networking (4 weeks)
- ✅ Added a `System.Text.Json` subset (writer, reader, and serializer) built on WinRT JSON primitives.
- ✅ Wrapped the WP8.1 HTTP stack with a policy-enforced `SafeHttpClientHandler`.
- ✅ Provided `System.IO` helpers backed by WinRT storage APIs with security manager hooks.

### Phase 4 — Packaging & Developer Experience (2 weeks)
- Create NuGet-style nupkg bundles for overlays (consumed by WP8.1 tooling scripts).
- Document integration in `docs/APP_LOCAL_OVERLAY_GUIDE.md` (new doc).
- Ship sample app demonstrating modern API usage.

## Loader & Binding Requirements
- Update assembly resolution order: `app-package/CLRNet.Facade.*` → `app-package/lib/` → system WP8.1 facades → CLRNET fallback.
- Support for `ResolveEvent` hooks to trace missing bindings.
- Cache resolved forwards to minimize startup overhead.
- Provide configuration flag (`OverlayEnable=true`) toggled via app manifest or runtime API.

## Testing Strategy
- **Metadata Validation:** Script compares facade public surface to manifest, ensuring no accidental additions.
- **Runtime Smoke Tests:** Sample app exercises tuples, async streams, buffers, JSON serialization, and HTTP requests.
- **Compatibility Tests:** Ensure existing apps without overlays load unaffected.
- **Performance Monitoring:** Measure startup impact (<100ms overhead) and memory (<5MB per overlay set).

## Risks & Mitigations
- **API Drift:** Use manifests and automated diff checks to keep declared APIs honest.
- **Performance Overhead:** Prefetch commonly used forwards during startup; optionally pre-JIT ValueTuple structs.
- **Security Exposure:** Gate new HTTP/IO helpers behind the existing capability and security policy manager.
- **Maintenance Load:** Centralize shared helpers in `CLRNet.Core` to avoid duplication across overlays.

## Deliverables
- Facade DLLs with accompanying manifests and unit tests.
- Updated loader code and configuration schema.
- Documentation and sample demonstrating the overlay workflow.
- Packaging scripts producing deployable bundles for app inclusion.

## Success Criteria
- Developers can reference modern .NET packages compiled against the provided facades without editing system files.
- Runtime successfully forwards calls to CLRNET or WP8.1 implementations with parity for targeted APIs.
- Overlay removal leaves the original app behavior unchanged.

