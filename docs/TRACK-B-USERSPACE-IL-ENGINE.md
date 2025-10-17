# Track B — Userspace IL Engine ("JIT in a box")

Track B introduces a self-contained IL virtual machine that lets apps keep using dynamic features even when the platform refuses to JIT new code. The VM ingests IL streams, lowers them into a compact bytecode, and interprets the result inside a sandbox that is owned by CLRNET.

## Components

| Component | Purpose |
|-----------|---------|
| `ILVirtualMachine` | Owns the interpreter, host callback table, and bytecode cache. Provides `Compile`, `Execute`, and call-site configuration APIs. |
| `BytecodeCompiler` | Parses MSIL method bodies (tiny and fat headers) and emits VM instructions. Handles arithmetic, loads/stores, branches, calls, boxing, field access, and object creation. |
| `BytecodeCache` | Persists compiled bytecode in `%Executable%/LocalCache/VmBytecode`, keyed by SHA-1 of the IL payload. |
| `VmHostCallbacks` | Lets the host supply timers, HTTP, storage, logging, managed call dispatch, type coercion, and string resolution hooks. |

## Workflow

1. **Compile IL** – `CLRNet_VM_CompileIL` turns raw IL bytes into a `VmProgram` handle, reusing cached bytecode where possible.
2. **Configure call sites** – `CLRNet_VM_ConfigureCallSite` associates each `call`, `callvirt`, or `newobj` instruction with the runtime target and argument arity the host wants the VM to use.
3. **Register host services** – `CLRNet_VM_RegisterHost` installs the callback table that powers syscalls and managed dispatch.
4. **Execute** – `CLRNet_VM_Execute` runs the bytecode under sandbox limits (time, memory, namespace) supplied via `VmExecutionContextNative`.
5. **Cache** – bytecode can be flushed with `ILVirtualMachine::FlushCache` or left to persist across sessions.

## Host callbacks

| Callback | Description |
|----------|-------------|
| `logCallback` | Receives diagnostic messages from the interpreter. |
| `timerCallback` | Allows bytecode to request coarse timers. |
| `httpCallback` | Bridges lightweight HTTP verbs to the host. |
| `storageCallback` | Wraps WinRT/isolated storage for VM consumers. |
| `managedCallCallback` | Executes managed `call`/`callvirt` invocations routed by the VM. |
| `managedCtorCallback` | Materialises `newobj` sites. |
| `managedCallArityCallback` | Reports argument counts when the host cannot provide them eagerly. |
| `fieldLoadCallback` / `fieldStoreCallback` | Surrogates for `ldfld`/`stfld`. |
| `stringLiteralCallback` | Returns interned strings for `ldstr`. |
| `typeCastCallback` | Implements `box`, `unbox.any`, and `castclass`. |

All callbacks run on the caller’s thread. They should be fast, exception-safe, and trust the sandbox metadata passed via `VmExecutionContextNative`.

## Execution context interop

`VmExecutionContextNative` is a blittable bridge for P/Invoke callers. The runtime lifts it into an internal `VmExecutionContext` that uses STL containers, runs the bytecode, and then copies locals/arguments back to the native struct.

```
struct VmExecutionContextNative {
    VmValue* arguments;
    uint32_t argumentCount;
    VmValue* locals;
    uint32_t localCount;
    uint64_t timeBudgetTicks;
    size_t memoryBudgetBytes;
    const wchar_t* sandboxNamespace;
    void* userData;
};
```

`VmExecutionResultNative` mirrors the success flag, step counter, return value, and last failure string.

## Caching

Compiled bytecode lives under `LocalCache/VmBytecode`. Each entry packs:

1. Local/argument counts
2. Instruction stream
3. Call-site table

The cache keeps a weak map in memory and reloads entries on demand. Hosts can purge the cache via `ILVirtualMachine::FlushCache()` or by deleting the directory on disk.

## Sandbox

Each execution enforces:

* **Time budget** – Checked against `GetTickCount64()` on every instruction.
* **Memory budget** – Approximated by the evaluation stack footprint.
* **Namespace** – Passed through `VmExecutionContextNative` for host-side policy (e.g., per-plugin capability gating).

## Next steps

* Layer a tiny IL-to-bytecode AOT pass for warm start performance.
* Extend opcode support (arrays, exceptions, `ldtoken`, etc.).
* Build managed bridges that automatically capture call-site metadata for `Expression.Compile` and `DynamicMethod`.
* Surface diagnostics/telemetry for bytecode cache hit rates and host callback latency.
