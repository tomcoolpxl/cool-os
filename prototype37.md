````md
# Prototype 37: High-Resolution Timer and Input Event API (Game-Grade Timing, No Sound)

## Purpose

Provide millisecond-grade monotonic timing and a stable userland input event发现 so interactive applications (including Doom-class ports) have:

- Accurate frame timing
- Smooth movement and consistent tick rates
- Low-latency keyboard input

After this prototype:

- Userland can query monotonic time at ~1ms or better
- Sleep has millisecond precision
- Keyboard input is available as event stream (not only cooked TTY lines)
- Fullscreen apps can poll input and time without fighting the shell

## Scope

In scope:
- High-resolution monotonic clock implementation
- nanosleep/usleep accuracy improvements
- /dev/input0 keyboard event device
- Non-canonical input mode for TTY (optional but recommended)
- Foreground application input focus integration

Out of scope:
- Mouse support
- Joysticks/gamepads
- Key repeat configuration
- Sound timing
- Networking time protocols

## Dependencies

Required components:
- Timer interrupts (PIT) working (Prototype 4/5 baseline)
- Preemptive scheduling (Prototype 17)
- Signals + job control (Prototypes 23/24)
- devfs support (Prototype 27)
- FD model (Prototype 21)
- Userland shell (Prototype 20)
- Framebuffer mmap (/dev/fb0) (Prototype 36)

## High-Resolution Time Strategy

### Choose One Primary Clock Source

Option A (Recommended): TSC-based monotonic clock
- Fast and high resolution
- Requires calibration against PIT during boot
- Works well under QEMU/KVM and most modern CPUs

Option B: HPET
- More complex device programming
- Not always enabled in firmware
- Good long-term option

Prototype 37 uses Option A.

## TSC Monotonic Clock

### Boot Calibration

At boot:

1) Program PIT to known rate (already have)
2) Read TSC at start
3) Wait fixed interval (eg 100ms) using PIT ticks
4) Read TSC at end
5) Compute tsc_hz

Store:

- uint64_t tsc_hz
- uint64_t tsc_ns_per_tick (fixed-point)
- boot_tsc

### Time Query

Implement:

```c
int clock_gettime(clockid_t clk, struct timespec *ts);
````

Support:

* CLOCK_MONOTONIC (primary)
* CLOCK_REALTIME (optional, can equal monotonic + constant offset)

Compute:

* delta_tsc = rdtsc() - boot_tsc
* ns = delta_tsc * 1e9 / tsc_hz

Accuracy requirement:

* Error < 5ms over 10 seconds in QEMU
* Better on real hardware

### Sleep Improvements

Implement nanosleep based on monotonic clock:

* sleep_until(now + duration)
* Put process into BLOCKED with wake_deadline_ns
* Timer interrupt checks deadlines and wakes processes

Avoid hlt loops in userland paths.

## Kernel Scheduler Timer Wheel (Minimal)

Add per-process:

* uint64_t wake_deadline_ns
* state PROC_SLEEPING or reuse PROC_BLOCKED with reason

In timer IRQ:

* Check sleeping tasks deadlines
* Move ready tasks to runqueue

This makes sleep accurate without busy waiting.

## Input Event API

### /dev/input0

Expose keyboard as event stream:

* open("/dev/input0")
* read() returns fixed-size events

Define:

```c
typedef struct input_event {
    uint64_t time_ns;  // monotonic timestamp
    uint16_t type;     // EV_KEY
    uint16_t code;     // key code (set1 or translated)
    uint32_t value;    // 1 press, 0 release
} input_event_t;
```

Type codes:

* EV_KEY = 1

Key codes:

* Use Linux-like key codes (recommended) or your own stable enum
* Document mapping

### Buffering

Maintain kernel ring buffer per device:

* 256 events minimum
* Overflow drops oldest or newest (document policy)

### Read Semantics

* Blocking read until at least one event
* Return multiple events if buffer has more

### Focus Rules

Keyboard events go to:

* Foreground TTY process group by default (cooked)
* Also to /dev/input0 consumers if they have focus

For Prototype 37, simplest acceptable behavior:

* /dev/input0 always receives raw events globally
* TTY cooked input continues separately

Optional improvement:

* Only foreground process group can open /dev/input0
* Enforced via permission bits or kernel focus

## TTY Non-Canonical Mode (Recommended)

Fullscreen apps need immediate keypresses.

Add ioctl-like control on stdin TTY:

* Set canonical=0
* Set echo=0

Minimal API option:

* open("/dev/tty0")
* write control sequences (not recommended)

Better:

* ioctl(TCSETMODE) (stub)

If ioctl framework not available, keep /dev/input0 as primary for apps.

## Validation Tests

### TEST1: Timer Precision

User program:

* Query clock_gettime monotonic
* sleep 100ms 20 times
* Measure average and variance

Expected:

* Average near 100ms (within +-5ms)
* No drift explosion

### TEST2: High-Frequency Animation

User program:

* Render loop targeting 35fps or 60fps
* Use monotonic time for frame pacing
* Draw moving rect using /dev/fb0 mapping

Expected:

* Smooth speed
* Consistent pacing independent of CPU speed

### TEST3: Input Event Stream

User program:

* Open /dev/input0
* Print events (press/release) with timestamps

Expected:

* Correct key codes
* Low latency
* Release events included

### TEST4: Sleep and Scheduling Stress

Run 10 tasks each sleeping 10ms repeatedly.

Expected:

* System remains responsive
* No busy-wait CPU burn
* No missed wakeups

## Safety Requirements

* Validate user pointers for clock_gettime and read buffers
* Prevent input buffer overflow corruption
* Ensure monotonic time never goes backwards
* Avoid division overflow in ns conversion
* Keep IRQ handler work bounded

## Implementation Notes

### Suggested Files

New:

* include/tsc.h, src/tsc.c
* include/time.h, src/time.c (kernel side)
* include/input.h, src/input.c
* src/input_kbd.c (keyboard event feeder)
* uapi headers for time and input events

Modified:

* src/timer.c (wake sleeping tasks)
* src/kbd.c (emit events)
* src/devfs.c (register /dev/input0)
* src/syscall.c (clock_gettime, nanosleep)
* libc wrappers for time and sleep

### Pitfalls

* TSC may be non-invariant on very old hardware; assume invariant for now
* Under QEMU, ensure -cpu host for stable TSC
* Use 128-bit math where needed for ns conversion

## Acceptance Criteria

Prototype 37 is complete when:

* CLOCK_MONOTONIC works with ~1ms or better resolution
* nanosleep achieves millisecond-level accuracy without busy-wait
* /dev/input0 provides press/release events
* Interactive graphics apps can time frames accurately
* System stays stable under timing + input stress

## Next Prototype (Planned)

Prototype 38: Doomgeneric Port (No Sound)

Goals:

* Implement doomgeneric platform layer for coolOS
* Use /dev/fb0 mmap for video
* Use /dev/input0 for input
* Use monotonic clock for tick pacing
* Load WAD from filesystem
* Run Doom at stable tick rate

```
::contentReference[oaicite:0]{index=0}
```
