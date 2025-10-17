# App-Local Facade Overlay Integration Guide

This guide explains how to enable the Track A overlay pipeline that ships
modern .NET facades with your Windows Phone 8.1 package. The overlay keeps
the in-box BCL intact while letting CLRNET resolve new API surface area from
assemblies that live beside your application binaries.

## Folder layout

Place overlay assets under an `CLRNetOverlay` directory at the root of your
AppX package (next to the executable). CLRNET automatically scans the
following locations in order:

1. `<package-root>\CLRNetOverlay`
2. `<package-root>\CLRNetOverlay\facades`
3. `<package-root>\CLRNet.Facades` (legacy fallback)

All directories discovered during probing are added to the overlay search
path. Assemblies in these directories are preferred over the stock platform
facades whenever a forwarded type or assembly match is requested.

```
MyApp.exe
CLRNetOverlay\
    facades\
        CLRNet.Facade.System.Runtime.dll
        CLRNet.Facade.System.ValueTuple.dll
    type-forward-map.txt
```

## Type forward manifest

Declare the mapping between modern type names and their implementing
assemblies by adding a `type-forward-map.txt` file inside the overlay
directory. Each entry uses the format:

```
<Full.Type.Name>=<Assembly.Simple.Name>
```

Comments (prefixed with `#` or `;`) and blank lines are ignored. Example:

```
# Core language features
System.ValueTuple=CLRNet.Facade.System.ValueTuple
System.Runtime.CompilerServices.Unsafe=CLRNet.Facade.System.Runtime
System.Runtime.CompilerServices.AsyncValueTaskMethodBuilder=CLRNet.Facade.System.Runtime

# Async primitives
System.Threading.Tasks.ValueTask=CLRNet.Facade.System.Threading.Tasks.Extensions
System.Collections.Generic.IAsyncEnumerable=CLRNet.Facade.System.Threading.Tasks.Extensions
System.Collections.Generic.IAsyncEnumerator=CLRNet.Facade.System.Threading.Tasks.Extensions

# Modern APIs
System.Buffers.ArrayPool=CLRNet.Facade.System.Buffers
System.Text.Json.JsonSerializer=CLRNet.Facade.System.Text.Json
System.Net.Http.SafeHttpClientHandler=CLRNet.Facade.System.Net.Http
System.IO.WinRtStorageExtensions=CLRNet.Facade.System.IO
```

## Shipped facade assemblies

The overlay bundle now includes the following facades and backing
implementations:

| Assembly | Key namespaces | Backing implementation |
| --- | --- | --- |
| `CLRNet.Facade.System.Runtime.dll` | `System.Runtime.CompilerServices.*` additions such as `Unsafe` and the ValueTask method builders. | Forwards into `CLRNet.Core.OverlaySupport` runtime helpers. |
| `CLRNet.Facade.System.ValueTuple.dll` | `System.ValueTuple`, `System.ValueTuple<T>`, `System.ValueTuple<T1,T2>` | Lightweight tuple structs implemented in `CLRNet.Core.OverlaySupport`. |
| `CLRNet.Facade.System.Threading.Tasks.Extensions.dll` | `System.Threading.Tasks.ValueTask`, async iterator primitives | Backed by the overlay’s async helpers and compatible with the existing CLRNET scheduler. |
| `CLRNet.Facade.System.Buffers.dll` | `ArrayPool<T>`, `ArrayBufferWriter<T>`, `IBufferWriter<T>` | Pooling implementation tuned for CLRNET’s GC. |
| `CLRNet.Facade.System.Text.Json.dll` | `JsonSerializer`, `Utf8JsonWriter`, `Utf8JsonReader`, `JsonTokenType` | A curated serializer built on top of `DataContractJsonSerializer` and WinRT JSON tooling. |
| `CLRNet.Facade.System.Net.Http.dll` | `SafeHttpClientHandler` | Wraps the WinRT HTTP stack with cookie, redirect, and certificate policies. |
| `CLRNet.Facade.System.IO.dll` | `WinRtStorageExtensions` | WinRT-backed stream helpers that plug into `System.IO`. |

When a managed assembly requests one of the listed types, the loader ensures
that the specified facade assembly is loaded from the overlay search path
before resolving the type.

## Environment overrides

You can override or augment the automatic detection by setting the following
environment variables before launching CLRNET:

| Variable | Purpose |
| --- | --- |
| `CLRNET_OVERLAY_ENABLE` | Explicitly enable (`1`/`true`) or disable (`0`/`false`) overlay probing. |
| `CLRNET_OVERLAY_PATHS` | Semicolon-separated list of additional search paths. |
| `CLRNET_OVERLAY_MANIFEST` | Alternate manifest file path to merge with the package manifest. |

If at least one overlay path is present or a manifest defines forwards,
the loader activates overlay mode even when the package directories are
missing.

## Verifying resolution order

Enable the overlay and run your application. The loader now:

1. Parses the manifest to build a forward map.
2. Loads facade assemblies from the overlay directories on demand when a
   forwarded type or assembly is requested.
3. Falls back to previously loaded overlays and finally to the stock
   Windows Phone 8.1 facades if no overlay match exists.

You can confirm the configuration by logging the overlay state (see
`OverlayConfig` in `src/phase1-userland/core`) or by watching the runtime load
trace.

With this overlay in place you can reference modern NuGet packages that target
`netstandard` profiles while keeping the device’s system components untouched.

