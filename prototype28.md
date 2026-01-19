````md
# Prototype 28: Users, Permissions, and Basic Security Model

## Purpose

Introduce a minimal user and permission model to enforce access control between processes and files. This prototype establishes the concept of privilege levels in userland and lays the groundwork for secure multi-user operation.

After this prototype:

- Processes run under user identities
- Files and devices have ownership
- Permission bits control access
- Root user concept exists
- Privileged operations are restricted

## Scope

In scope:
- User IDs (UID) and Group IDs (GID)
- Root user (UID 0)
- File ownership
- rwx permission bits
- Permission enforcement in VFS
- Credential inheritance on fork/exec
- Basic privilege checks

Out of scope:
- Multiple groups per user
- Setuid/setgid bits
- ACLs
- Capabilities
- Authentication system
- Login manager
- Encrypted storage

## Dependencies

Required components:
- VFS with mount support (Prototype 27)
- FD layer (Prototype 21)
- Userland shell (Prototype 20)
- execve and fork (Prototypes 18/19)
- Process lifecycle management (Prototype 15)
- Device filesystem (/dev)

## Credential Model

### Process Credentials

Extend process structure:

```c
typedef struct credentials {
    uid_t uid;
    gid_t gid;
} credentials_t;
````

Attach to each process:

* creds.uid
* creds.gid

Initial kernel-launched init process:

* uid = 0
* gid = 0

## File Metadata Extensions

Extend VFS inode or file struct:

```c
typedef struct file_meta {
    uid_t owner;
    gid_t group;
    uint16_t mode;   // rwx bits
} file_meta_t;
```

Mode bits:

```
Owner: r(4) w(2) x(1)
Group: r(4) w(2) x(1)
Other: r(4) w(2) x(1)
```

Example:

* 0755 = rwxr-xr-x

## Default Permissions

When creating files:

* owner = current uid
* group = current gid
* mode = 0644 for files
* mode = 0755 for directories

/dev nodes:

* Owned by root
* Permissions vary by device type

## Permission Enforcement

### Read Access

Allow read if:

* uid == owner AND owner read bit set
* OR gid == group AND group read bit set
* OR other read bit set

### Write Access

Allow write if:

* uid == owner AND owner write bit set
* OR gid == group AND group write bit set
* OR other write bit set

### Execute Access

For binaries:

Allow exec if:

* x bit set according to same rules

Shell must respect this when executing programs.

## Root Privileges

UID 0:

* Bypasses permission checks
* Allowed to:

  * Access any file
  * Create device nodes
  * Mount filesystems (future)
  * Send signals to any process

Other users restricted.

## fork and exec Credential Handling

### fork()

Child inherits:

* uid
* gid

### execve()

Credentials unchanged.

Later prototype may add setuid binaries.

## Shell Integration

Shell runs as:

* UID 0 initially (single-user mode)

Add built-in command:

```
id
```

Outputs:

```
uid=0 gid=0
```

Optional:

```
su <uid>
```

Allows switching user identity (no password yet).

## Device Access Control

/dev permissions examples:

* /dev/console: rw-rw-rw-
* /dev/keyboard: r--r--r--
* /dev/null: rw-rw-rw-

Kernel enforces access based on FD open flags.

## Validation Tests

### TEST1: File Permission Enforcement

Create file as root:

* chmod to 0600
* switch to uid 1000
* attempt read

Expected:

* Access denied

### TEST2: Exec Permission

Remove execute bit from program.

Attempt to run.

Expected:

* Permission denied

### TEST3: Root Override

As UID 0:

* Access protected file

Expected:

* Access succeeds

### TEST4: fork Inheritance

User process forks.

Expected:

* Child inherits same uid/gid

### TEST5: Device Permission

Restrict /dev/keyboard to root only.

Try read as normal user.

Expected:

* Access denied

## Safety Requirements

* Prevent negative UID/GID
* Prevent overflow in permission bits
* Enforce checks at VFS boundary
* Prevent user from modifying kernel-owned structures
* Validate metadata updates

## Implementation Notes

### Suggested Files

New:

* include/cred.h
* src/cred.c
* include/perm.h
* src/perm.c

Modified:

* src/task.c
* src/syscall.c
* src/vfs.c
* src/devfs.c
* src/fd.c
* userland shell

### chmod and chown

Optional minimal syscalls:

* chmod(path, mode)
* chown(path, uid, gid)

Can be restricted to root only.

### Storage

FAT filesystem does not support Unix permissions natively.

Prototype solution:

* Store permissions in memory only
* Or attach metadata in VFS layer
* Persistence optional

## Acceptance Criteria

Prototype 28 is complete when:

* Processes have uid/gid
* File access obeys permission bits
* Root user bypass works
* exec permissions enforced
* Shell can demonstrate permission denial
* No security bypass bugs discovered

## Next Prototype (Planned)

Prototype 29: System Calls Hardening and Capability Isolation

Goals:

* Syscall argument validation
* Kernel boundary hardening
* Per-process capability flags
* Privilege separation improvements
* Security audit phase

```
```
