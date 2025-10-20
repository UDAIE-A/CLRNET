# CLRNet

CLRNet is now a tiny, cross-platform script runtime designed to be easy to
build and simple to use. Instead of dozens of platform-specific components, the
project ships a single `clrnet` CLI that understands a lightweight scripting
language with commands such as `set`, `print`, and `sleep`.

## Features

* **One-step build** – standard CMake project that targets any modern C++20
  compiler.
* **Human-friendly scripts** – author automation in plain text, no manifest or
  MSBuild knowledge required.
* **Explainable runs** – use `clrnet explain` to inspect what will happen before
  executing a script.
* **Safe dry runs** – execute scripts without waiting for `sleep` commands to
  finish.

## Getting started

1. Configure and build the CLI:
   ```bash
   cmake -S . -B out/build
   cmake --build out/build
   ```
2. Execute the bundled example script:
   ```bash
   ./out/build/clrnet run examples/scripts/hello.clr
   ```
3. Preview what a script will do:
   ```bash
   ./out/build/clrnet explain examples/scripts/hello.clr
   ```
4. Generate a new starter script:
   ```bash
   ./out/build/clrnet init my-first-script.clr
   ```

The example script demonstrates all supported commands and variable
interpolation. Run with `--dry-run` to skip actual pauses.

## Command reference

`clrnet` exposes three entry points:

| Command | Description |
| --- | --- |
| `run <script> [--dry-run] [--quiet] [--no-banner]` | Execute the specified script. |
| `explain <script>` | Print a human-readable summary of metadata and commands. |
| `init <path>` | Create a sample script in the provided location. |

See [docs/SCRIPT_REFERENCE.md](docs/SCRIPT_REFERENCE.md) for the full language
reference.

## Writing scripts

Scripts are plain-text files. Lines beginning with `@` set metadata and lines
starting with `#` are comments. The runtime exposes metadata and variables using
`${name}` placeholders that are expanded at execution time.

A minimal script:

```
@name Hello
set greeting Hello CLRNet!
print ${greeting}
sleep 200
print Goodbye.
```

## Testing

The project enables CTest by default. After building run:

```bash
cd out/build
ctest
```

A single regression test verifies that the bundled example parses and executes
in dry-run mode.

## Legacy materials

Historical documents and Windows Phone–specific notes remain in the repository
under `docs/`, `research/`, and other folders for archival purposes. They no
longer reflect the current implementation but have been kept for reference.
