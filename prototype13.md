```md
# Prototype 13: Kernel Shell and Command Interface

## Purpose

Introduce an interactive kernel-side shell that allows basic user interaction, filesystem inspection, and program execution. This shell will use the framebuffer console and keyboard input to provide a command-driven interface similar to early UNIX monitors and boot shells.

This prototype connects all major subsystems implemented so far: console, keyboard, filesystem, ELF loader, and scheduler.

## Scope

In scope:
- Interactive command loop
- Command parsing
- Built-in shell commands
- Integration with VFS and task loader
- Console output formatting
- Error reporting

Out of scope:
- User-space shell
- Job control
- Pipes and redirection
- Background execution
- Environment variables
- Command history persistence

## Dependencies

Required components:
- Framebuffer console (Prototype 11)
- Keyboard input and readline support (Prototype 12)
- FAT32 + VFS read-only filesystem (Prototype 9)
- ELF loader + task_create_from_path() (Prototype 8/9)
- Cooperative scheduler
- User mode + syscalls

## Architecture Overview

The shell runs as a kernel task and operates in a simple read-eval-print loop:

1. Print prompt
2. Read line from keyboard
3. Parse command
4. Execute built-in handler or launch user program
5. Return to prompt

The shell must not block the entire system except when waiting for keyboard input.

## Command Input Handling

### Input Buffer

Use:

- kbd_readline(char *buf, size_t max)

Behavior:
- Blocks until Enter
- Echoes characters to console
- Handles backspace
- Null-terminates input string

Maximum line length:

- Minimum 128 characters recommended

## Command Parsing

### Tokenization

Split input line by spaces:

- First token: command name
- Remaining tokens: arguments

Rules:
- Consecutive spaces collapse into one separator
- No quoting support required
- No escape characters

Example:

```

run init.elf

```

Tokens:

- argv[0] = "run"
- argv[1] = "init.elf"

### Argument Limit

Support at least:

- 8 arguments per command

Extra tokens may be ignored.

## Built-in Commands

### help

Syntax:

```

help

```

Behavior:
- Print list of available commands and short descriptions

### clear

Syntax:

```

clear

```

Behavior:
- Clear framebuffer console
- Reset cursor position

### ls

Syntax:

```

ls

```

Behavior:
- List files in root FAT directory
- Print filenames and sizes
- Only root directory support required in this prototype

### cat

Syntax:

```

cat <filename>

```

Behavior:
- Open file via VFS
- Read contents
- Print to console
- Close file

Constraints:
- Limit output to a reasonable size (eg first 64 KB)
- Handle missing file gracefully

### run

Syntax:

```

run <program.elf>

```

Behavior:
- Load ELF via task_create_from_path()
- Schedule user task
- Return to shell immediately (cooperative model)
- Shell continues running concurrently

User program output appears on console.

### reboot (optional)

Syntax:

```

reboot

```

Behavior:
- Trigger CPU reset using keyboard controller or ACPI reset if implemented
- Optional in this prototype

## Error Handling

Shell must detect:

- Unknown commands
- Missing arguments
- File not found
- ELF load failure

Print descriptive error messages to console.

Kernel must not panic on shell errors.

## Shell Loop Behavior

Pseudo-flow:

```

while (true):
print("> ")
line = kbd_readline()
parse tokens
if no tokens: continue
dispatch command

```

Shell must remain responsive while user tasks run.

## Task Interaction Model

For Prototype 13:

- User tasks run cooperatively
- Shell remains runnable
- Users must use SYS_yield in programs to allow shell responsiveness

No preemptive multitasking required.

## Validation Tests

### TEST1: Basic Shell Interaction

Commands:

- help
- clear
- unknowncommand

Expected:
- Correct output
- No crash

### TEST2: Filesystem Integration

Commands:

- ls
- cat README.TXT (or test file)

Expected:
- Correct file listing
- Correct file output

### TEST3: Program Execution

Command:

```

run INIT.ELF

```

Expected:
- Program executes in user mode
- Prints output
- Exits cleanly
- Shell remains usable

### TEST4: Multiple Program Launch

Commands:

```

run YIELD1.ELF
run YIELD2.ELF

````

Expected:
- Both programs run concurrently
- Output interleaves
- Shell prompt remains usable

## Safety Requirements

- Shell input buffer must be bounds-checked
- File reads must validate sizes
- ELF loader errors must not crash kernel
- Shell must not block interrupts

## Implementation Notes

### Suggested Files

Add:

- include/shell.h
- src/shell.c

Modify:

- src/kernel.c (spawn shell task)
- Makefile (add shell.c)

### Output Formatting

Use framebuffer console functions only.

Serial output remains optional debug mirror.

### Command Table

Implement built-in commands using a static table:

```c
typedef struct {
    const char *name;
    void (*handler)(int argc, char **argv);
} shell_cmd_t;
````

This avoids large if-else chains.

## Acceptance Criteria

Prototype 13 is complete when:

* Interactive prompt appears on framebuffer console
* Keyboard input works reliably
* Built-in commands function correctly
* User programs can be launched from shell
* Shell remains stable during repeated command execution
* No kernel crashes or memory corruption occur

## Next Prototype (Planned)

Prototype 14: Userland stdio and libc expansion

Goals:

* Map SYS_write to stdout abstraction
* Add basic printf implementation in userland
* Redirect user program output cleanly to console
* Prepare environment for more complex applications (including games)

```
```
