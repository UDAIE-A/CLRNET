# CLRNET Application Integration Playbook

This playbook walks through the end-to-end steps for bringing a new or existing Windows Phone 8.1 application onto the full CLRNET stack. It combines the Track A overlays, Track B userspace IL engine, and Track C modern C# pipeline so your future apps can target modern APIs, dynamic IL, and the latest Roslyn language features without sacrificing device stability.

The process is organised into five stages:

1. **Plan your feature set** – decide which Track capabilities are required and confirm the API surface is supported.
2. **Prepare your build environment** – wire Roslyn `/langversion:latest`, overlay references, and the IL post-pass into your solution.
3. **Compile and post-process assemblies** – build your projects, rewrite IL where necessary, and generate bytecode caches for dynamic code.
4. **Package overlay assets** – include the facade DLLs, manifests, and cache files in your AppX payload.
5. **Verify on-device** – confirm overlay probing, VM execution, and modern feature usage before shipping.

Each stage below calls out the exact files, scripts, and configuration knobs already in the repo so you can reuse the infrastructure with minimal friction.

## 1. Plan your feature set

1. **Map feature requirements to Tracks.**
   * Use the Track A [feature table](TRACK-A-APP-LOCAL-BCL-OVERLAY.md) to confirm the modern APIs your app expects are covered by the shipped facades (ValueTask, JSON, HTTP, WinRT IO, etc.).
   * Cross-check dynamic scenarios against the Track B VM workflow in [TRACK-B-USERSPACE-IL-ENGINE.md](TRACK-B-USERSPACE-IL-ENGINE.md) to ensure Expression trees, DynamicMethod usage, or scripting engines can be routed through the VM.
   * Review the Track C [feature matrix](TRACK-C-MODERN-CS-PIPELINE.md) to verify that the language constructs you plan to use are in the “Green” or “Yellow” tiers.
2. **Identify no-go areas early.** Anything listed in the Track C “No-go” column (Span fast paths, default interface methods, hardware intrinsics) still requires architectural workarounds. Plan mitigations before implementation begins.
3. **Decide on sandbox policies.** For plugins or untrusted code, determine time/memory budgets and namespace permissions that will be enforced through the VM execution context.

Documenting these decisions in your app repo up front keeps the later build and packaging steps deterministic.

## 2. Prepare your build environment

1. **Adopt the repository `Directory.Build.props`.**
   * Copy or import the root [Directory.Build.props](../Directory.Build.props) into your solution so every project compiles with Roslyn `/langversion:latest`, nullable analysis, and consistent warning levels.
   * Set `<CLRNET_AddOverlayFacades>true</CLRNET_AddOverlayFacades>` in project-level `PropertyGroup`s that should automatically reference the `CLRNet.Facade.*` assemblies (mirroring the pattern in [examples/ModernCSharpPipeline/Directory.Build.props](../examples/ModernCSharpPipeline/Directory.Build.props)).
2. **Wire the IL post-pass.**
   * Add an `AfterBuild` target that runs `dotnet run --project tools/modern-pipeline/CLRNet.ModernCSharpPipeline/CLRNet.ModernCSharpPipeline.csproj -- --input "$(TargetPath)"` with any `--embed-json` or `--strip-attribute` options required by your project. See the sample target in [examples/ModernCSharpPipeline/Directory.Build.props](../examples/ModernCSharpPipeline/Directory.Build.props) for a drop-in template.
   * For CI, make this target part of the standard build so rewritten assemblies are always produced before packaging.
3. **Reference overlay helpers.** Ensure projects that rely on new APIs reference `CLRNet.Core.OverlaySupport`. The IL post-pass automatically injects the facade references, but direct project references help with IntelliSense during development.
4. **Check in configuration.** Store the customised props/targets alongside your solution so every developer and build agent picks up the same pipeline without manual steps.

## 3. Compile and post-process assemblies

1. **Build your projects.** Use `dotnet build` (or MSBuild in Visual Studio) with the props above so the modern Roslyn toolset generates IL that targets CLRNET’s facades.
2. **Review IL post-pass output.** Run the pipeline with `--verbose` initially to confirm redirects and attribute stripping match expectations (details in [TRACK-C-MODERN-CS-PIPELINE.md](TRACK-C-MODERN-CS-PIPELINE.md)).
3. **Generate VM bytecode caches.**
   * For dynamic IL producers (Expression trees, scripting), capture the IL stream and call `CLRNet_VM_CompileIL` during your app’s initialisation phase. This seeds the cache under `LocalCache/VmBytecode` (see [TRACK-B-USERSPACE-IL-ENGINE.md](TRACK-B-USERSPACE-IL-ENGINE.md)).
   * Persist cache hits by shipping the generated files with your app or prepopulating them on first run to minimise warm-up costs.
4. **Automate validation.** Add smoke tests that execute representative expressions through `CLRNetExpressionCompiler` so regressions in the post-pass or VM are caught during CI.

## 4. Package overlay assets

1. **Lay out the overlay directory.** Follow the folder conventions in the [App-Local Facade Overlay Integration Guide](APP_LOCAL_OVERLAY_GUIDE.md): create `CLRNetOverlay\facades\` and drop the required `CLRNet.Facade.*.dll` files alongside `type-forward-map.txt`.
2. **Include support assemblies.** Ship `CLRNet.Core.OverlaySupport.dll`, any other managed helpers you depend on, and the VM cache directory if pre-built.
3. **Ship manifests and configuration.**
   * Maintain an up-to-date `type-forward-map.txt` describing every forwarded type (samples live in [examples/overlay/type-forward-map.txt](../examples/overlay/type-forward-map.txt)).
   * If you override probing behaviour, package a manifest or config file that sets `CLRNET_OVERLAY_ENABLE`, `CLRNET_OVERLAY_PATHS`, or `CLRNET_OVERLAY_MANIFEST` as described in [APP_LOCAL_OVERLAY_GUIDE.md](APP_LOCAL_OVERLAY_GUIDE.md).
4. **Bundle post-pass resources.** Ensure any JSON or other resources embedded via the IL post-pass are present in the output AppX so runtime lookups succeed.

## 5. Verify on-device

1. **Overlay probing.** Launch the app and inspect the overlay loader logs (via `OverlayConfig` diagnostics) to confirm facades are loaded from the app package before system facades (see `src/phase1-userland/core/OverlayConfig.cpp`).
2. **VM execution.** Exercise scenarios that call `Expression.Compile()` or other dynamic features and verify they execute through the VM (watch for cache hits under `LocalCache/VmBytecode`).
3. **Modern API usage.** Run through ValueTask-based flows, JSON serialisation, WinRT IO helpers, and HTTP calls to ensure they route through the overlay implementations without tapping forbidden OS APIs.
4. **Sandbox policies.** Validate time/memory budgets and namespace restrictions by running trusted and untrusted payloads under the VM and observing enforcement.
5. **Regression guardrails.** Run `.\scripts\simple-check.ps1` and any app-specific smoke tests before packaging release builds.

## Rollout checklist

Before shipping, confirm the following items are complete:

- [ ] Track selection documented with feature matrix references.
- [ ] Repository `Directory.Build.props` (or equivalent) imported and committed.
- [ ] IL post-pass target enabled in CI and local builds.
- [ ] VM bytecode cache generation (or warm-up) in place for dynamic code paths.
- [ ] Overlay directory populated with required facades, manifests, and helper assemblies.
- [ ] On-device verification logs captured for overlay probing and VM execution.
- [ ] App-specific tests updated to cover modern API usage and sandbox scenarios.

Keeping this checklist in your project wiki or README ensures future app teams follow the same proven path when adopting CLRNET.
