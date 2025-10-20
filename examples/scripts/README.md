# Script Examples

The files in this folder are small sample programs for the new CLRNet script
runtime. Each file is a plain-text script made up of simple commands such as
`set`, `print`, and `sleep`.

Run an example from the repository root:

```bash
cmake -S . -B out/build
cmake --build out/build
./out/build/clrnet run examples/scripts/hello.clr
```

Use `clrnet explain` to preview what a script will do before executing it:

```bash
./out/build/clrnet explain examples/scripts/hello.clr
```
