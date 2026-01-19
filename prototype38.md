```md
# Prototype 38: Doomgeneric Port (No Sound, Software Rendering)

## Purpose

Port and run Doom on coolOS using the doomgeneric platform layer. This prototype validates the entire OS stack under a real-world workload and demonstrates full graphics, input, filesystem, memory, and timing integration.

After this prototype:

- Doom runs natively on coolOS
- Framebuffer rendering works at real-time speed
- Keyboard input drives gameplay
- WAD files load from disk
- The OS proves capable of running non-trivial applications

Sound is intentionally excluded to reduce complexity.

## Scope

In scope:
- doomgeneric source port
- coolOS platform backend
- Framebuffer rendering via /dev/fb0 mmap
- Keyboard input via /dev/input0
- High-resolution timing via CLOCK_MONOTONIC
- WAD file loading
- Game loop pacing

Out of scope:
- Sound and music
- Joystick/mouse
- Network multiplayer
- Savegames (optional)
- Windowed mode

## Dependencies

Required components:
- Native GCC toolchain (Prototype 35)
- musl libc runtime (Prototype 31)
- /dev/fb0 framebuffer mapping (Prototype 36)
- High-resolution timer + input API (Prototype 37)
- Stable filesystem with file read support
- Sufficient memory (>=64MB recommended)
- Preemptive scheduler stability

## Doom Source Choice

Use:

```

doomgeneric

```

Reasons:
- Portable backend architecture
- Minimal dependencies
- Proven on bare-metal and hobby OS targets
- Software renderer friendly

Avoid SDL-based ports.

## Platform Backend Interface

Implement:

```

doomgeneric_coolos.c

````

Required functions:

```c
void DG_Init(void);
void DG_DrawFrame(void);
void DG_SleepMs(int ms);
uint32_t DG_GetTicksMs(void);
int DG_GetKey(int *pressed, unsigned char *key);
void DG_Quit(void);
````

## Graphics Integration

### Framebuffer Setup

During DG_Init:

* Open /dev/fb0
* Read fb_info
* mmap framebuffer
* Allocate backbuffer in user memory

Expected format:

* 32-bit XRGB or ARGB

### Rendering Strategy

doomgeneric provides:

* 320x200 framebuffer output

Scale to screen:

Option A (Simple):

* Integer scale (2x, 3x, 4x)
* Center on screen
* Black bars allowed

Option B (Stretch):

* Bilinear scale (slower)
* Fullscreen stretch

Prototype 38 uses Option A.

### Frame Copy

Per frame:

* Convert doom buffer to framebuffer format
* Use memcpy-style bulk copy
* Avoid per-pixel syscall calls

## Input Integration

DG_GetKey():

* Poll /dev/input0
* Translate key codes to Doom key constants
* Support press/release events
* Map WASD/arrows/space/ctrl/esc

Maintain local key queue.

## Timing Integration

### Tick Source

Use:

```
clock_gettime(CLOCK_MONOTONIC)
```

DG_GetTicksMs():

* Return milliseconds since boot

### Frame Pacing

DG_SleepMs():

* Call nanosleep
* Avoid busy loop

Doom tick rate:

* 35 Hz target

Ensure consistent pacing.

## Filesystem Integration

Place WAD file:

```
/HOME/doom/doom1.wad
```

Launch:

```
doomgeneric -iwad /HOME/doom/doom1.wad
```

Ensure:

* open/read works
* Large sequential reads performant

## Performance Targets

Minimum acceptable:

* > = 30 FPS
* Input latency < 50ms
* No frame tearing severe enough to affect play

On QEMU/KVM:

* Expect full speed.

## Validation Tests

### TEST1: Startup

Run doomgeneric.

Expected:

* Doom title screen appears
* No crash

### TEST2: Input

Use keyboard:

* Move player
* Fire weapon
* Open menu

Expected:

* Immediate response

### TEST3: Performance

Let Doom run idle for 2 minutes.

Expected:

* No slowdown
* No memory leak
* Stable framerate

### TEST4: Exit Handling

Press quit key.

Expected:

* Return cleanly to shell
* Framebuffer restored to console

## Console Restore Behavior

On exit:

* Clear framebuffer
* Reinitialize kernel console
* Restore TTY ownership

Prevent visual corruption.

## Safety Requirements

* Doom must not crash kernel
* No framebuffer memory corruption
* Prevent invalid input buffer reads
* Validate WAD file size
* Prevent out-of-bounds blitting

## Implementation Notes

### Suggested Files

Userland:

* ports/doomgeneric_coolos.c
* Makefile for doomgeneric
* input mapping header

Kernel:

* Minor fixes uncovered by workload
* Improve memcpy speed if needed

### Optimization Opportunities

Optional:

* Write-combined framebuffer pages
* SSE memcpy for blits
* Dirty rectangle updates
* Page-aligned backbuffer

Not required for acceptance.

## Acceptance Criteria

Prototype 38 is complete when:

* Doom launches successfully
* Gameplay is responsive
* Frame rate stable
* No kernel crashes
* System returns cleanly to shell after exit

## Milestone Status

After Prototype 38:

coolOS will be capable of running:

* Graphical applications
* Real-time workloads
* Native compiled software
* Classic game engines

This represents a full-stack OS engineering achievement.

```
```
