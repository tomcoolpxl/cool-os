# USB Debugging & Stabilization Results

**Date:** 2026-01-30
**Status:** Resolved (via Fallback Strategy)

## Summary
The investigation into the non-functional USB XHCI driver revealed significant complexities with QEMU's USB Legacy Emulation and XHCI Hand-off mechanisms when running under OVMF (UEFI).

While a correct BIOS-to-OS Handoff mechanism was implemented in `src/xhci.c`, testing showed that QEMU's emulation behavior in this specific configuration (OVMF + XHCI) remained unstable for legacy input.

## Resolution
As per the "Step Back" strategy outlined in the Debug Plan, we have opted to **disable the native XHCI driver** by default and rely on the industry-standard **PS/2 Legacy Emulation** provided by the firmware.

This decision aligns with the project's educational goals (Simplicity over Abstraction) and the original Requirements for Prototype 12.

## Implementation Details
1.  **Driver Code:** The native XHCI driver (`src/xhci.c`) and BIOS Handoff logic are preserved in the codebase for future reference but are currently disabled.
2.  **Kernel Init:** `pci_init()` (which triggers XHCI discovery) has been commented out in `src/kernel.c`.
3.  **Keyboard Driver:** `src/kbd.c` (PS/2) is the active input driver. It successfully handles input from standard keyboards and USB keyboards (via BIOS legacy emulation).
4.  **Testing:** `tests/input_test.py` has been updated to use a robust, non-blocking I/O loop and validates the PS/2 input path using QEMU's QMP interface. The test passes consistently.

## Next Steps
- Proceed to **Prototype 13 (Minimal Shell)**, utilizing the working `kbd_readline()` input.
- Re-visit Native USB support in a much later prototype (e.g., after full preemptive multitasking and PCI refactoring).