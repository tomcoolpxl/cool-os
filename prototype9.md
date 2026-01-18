Next step should be Prototype 9: Filesystem-backed loading (FAT read + seek) and a minimal VFS abstraction. That gets you off Limine modules and onto a disk image, which is required for DOOM WAD loading later.

Before writing Prototype 9, one quick technical note to keep you honest: your "ELF loader with relocation support" is fine, but for now keep user programs either ET_EXEC at fixed addresses or ET_DYN with a simple base relocation approach. Do not drift into dynamic linking.

Below is a complete prototype9.md.

```md
# Prototype 9: Filesystem and Disk-Backed Program Loading (FAT)

## Purpose

Replace Limine-module-based user program loading with disk-backed loading from a filesystem. Implement a minimal read-only FAT filesystem driver and a tiny VFS-style interface sufficient to:

- List directories (optional)
- Open and read files
- Seek within files
- Load and execute ELF64 user programs from paths

This prototype is a prerequisite for loading game assets (eg DOOM WAD) and building a shell.

## Scope

In scope:
- Block device abstraction for QEMU (initially virtio-blk or AHCI, choose one)
- Partition handling (optional; may assume "superfloppy" FAT image first)
- FAT32 (or FAT16) read-only support
- File API: open, read, seek, close
- ELF loading from filesystem path
- Validation tests using files stored in disk image

Out of scope:
- Write support
- Journaling
- Permissions
- Long file name (LFN) support (optional; can restrict to 8.3 names initially)
- Caching beyond a single sector buffer (optional)
- VFS mount table supporting multiple FS types (later)
- Path normalization corner cases (later)

## Dependencies

Required components:
- Prototype 8: ELF loader and task_create_elf() or equivalent interface
- Kernel heap (kmalloc/kfree)
- PMM/HHDM helpers
- Serial output for diagnostics
- Stable exception handling
- Cooperative scheduler and user mode

Recommended:
- A minimal block cache layer (optional)
- CRC not required

## Storage Model (Phase 1)

Phase 1 assumes a single FAT volume stored in a raw disk image used by QEMU.

Two acceptable approaches:

A) FAT "superfloppy" image (no partition table)
- Simplest
- FAT boot sector at LBA 0

B) MBR partitioned disk
- Adds partition parsing work
- Allows "realistic" layout

For teaching simplicity, start with A and optionally add B later.

## Block Device Layer

### Requirements

Provide a minimal block device interface:

- int block_read(uint64_t lba, uint32_t count, void *dst);

Where:
- lba is 512-byte sector address
- count is number of sectors
- dst is kernel virtual pointer (HHDM accessible)

All higher layers use this interface.

### Device choice

Pick one for QEMU first:

Option 1 (recommended): virtio-blk
- Simple MMIO/PCI device model
- Good for OS dev
- Works in QEMU

Option 2: AHCI
- More complex
- More "real hardware"

Acceptance: virtio-blk is recommended for speed-to-success.

## FAT Filesystem Layer (Read-Only)

### Supported FAT variant

Choose FAT32 unless you already have FAT16 tooling. FAT32 is common for ESP-style volumes.

### Required operations

Implement:
- fat_mount()
- fat_open(path)
- fat_read(handle, buf, nbytes)
- fat_seek(handle, offset)
- fat_close(handle)

### Minimal file handle

Store:
- current position
- start cluster
- file size
- current cluster
- cached sector buffer (optional)

### Directory traversal

Phase 1 path support:
- root directory + one subdirectory level (optional)
- 8.3 filenames only (recommended initial constraint)

If LFN is not implemented:
- Document limitation clearly
- Ensure build system uses 8.3 names for test files (eg INIT.ELF, YIELD1.ELF)

### FAT parsing requirements

Must correctly parse:
- BPB from boot sector
- FAT start LBA
- Data region start LBA
- Sectors per cluster
- Root directory cluster (FAT32)

Cluster to LBA calculation:
- data_lba = data_region_lba + (cluster - 2) * sectors_per_cluster

FAT entry reading:
- read FAT sector containing the cluster entry
- follow chain until end-of-chain

### Caching (optional but recommended)

Implement a 1-sector cache:
- cache last read sector to reduce repeated FAT reads

Not required for correctness.

## VFS Thin Abstraction (Minimal)

Implement a tiny API in include/fs.h:

- int vfs_open(const char *path, int flags)
- int vfs_read(int fd, void *buf, size_t n)
- int vfs_seek(int fd, uint64_t off)
- int vfs_close(int fd)

Only support:
- read-only open
- sequential read + seek

File descriptors can be indices into a static table or heap-allocated handles.

## ELF Loading From FS

Provide:

- task_t *task_create_from_path(const char *path);

Behavior:
- vfs_open(path)
- read file into kernel buffer (or stream into ELF loader if supported)
- elf_load_at() into user memory
- create user task and schedule it
- close file handle

Phase 1 can read the whole file into memory first for simplicity.

## Disk Image Build Requirements

Build system must create a FAT image containing:
- one or more ELF user programs
- optional text files for read tests
- later: assets (WAD)

Use mtools for host-side population.

Naming constraints (if 8.3 only):
- INIT.ELF
- YIELD1.ELF
- YIELD2.ELF
- FAULT.ELF

## QEMU Requirements

QEMU must attach disk image in a way the chosen block driver can see.

For virtio-blk:
- Add a virtio-blk device backed by your FAT image

Kernel must detect and mount it on boot.

## Validation Tests

### TEST1: Mount FAT and read a file
- Mount volume
- Open "HELLO.TXT"
- Read contents
- Print to serial
Expected: contents match

### TEST2: Load and run INIT.ELF from disk
- Load ELF from filesystem path
- Program prints and exits
Expected: output correct, kernel survives

### TEST3: Seek test
- Read file, seek back, read again
Expected: identical output

### TEST4: Two user programs from disk
- Load YIELD1.ELF and YIELD2.ELF
Expected: alternating output as in Prototype 8

## Acceptance Criteria

Prototype 9 is complete when:
- A FAT volume is mounted from a QEMU disk image
- Kernel can open/read/seek/close files reliably
- Kernel can load and execute ELF64 user programs from disk paths
- Tests pass consistently under QEMU with KVM
- No page faults or memory corruption occurs during reads
- Limitations (8.3 only, read-only) are documented

## Deliverables

- prototype9.md (this document)
- Block device driver (virtio-blk recommended)
- FAT read-only filesystem implementation
- Minimal VFS API implementation
- Updated build system to generate/populate FAT image
- Kernel tests demonstrating file IO and disk-backed ELF execution

## Next Prototype (Planned)

Prototype 10: Framebuffer and software blitter
- Acquire GOP framebuffer via Limine
- Back buffer allocation
- Present/blit operations (full and rect)
- Minimal framebuffer text renderer (optional)
```

If you want to keep scope minimal, do Prototype 9 with:

* virtio-blk
* FAT superfloppy (no partition table)
* 8.3 names only

That gets you disk-backed programs quickly and keeps the code teachable.
