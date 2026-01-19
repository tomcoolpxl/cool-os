````md
# Prototype 27: VFS Mount Points and Device Nodes (/dev)

## Purpose

Extend the virtual filesystem (VFS) to support multiple mount points and introduce a device filesystem (/dev). This enables uniform access to hardware devices and virtual kernel interfaces through file semantics.

After this prototype:

- Multiple filesystems can be mounted simultaneously
- A root filesystem hierarchy exists
- Devices are exposed as files
- User programs interact with devices via standard read/write/open

## Scope

In scope:
- VFS mount table
- Path traversal across mount points
- Root filesystem abstraction
- /dev pseudo filesystem
- Character device node framework
- Kernel device registration API

Out of scope:
- Permissions and ownership
- Block device mounting
- Removable media hotplug
- udev-style dynamic management
- Filesystem journaling

## Dependencies

Required components:
- VFS layer (Prototype 9)
- File descriptor abstraction (Prototype 21)
- Shell and userland tools (Prototype 20)
- Pipe and device IO semantics (Prototype 22)
- Keyboard and console drivers
- Block device layer

## VFS Mount Model

### Mount Table

Introduce mount structure:

```c
typedef struct mount {
    char mount_path[64];
    fs_ops_t *fs;
    void *fs_data;
} mount_t;
````

Kernel maintains:

* Fixed-size mount table
* Lookup by longest prefix match

## Root Filesystem

Boot sequence:

* FAT32 root mounted at "/"
* Becomes base filesystem

All other mounts overlay subpaths.

Example layout:

```
/
  BIN/
  DEV/
  LIB/
  HOME/
```

## Path Resolution

Path lookup algorithm:

1. Find longest matching mount_path
2. Strip prefix
3. Forward remaining path to filesystem driver

Example:

Path:

```
/dev/console
```

Matches mount "/dev" first, not "/".

## /dev Pseudo Filesystem

Introduce devfs:

* In-memory filesystem
* No backing storage
* Nodes created dynamically by kernel

Mount devfs at:

```
/dev
```

## Device Node Types

Support character devices:

```c
typedef struct dev_node {
    char name[32];
    ssize_t (*read)(void *buf, size_t len);
    ssize_t (*write)(const void *buf, size_t len);
} dev_node_t;
```

## Required Device Nodes

Minimum required:

### /dev/console

Maps to:

* Framebuffer console output
* stdin/stdout terminal

### /dev/keyboard

Maps to:

* Keyboard input stream

### /dev/null

Behavior:

* write: discard data
* read: return EOF

### /dev/zero (optional)

Behavior:

* read: return zero-filled buffer
* write: discard

## FD Layer Integration

When opening:

```
open("/dev/console")
```

FD layer must:

* Detect devfs file
* Attach FD entry with type FD_DEVICE
* Route read/write to device callbacks

## Mount API

Kernel function:

```c
int vfs_mount(const char *path, fs_ops_t *fs, void *data);
```

Used during boot:

* Mount FAT at "/"
* Mount devfs at "/dev"

Later usable by userland via mount syscall (optional future).

## Shell Integration

Shell tools can now use:

```
cat /dev/keyboard
echo hello > /dev/console
```

For testing and debugging.

## Validation Tests

### TEST1: Device File Access

Shell:

```
echo test > /dev/console
```

Expected:

* Output appears on screen

### TEST2: Keyboard Device

Shell:

```
cat /dev/keyboard
```

Expected:

* Typing prints characters

### TEST3: Null Device

Shell:

```
echo test > /dev/null
```

Expected:

* No output
* No error

### TEST4: Mixed Mount Paths

Access file from FAT:

```
cat /BIN/HELLO.ELF
```

Expected:

* Correct data output

## Safety Requirements

* Validate mount paths
* Prevent overlapping mount corruption
* Prevent kernel pointer leaks
* Protect devfs from filesystem operations not supported
* Prevent buffer overflow in device callbacks

## Implementation Notes

### Suggested Files

New:

* include/mount.h
* src/mount.c
* include/devfs.h
* src/devfs.c

Modified:

* src/vfs.c
* src/fd.c
* src/syscall.c
* src/kernel.c

### Mount Matching Strategy

Use:

* Longest prefix match
* Exact directory boundary check

Example:

"/dev" matches "/dev/console"
but not "/device/file"

## Acceptance Criteria

Prototype 27 is complete when:

* Multiple mounts function correctly
* /dev filesystem is operational
* Device nodes accessible via standard IO
* Shell can interact with devices
* No kernel crashes under device IO stress

## Next Prototype (Planned)

Prototype 28: Permissions, Users, and Basic Security Model

Goals:

* User IDs and group IDs
* File ownership
* Permission bits (rwx)
* Privilege separation groundwork
* Root user concept

```
```
