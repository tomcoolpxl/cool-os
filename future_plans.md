Prototype 15: Process Model Cleanup

Goals:

Proper process states:

READY

RUNNING

BLOCKED

ZOMBIE

Parent/child relationship

Wait syscall (waitpid minimal)

Clean process exit handling

Prototype 16: Virtual Memory Isolation

Goals:

Separate page tables per process

Kernel mapped globally

User address spaces isolated

Prepare for fork later

This is a major correctness milestone.

Prototype 17: Preemptive Scheduler

Goals:

Timer-driven preemption

Time slicing

No reliance on cooperative yield

This makes the system actually multitasking.

Prototype 18: Improved VFS

Goals:

Subdirectories

Long filenames (LFN)

Path resolution

File descriptors usable by user programs

Prototype 19: Userland Shell

Goals:

Move shell out of kernel

Use libc

Real stdin/stdout

Exec model cleanup