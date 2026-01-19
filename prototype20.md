```md
# Prototype 20: Userland Shell and Full Process Execution Model

## Purpose

Move the interactive shell out of the kernel and into userland. This completes the fork + exec process model and establishes a clean separation between kernel and user responsibilities.

After this prototype:

- The kernel provides mechanisms (processes, memory, filesystem, IO)
- Userland provides policy (shell, command interpretation)
- The kernel shell is reduced to a debug fallback only

This is the transition point to a real multi-process operating system.

## Scope

In scope:
- Userland shell implementation
- fork + exec based command execution
- stdin/stdout integration with console
- Foreground job execution
- wait() integration
- PATH-like command lookup (simple)
- Kernel boot launches init shell process

Out of scope:
- Background jobs (&)
- Pipes and redirection
- Job control signals
- TTY line discipline
- Environment variables
- Script execution

## Dependencies

Required components:
- fork() and COW support (Prototype 18)
- execve() syscall (Prototype 19)
- wait()/process lifecycle (Prototype 15)
- Preemptive scheduler (Prototype 17)
- Userland libc + stdio (Prototype 14)
- VFS filesystem access (Prototype 9)
- Framebuffer console and keyboard input (Prototypes 11,12)

## Boot Flow Changes

### Old Model

Kernel boots and runs kernel shell directly.

### New Model

Kernel boot sequence:

1) Initialize subsystems
2) Mount filesystem
3) Spawn INIT process (PID 1)
4) INIT execs userland shell ELF
5) Kernel switches to scheduler idle loop

Kernel no longer owns the interactive loop.

## INIT Process

Provide a small init program:

- File: user/init.c or init.elf
- Responsibility:
  - Launch user shell using execve()
  - Restart shell if it exits unexpectedly (optional but recommended)

Example behavior:

```

while (1) {
pid = fork();
if (pid == 0)
execve("/BIN/SHELL.ELF");
wait();
}

```

## Userland Shell Design

### Shell Loop

Basic REPL:

1) Print prompt ("> ")
2) Read input line
3) Parse command
4) fork()
5) In child: execve(command)
6) In parent: wait()

Shell must not block kernel scheduling.

## Command Parsing

Simple tokenization:

- Split on spaces
- argv[0] = command
- argv[1..] = arguments

No quoting support required.

## PATH Resolution

Implement minimal PATH logic:

Search directories in order:

- /BIN
- /

If user types:

```

ls

```

Shell tries:

- /BIN/LS.ELF
- /LS.ELF

Stop on first match.

Case sensitivity behavior must match filesystem.

## Built-in Commands

Shell must support a small built-in command set without exec:

### exit

Syntax:

```

exit

```

Behavior:
- Terminates shell
- Returns to init which restarts shell (optional)

### clear

Syntax:

```

clear

```

Behavior:
- Clears framebuffer console

### help

Syntax:

```

help

```

Behavior:
- Prints command list and usage

Built-ins must be executed without fork/exec.

## stdin/stdout Integration

### Input

Shell uses:

- libc getchar() / readline helper
- SYS_read (if implemented) or keyboard abstraction

### Output

Shell uses:

- printf()
- puts()

Mapped to:

- SYS_write -> kernel console

## Process Interaction Model

Shell behavior for commands:

Example:

```

hello

```

Execution:

- fork()
- child: execve("/BIN/HELLO.ELF")
- parent: wait()

Exit status printed optionally:

```

[exit code: 0]

```

## Error Handling

Shell must handle:

- Command not found
- execve failure
- fork failure
- Invalid input

Print readable error messages.

Kernel must not panic on shell misuse.

## Validation Tests

### TEST1: Boot to User Shell

Boot system.

Expected:

- Kernel initializes
- INIT starts shell
- Prompt appears automatically

No kernel interaction required.

### TEST2: Program Execution

Commands:

```

hello
yieldtest

```

Expected:

- Programs execute correctly
- Return to shell after exit

### TEST3: fork + exec Stability

Run commands repeatedly.

Expected:

- No memory leaks
- No zombie accumulation
- Stable scheduling

### TEST4: Built-in Commands

Test:

```

help
clear
exit

```

Expected:

- Correct behavior
- Shell restarts via init if exit used

## Safety Requirements

- Shell must not run with kernel privileges
- User pointers validated in syscalls
- Shell must not crash kernel on malformed input
- Prevent fork bombs (optional soft limit)

## Implementation Notes

### Suggested Files

Userland:

- user/shell.c
- user/init.c

Kernel:

- src/kernel.c (spawn init instead of shell)
- Remove kernel shell loop

Build System:

- Add shell and init programs to FAT image
- Install under /BIN directory

### Shell Memory Usage

Shell should:

- Avoid large stack buffers
- Use bounded input buffers (256 bytes recommended)

### Debug Fallback

Keep kernel shell available behind:

- Boot flag
- Compile-time DEBUG_SHELL option

Useful for recovery.

## Acceptance Criteria

Prototype 20 is complete when:

- System boots directly into userland shell
- Kernel shell no longer handles commands
- fork + exec workflow is stable
- User commands work consistently
- No kernel panics under heavy shell usage

## Next Prototype (Planned)

Prototype 21: File Descriptor Table and Userland IO Expansion

Goals:
- Per-process FD tables
- stdin/stdout/stderr separation
- File descriptors usable by user programs
- Prepare for pipes and redirection
```
