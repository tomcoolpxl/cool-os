````md
# Prototype 10: Fixed-Resolution Framebuffer (960x540) and Software Blitter

## Purpose

Introduce graphical output using the UEFI framebuffer via Limine and establish a fixed internal rendering resolution of **960x540, 32-bit true color**. This prototype implements:

- Preferred framebuffer mode selection
- Software back buffer rendering at fixed resolution
- Scaled presentation to the hardware framebuffer
- Double buffering and blitting pipeline

This creates a stable graphics target for games and UI while remaining independent of physical display resolution (4K, 1080p, etc).

## Design Goals

- Internal render resolution is always 960x540
- Output is scaled to the actual framebuffer resolution
- Rendering code never depends on monitor native resolution
- Back buffer always uses 32-bit pixels (XRGB8888 or equivalent)

## Scope

In scope:
- Limine framebuffer mode request (prefer 960x540x32)
- Framebuffer fallback handling
- Back buffer allocation at fixed resolution
- Software scaling blitter
- Full-frame presentation
- Basic drawing primitives
- Frame pacing

Out of scope:
- Hardware acceleration
- GPU drivers
- VSync synchronization
- Window manager
- Text console (next prototype)

## Dependencies

Required:
- Limine boot protocol
- HHDM mapping helpers
- Kernel heap and PMM
- Timer sleep API
- Paging infrastructure
- Stable kernel main loop

## Framebuffer Mode Strategy

### Preferred Mode

Kernel must request:

- Resolution: 960x540
- Color depth: 32 bits per pixel

Using Limine framebuffer request with preferred mode selection.

### Fallback Behavior

If firmware does not provide 960x540:

- Accept the default framebuffer mode
- Continue using 960x540 as internal render resolution
- Enable software scaling to the real framebuffer size

Kernel must never panic on unsupported resolution.

## Framebuffer Acquisition

Kernel must retrieve from Limine:

- Front buffer physical address
- Width
- Height
- Pitch
- Bits per pixel
- Pixel format

Validation:

- Only accept 32-bit framebuffer formats
- Panic on unsupported formats (eg 16-bit, indexed)

## Memory Model

### Front Buffer

- Provided by firmware
- Accessed through HHDM mapping
- Never drawn into directly
- Treated as display scanout buffer

### Back Buffer (Render Buffer)

Allocated by kernel:

- Resolution: 960x540
- Format: 32-bit
- Size: 960 * 540 * 4 bytes (~2 MB)
- Cacheable normal RAM

All rendering occurs here.

## Core Structures

Example structure:

```c
typedef struct {
    uint32_t hw_width;
    uint32_t hw_height;
    uint32_t hw_pitch;
    uint32_t bpp;
    void *front;

    uint32_t render_width;
    uint32_t render_height;
    void *back;
} framebuffer_t;
````

render_width and render_height are fixed to 960x540.

## Drawing Primitives (Back Buffer Only)

Implement:

* fb_putpixel(x, y, color)
* fb_clear(color)
* fb_fill_rect(x, y, w, h, color)

Clipping rules:

* Writes outside render buffer bounds must be ignored
* Coordinates always relative to 960x540 space

Color format:

* 0x00RRGGBB or equivalent native layout

## Presentation Pipeline

### Scaling Strategy

When presenting:

* Scale 960x540 back buffer into hardware framebuffer
* Maintain aspect ratio (16:9)
* Center image if needed
* Add black bars if aspect mismatch occurs

Scaling algorithm (Prototype 10):

* Nearest-neighbor scaling (simple and fast)

Future prototypes may add bilinear filtering.

### Present Function

Implement:

```c
void fb_present(void);
```

Behavior:

* Iterate over hardware framebuffer pixels
* Map each destination pixel to source back buffer pixel
* Write scaled output to front buffer

Direct memcpy is only used if hardware framebuffer is exactly 960x540.

## Frame Timing

Target frame rate:

* 60 FPS preferred
* 30 FPS acceptable fallback

Use timer_sleep_ms() for pacing.

Busy waiting is forbidden.

## Validation Tests

### TEST1: Solid Fill

* Clear back buffer to blue
* Present
  Expected:
* Screen shows blue fullscreen image scaled properly

### TEST2: Moving Rectangle

* Draw white rectangle moving horizontally
* Present each frame
  Expected:
* Smooth animation
* No flicker
* Correct scaling

### TEST3: Resolution Independence

* Print framebuffer hardware resolution to serial
* Verify rendering remains 960x540 logical resolution

### TEST4: Stress Render

* Alternate full-screen colors at 60 FPS for 10 seconds
  Expected:
* Stable output
* No kernel faults
* No memory corruption

## Safety Requirements

* Front buffer access must use HHDM mapping
* Back buffer must not overlap kernel memory
* No dynamic allocation inside render loop
* All drawing must be bounds checked

## Performance Requirements

* Scaling blit must complete within one frame interval
* No per-pixel front buffer writes during rendering stage
* Sequential writes preferred during presentation

## Implementation Notes

### Aspect Ratio

960x540 is 16:9.

Scaling must preserve aspect ratio on:

* 1920x1080
* 2560x1440
* 3840x2160 (4K)

Black bars allowed on non-16:9 displays.

### Cache Behavior

* Back buffer: cacheable
* Front buffer: write-combined or uncached (if supported)

### Debug Mode

Add option to:

* Disable scaling
* Draw back buffer centered unscaled

Useful for debugging pixel correctness.

## Deliverables

* prototype10.md (this document)
* Limine framebuffer request implementation
* Framebuffer initialization code
* Back buffer allocator
* Scaling blitter
* Drawing primitives
* Kernel graphics test code

## Next Prototype (Planned)

Prototype 11: Framebuffer Text Console

Goals:

* Bitmap font rendering
* Character grid on back buffer
* Scrollback support
* Kernel log output on screen
* Panic screen renderer

