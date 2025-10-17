# Track C — Modern C# pipeline (builder magic)

Track C rounds out the CLRNET experience by wiring the build system, post-processing tools, and documentation required to target
modern C# features while still running on Windows Phone 8.1. The pipeline layers on top of Track A (app-local overlays) and
Track B (userspace IL engine) so developers can author code with the latest Roslyn and have it execute against CLRNET's
implementations.

## Components

| Component | Purpose |
|-----------|---------|
| `Directory.Build.props` | Globally enables `/langversion:latest`, pulls in the Microsoft.Net.Compilers toolset, and exposes a switch (`CLRNET_AddOverlayFacades`) for projects that want the Track A facade references automatically. |
| `CLRNet.ModernCSharpPipeline` | Mono.Cecil powered IL post-processor that retargets APIs to overlays, strips unsupported metadata, embeds JSON configuration stubs, and rewrites `Expression.Compile()` to CLRNET's VM bridge. |
| `CLRNetExpressionCompiler` / `VmExpressionCompiler` | Managed glue inside `CLRNet.Core.OverlaySupport` that feeds simple `Expression` trees to the userspace IL VM while falling back to Roslyn's interpreter for complex constructs. |
| `examples/ModernCSharpPipeline` | Sample project that consumes the pipeline end-to-end with modern language features, JSON serialization, ValueTask, and the VM-backed expression hook. |

## Workflow

1. **Compile with latest Roslyn.** Import the repository `Directory.Build.props` (or set `CLRNET_AddOverlayFacades=true` in your project) to force `/langversion:latest` and reference CLRNET's facades instead of the phone OS facades.
2. **Run the IL post-pass.** Call `dotnet run --project tools/modern-pipeline/CLRNet.ModernCSharpPipeline/...` with `--input`, optional `--embed-json`, and `--strip-attribute` options. The sample in `examples/ModernCSharpPipeline` wires this into `AfterBuild` automatically.
3. **Deploy overlays + binaries.** Ship the rewritten assembly, CLRNET overlay assemblies, and any embedded JSON resources in your app package. The loader will resolve CLRNET facades first because of Track A.
4. **Enjoy VM fallback.** `Expression.Compile()` and `Expression<T>.Compile()` automatically route to `CLRNetExpressionCompiler`, which attempts to lower simple arithmetic expressions into the Track B VM. Unsupported expressions continue to work through Roslyn's interpreter.

## IL post-processor switches

```
Usage: clrnet-modern-pipeline --input Foo.dll [--output Foo.Rewritten.dll]
                              [--embed-json LogicalName=path\to.json]
                              [--strip-attribute Namespace.TypeAttribute]
                              [--verbose]
```

* `--input` (required): path to the assembly to rewrite.
* `--output`: optional destination. If omitted, the input file is overwritten.
* `--embed-json`: embed JSON content under the given manifest resource name. Accepts `name=path` or `name:path`.
* `--strip-attribute`: remove attributes the WP8.1 runtime cannot understand. Defaults include SkipLocalsInit, RequiredMember, and other modern metadata.
* `--verbose`: prints every redirect, attribute removal, and resource embed.

## Feature matrix

| Tier | Details |
|------|---------|
| **Green** | Roslyn latest syntax (records, pattern matching, switch expressions, init-only setters, target-typed `new`, nullable context), async/await, tuples, ValueTask plumbing, JSON serialization via `System.Text.Json` overlay, ValueTuple/ArrayPool helpers, safe HTTP handler. |
| **Yellow** | Index/Range helpers, extended JSON scenarios, host-provided HTTP handling nuances, larger async graphs that rely on CLRNET's ValueTask polyfill. |
| **Red** | Expression trees that capture method calls, dynamic call-site binders, Reflection.Emit heavy IL — rerouted to the Track B VM when possible, otherwise falling back to Roslyn's interpreter. |
| **No-go** | Span fast paths, default interface methods, hardware intrinsics, and other features that require runtime support missing from WP8.1/CLRNET today. |

## Example project

`examples/ModernCSharpPipeline` demonstrates the full stack:

1. Uses records, ValueTask, async `Main`, and pattern matching in the same project.
2. Serialises a record to JSON via `System.Text.Json` overlay types.
3. Calls `Expression.Compile()` which the post-processor rewrites to `CLRNetExpressionCompiler`, delivering VM-backed execution for simple arithmetic.
4. Embeds `appsettings.json` via the post-pass so resources travel with the assembly.

To build the sample on a modern desktop:

```
dotnet build examples/ModernCSharpPipeline/ModernPipelineApp.csproj
```

The `AfterBuild` target will automatically invoke the IL post-pass and embed the JSON stub. For Windows Phone packaging, copy the resulting binaries plus the overlay assemblies into your app package alongside the Track A facade outputs.

## Integration notes

* Set `CLRNET_AddOverlayFacades=true` for projects that should default to CLRNET facades; leave it `false` for the overlay projects themselves to avoid circular references.
* The VM bridge currently supports integer/boolean expression trees (no method calls or heap allocations). Complex lambdas fall back to Roslyn's interpreter without breaking existing behaviour.
* Use `--strip-attribute` to remove modern metadata that WP8.1 refuses to load. The tool defaults cover the most common offenders but you can extend the list per project.
* Embed JSON resources via `--embed-json` so WinRT storage helpers can locate configuration at runtime even when the phone refuses to ship loose files.
* Combine Track A, Track B, and Track C documents when onboarding app teams—Track C assumes overlay assemblies and the VM exports are already deployed.
