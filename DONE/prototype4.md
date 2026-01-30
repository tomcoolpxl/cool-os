Proceed to Prototype 4: Timer interrupts. Since this is a teaching kernel, start with PIT + PIC. It is simpler, deterministic, and avoids pulling ACPI/APIC into the curriculum immediately. You can migrate to APIC later as a separate prototype.

Below is a complete prototype4.md you can drop into your repo.

```md
# Prototype 4: Timer Interrupts (PIT + PIC) Specification

## Purpose

Add periodic timer interrupts to the x86-64 teaching kernel to introduce asynchronous execution, interrupt masking, and basic interrupt-driven state updates.

This prototype must remain deterministic and debuggable. It must not introduce scheduling yet.

## Scope

In scope:
- Enable hardware IRQ delivery via legacy PIC (8259A)
- Program PIT (8253/8254) to generate periodic interrupts
- Install an IRQ handler for the timer interrupt
- Maintain a global tick counter
- Print a throttled status message to serial
- Properly acknowledge interrupts (EOI)
- Enable interrupts (sti) only after everything is installed

Out of scope:
- APIC/IOAPIC and ACPI parsing
- SMP support
- Preemptive scheduler / context switching
- Sleeping API
- High resolution timers
- Keyboard IRQs

## Dependencies

Required existing components:
- Serial output (COM1)
- panic() and ASSERT macro
- IDT with 256 vectors installed
- ISR/exception machinery and crash reporting
- PMM and heap (for future prototypes; timer itself must not depend on heap correctness)
- cpu.h helpers for halt and basic control register reads

## Design

### Interrupt Model

Use legacy PIC + PIT:

- PIT channel 0 generates IRQ0
- PIC master maps IRQ0..IRQ7 to IDT vectors starting at a configurable base
- Choose a remap base of 0x20 (32) for master PIC and 0x28 (40) for slave PIC
- Timer interrupt vector becomes 0x20

### Vector Layout

- Exceptions remain vectors 0..31
- Hardware IRQs use vectors 32..47 (after PIC remap)

Timer:
- IRQ0 -> vector 32 (0x20)

## Functional Requirements

### 1. PIC Remapping and Masking

Implement PIC initialization that:
- Remaps master PIC to 0x20 and slave PIC to 0x28
- Restores/sets interrupt masks

Masking requirements:
- Unmask only IRQ0 (timer)
- Mask all other IRQ lines initially

EOI requirements:
- Send EOI to master PIC for IRQ0
- For slave IRQs later, send EOI to slave then master (not required yet)

### 2. PIT Programming

Program PIT channel 0 to generate a periodic interrupt.

- PIT input frequency: 1193182 Hz
- Use mode 3 (square wave) or mode 2 (rate generator)
- Choose a teaching-friendly tick rate, e.g. 100 Hz

Divisor:
- divisor = 1193182 / hz
- Write command to port 0x43
- Write divisor low byte then high byte to port 0x40

### 3. IRQ Handler

Add an IRQ handler for vector 0x20 that:
- Increments a global tick counter (uint64_t)
- Optionally prints a message every N ticks (throttled), for example every 100 ticks
- Sends EOI to the PIC before returning
- Returns via iretq through the existing ISR stub mechanism

Re-entrancy:
- IRQ handler must not call panic unless a serious invariant is violated
- Avoid heavy printing; use throttling to keep output readable

### 4. Enable Interrupts Safely

Kernel init ordering must be:

1) serial_init()
2) hhdm_init()
3) idt_init() and ISR stubs installed (already done)
4) pic_init() with remap and masks
5) pit_init(hz)
6) Verify timer vector gate is installed
7) sti
8) Enter idle loop using hlt

Do not execute sti before PIC, PIT, and IDT gate are ready.

### 5. QEMU Behavior

Under QEMU with -no-reboot -no-shutdown:
- Timer interrupts must fire continuously
- Kernel must remain responsive (not locked up)
- No unexpected exceptions should occur
- Printing should not flood output

## Implementation Requirements

### Files to Add

- include/ports.h
  - inb/outb/io_wait helpers

- include/pic.h
  - pic_init()
  - pic_send_eoi(uint8_t irq)
  - pic_set_mask(uint8_t irq)
  - pic_clear_mask(uint8_t irq)

- src/pic.c
  - PIC remap sequence and masks

- include/pit.h
  - pit_init(uint32_t hz)

- src/pit.c
  - PIT programming

- src/irq.c (or src/timer.c)
  - timer IRQ handler logic
  - tick counter

You may also extend existing ISR infrastructure to register handlers per vector.

### Ports

PIC:
- Master command: 0x20
- Master data: 0x21
- Slave command: 0xA0
- Slave data: 0xA1

PIT:
- Command: 0x43
- Channel 0 data: 0x40

### PIC Remap Sequence (Required)

The initialization control words must be sent in the standard order:

- ICW1: start init, expect ICW4
- ICW2: vector offset
- ICW3: wiring info
- ICW4: 8086 mode

Then set masks.

### IRQ Handler Registration

Minimum acceptable approach:
- Hardwire vector 0x20 stub to call timer handler

Preferred approach:
- Provide an array of function pointers (handlers[256])
- Common interrupt handler dispatches based on vector number

## Tests and Validation

### Required Runtime Validation

1) Boot message:
- Print "Timer: enabling PIT at <hz> Hz"

2) Tick output:
- Print tick count every 100 ticks (or every 1 second at 100 Hz)

Example output:
- "tick=100"
- "tick=200"
- ...

3) Stability:
- Run for at least several seconds under QEMU without faults

### Negative Test (Optional)

Deliberately mask IRQ0 and verify tick stops incrementing, then unmask and verify it resumes.

## Acceptance Criteria

Prototype 4 is complete when:
- PIC is remapped to 0x20/0x28
- Only IRQ0 is unmasked
- PIT generates periodic interrupts at configured rate
- Vector 0x20 handler increments tick counter
- EOI is sent correctly (no interrupt storm lockup)
- Kernel runs in an idle loop with sti+hlt and continues receiving ticks
- Output is throttled and readable
- No unexpected exceptions or ASSERT failures occur


If you want the cleanest teaching story, keep the handler dispatch model minimal now (hardwire vector 0x20) and refactor to a general IRQ dispatcher only when you add keyboard/network.
