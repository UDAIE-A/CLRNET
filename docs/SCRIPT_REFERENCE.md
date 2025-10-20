# CLRNet Script Reference

CLRNet scripts are plain text files that describe a sequence of lightweight
automation steps. Lines that start with `#` are comments. Metadata entries use
`@key value` syntax and regular commands follow the format `command arguments`.

## Metadata

Metadata entries are optional and stored before execution as initial variables.
They use the `@` prefix. All metadata is available to commands via variable
substitution (`${key}`).

Common metadata keys:

| Key | Purpose |
| --- | ------- |
| `name` | Display name used by the CLI. |
| `description` | Freeform description. |
| `author` | Author or owner of the script. |

The runtime automatically sets `script.path`, `script.directory`, and
`script.name`.

## Commands

### `set <name> <value>`
Stores a value in the runtime state. The value may reference existing
variables using `${variable}` syntax.

### `append <name> <value>`
Appends text to an existing variable. The runtime automatically inserts a
newline between the existing content and the appended text when the variable is
not empty.

### `print <message>`
Writes a message to standard output. Variables inside `${...}` are resolved
before printing.

### `sleep <milliseconds>`
Pauses execution for the specified duration. When the CLI is invoked with
`--dry-run` the runtime skips the pause but records it in the execution log.

### `fail <message>`
Stops execution immediately and marks the run as failed. The message is written
to the error stream.

## Variable substitution

Any `${key}` pattern within a command value is replaced with the corresponding
entry in the runtime state. Unknown keys are left untouched (for example,
`${missing}` remains `${missing}`).

## Example

```
# metadata
@name Hello CLRNet

# commands
set greeting Hello from CLRNet!
print ${greeting}
append greeting This script uses ${script.name}.
print ${greeting}
```

## Tips

* Use `clrnet explain <script>` to preview how the runtime will interpret a
  script before running it.
* `clrnet run --dry-run` executes the script without sleeping, which is useful
  for CI pipelines.
* Combine `set` and `append` to build larger blocks of text before writing them
  to the console or another destination.
