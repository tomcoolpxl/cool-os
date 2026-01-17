Good. Linear bitmap PMM is the right choice for a teaching kernel.

Below is the exact design you should implement for Prototype 2. This avoids common traps and keeps the code simple and correct.

---

# Prototype 2 Design: Bitmap Physical Memory Manager

## Core idea

One bit represents one 4 KiB physical frame.

* Bit = 1 -> frame used
* Bit = 0 -> frame free

Allocator scans bitmap linearly for a zero bit.

---

## Constants

In a shared header (memory.h or pmm.h):

```c
#define PAGE_SIZE 4096
#define PAGE_SHIFT 12
```

---

## Data structures

Global state:

```c
static uint8_t *pmm_bitmap;
static uint64_t pmm_frame_count;
static uint64_t pmm_bitmap_size;
static uint64_t pmm_free_frames;
```

All addresses stored as physical addresses except bitmap pointer which is virtual (HHDM mapped).

---

## Step 1: Parse Limine memory map

During kernel init:

Get:

* Highest physical address:

```c
max_addr = max(entry.base + entry.length)
```

Compute:

```c
pmm_frame_count = max_addr / PAGE_SIZE;
pmm_bitmap_size = (pmm_frame_count + 7) / 8;
```

Round bitmap size up to PAGE_SIZE for easier placement:

```c
bitmap_pages = (pmm_bitmap_size + PAGE_SIZE - 1) / PAGE_SIZE;
```

---

## Step 2: Choose bitmap placement

Find first LIMINE_MEMMAP_USABLE region large enough to hold bitmap_pages * PAGE_SIZE.

Algorithm:

For each usable entry:

* Align entry.base up to PAGE_SIZE
* Check if region length >= bitmap_size
* Use this region start as bitmap physical address

Then:

```c
bitmap_phys = chosen_region_base;
pmm_bitmap = phys_to_hhdm(bitmap_phys);
```

Important:

Immediately mark those frames as used in bitmap later.

---

## Step 3: Initialize bitmap

Initial state:

Mark everything as used:

```c
memset(pmm_bitmap, 0xFF, pmm_bitmap_size);
```

---

## Step 4: Free usable memory regions

Iterate Limine map:

For each entry with type == USABLE:

* Compute start_frame
* Compute end_frame
* For each frame:

  * clear bit (mark free)

Then:

Explicitly re-mark bitmap frames themselves as used:

* Determine frame range covering bitmap_phys -> bitmap_phys + bitmap_size
* Set those bits back to 1

This prevents allocator from returning its own bitmap memory.

---

## Step 5: Allocation logic

### pmm_alloc_frame()

Algorithm:

Linear scan:

```c
for frame = 0 .. pmm_frame_count:
  if bit is 0:
    set bit to 1
    decrement free counter
    return frame * PAGE_SIZE
panic if none found
```

Optimizations are not needed now.

---

### pmm_free_frame(phys)

Steps:

* Assert phys is page aligned
* Compute frame index
* Assert bit is currently set
* Clear bit
* Increment free counter

---

## Step 6: Validation test (mandatory)

Add test code in kernel init after pmm_init():

Example:

```c
for i in range(10):
  phys = pmm_alloc_frame()
  virt = phys_to_hhdm(phys)
  *(uint64_t*)virt = 0xCAFEBABECAFEBABE
  ASSERT(*(uint64_t*)virt == 0xCAFEBABECAFEBABE)
```

Print allocated addresses to serial.

---

## Safety checks you must include

Add ASSERTs:

* bitmap pointer not null
* bitmap placement region found
* phys_to_hhdm does not overflow canonical range
* pmm_alloc_frame returns aligned address

These save hours of debugging.

---

## Output information (teaching useful)

Print:

* Total memory detected
* Total frames
* Free frames after init
* Bitmap location physical + virtual

This is good teaching feedback.

---

## Failure modes to watch

If you see:

* Immediate page fault during bitmap write:
  -> HHDM offset wrong or not used

* Allocation returns same address repeatedly:
  -> bitmap marking bug

* Allocator returns kernel memory:
  -> forgot to reserve kernel physical range

---

## Kernel physical range reservation

Important detail:

Limine provides kernel physical base and size via kernel file info request.

You must:

* Mark kernel physical frames as used

If you skip this:

* Allocator will hand out kernel memory later.

So add Limine kernel file request:

* limine_kernel_file_request

Then:

* kernel_phys_base
* kernel_size

Reserve that region explicitly.

---

## Final Prototype 2 acceptance checklist

* Kernel prints memory summary
* Bitmap allocated inside usable memory
* Kernel physical region reserved
* Allocator returns unique aligned frames
* Memory write/read test passes
* No faults

---

