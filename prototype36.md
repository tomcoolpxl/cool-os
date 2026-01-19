```md
# Prototype 36: Userland Framebuffer mmap API and Graphics Access Layer

## Purpose

Expose the hardware framebuffer to userland in a controlled, high-performance way using mmap(). This enables native graphics applications to render directly to video memory and is the critical prerequisite for running Doom and other graphical software.

After this prototype:

- User programs can map the framebuffer
- High-speed pixel blitting becomes possible
- Graphics programs no longer rely on kernel drawing
- The OS gains real graphical application capability

## Scope

In scope:
- /dev/fb0 character device
- Framebuffer metadata interface
- mmap() support for framebuffer memory
- Userland-safe VRAM mapping
- Write-combined or uncached mapping flags (if supported)
- Simple ioctl-style info query (optional)

Out of scope:
- GPU acceleration
- OpenGL/Vulkan
- Hardware modesetting
- Multiple monitors
- Window system
- Compositor

## Dependencies

Required components:
- Framebuffer driver (Prototype 10/11)
- mmap() infrastructure (Prototype 25)
- VFS + devfs (Prototype 27)
- Permissions model (Prototype 28)
- Syscall hardening (Prototype 29)
- musl libc runtime (Prototype 31)

## Device Interface Design

### Device Node

Expose framebuffer as:

```

/dev/fb0

```

Permissions:

```

rw-rw-rw- (0666) initially

````

Later can be restricted.

## Framebuffer Metadata API

Define structure:

```c
typedef struct fb_info {
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t bpp;
    uint64_t phys_addr;
} fb_info_t;
````

Provide access via:

Option A (simple):

* read() on /dev/fb0 returns fb_info struct

Option B (cleaner):

* ioctl(FBIOGET_INFO)

Prototype 36 may use read() first.

## mmap() Behavior

Allow:

```
fd = open("/dev/fb0", O_RDWR);
buf = mmap(NULL, fb_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
```

Kernel behavior:

* Map physical framebuffer pages
* User-accessible
* Writable
* Non-executable
* Cache policy:

  * UC (uncached) initially
  * WC (write-combined) later optimization

Mapping must:

* Be page-aligned
* Respect pitch padding

## Framebuffer Layout

Expose native layout:

* XRGB or ARGB 32-bit
* One contiguous linear buffer
* Stride = pitch

No conversion layer.

User program handles format.

## Page Protection

Framebuffer pages:

* U/S = 1
* RW = 1
* NX = 1
* Not included in COW
* Not swappable

Prevent mapping outside VRAM region.

## Kernel Implementation

### devfs Integration

Add device:

```
register_device("fb0", fb_read, fb_write, fb_mmap);
```

fb_mmap handler:

* Validate offset and length
* Map VRAM pages into process page table
* Track mapping for munmap()

### mmap Handler Extension

Extend mmap path:

If FD is device with mmap handler:

* Delegate mapping to device driver

## Userland Graphics API (Minimal)

Provide optional helper library:

```
libfb.a
```

Functions:

```c
fb_open();
fb_get_info();
fb_map();
fb_unmap();
```

Not required for Doom, but useful.

## TTY Interaction

When framebuffer is in use by fullscreen app:

* Shell should relinquish TTY foreground
* Console output may be suspended
* On exit, console restored

Reuse job control (Prototype 24).

## Validation Tests

### TEST1: Framebuffer Map

User program:

* open /dev/fb0
* mmap framebuffer
* Fill with solid color

Expected:

* Screen fills with color
* No kernel crash

### TEST2: Pattern Draw

Draw:

* Color gradient
* Checkerboard pattern

Expected:

* Correct visual output
* No tearing artifacts

### TEST3: Stress Copy

Loop:

* Full-screen memcpy every frame

Expected:

* Stable performance
* No slowdown
* No memory fault

### TEST4: Permission Enforcement

Change /dev/fb0 permission.

Attempt open as restricted user.

Expected:

* Access denied

## Performance Considerations

Initial version acceptable with:

* Uncached VRAM

Later optimize:

* PAT write-combining
* Double buffering in userland
* Page-aligned bulk copy

## Safety Requirements

* Prevent mapping arbitrary physical memory
* Validate mapping size
* Prevent kernel VRAM overwrite
* Ensure unmap cleans page tables
* Prevent use-after-unmap

## Implementation Notes

### Suggested Files

New:

* include/fbdev.h
* src/fbdev.c

Modified:

* src/devfs.c
* src/mmap.c
* src/framebuffer.c
* src/syscall.c

### Page Attribute Table (Optional)

If supported:

* Mark framebuffer pages as WC
* Improves write throughput drastically

Not required for Prototype 36 acceptance.

## Acceptance Criteria

Prototype 36 is complete when:

* /dev/fb0 exists
* Framebuffer can be mmap'd
* User program can draw directly
* Performance sufficient for real-time rendering
* System remains stable under repeated mapping/unmapping

## Next Prototype (Planned)

Prototype 37: High-Resolution Timer and Input API for Game Ports

Goals:

* Upgrade timer to 1ms resolution (TSC or HPET)
* Improve keyboard event API
* Expose monotonic clock syscall
* Prepare Doom timing loop
* Reduce input latency
