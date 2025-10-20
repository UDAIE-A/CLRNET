# CLRNet Quick Start Guide

Follow these steps to build the CLI and run your first script:

1. Configure the project with CMake:
   ```bash
   cmake -S . -B out/build
   ```
2. Build the executable:
   ```bash
   cmake --build out/build
   ```
3. Run the bundled example in dry-run mode (skips actual delays):
   ```bash
   ./out/build/clrnet run examples/scripts/hello.clr --dry-run
   ```
4. Explain the script to see each step:
   ```bash
   ./out/build/clrnet explain examples/scripts/hello.clr
   ```
5. Generate a starter script:
   ```bash
   ./out/build/clrnet init my-script-directory
   ```

For more details about the script language see
[docs/SCRIPT_REFERENCE.md](docs/SCRIPT_REFERENCE.md).
