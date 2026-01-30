# Prototype 12: USB XHCI Technical Retrospective

**Date:** 2026-01-30
**Component:** USB XHCI Input Stack
**Outcome:** Native Driver Disabled; Reverted to BIOS Legacy Emulation.

## 1. The "Silent Failure" of Interrupts/Events

The native driver implementation (`src/xhci.c`) successfully performed the heavy lifting of initialization:
- **PCI Enumeration:** Correctly identified the controller (Class 0x0C, Subclass 0x03, ProgIF 0x30).
- **MMIO Mapping:** Correctly mapped the 64-bit BAR to virtual memory.
- **Ring Allocation:** allocated and initialized Command, Event, and Transfer rings.
- **BIOS Handoff:** Implemented the correct semaphore exchange via `HCCPARAMS1` Extended Capabilities to request OS ownership.
- **Command Execution:** Successfully sent `NO_OP`, `ENABLE_SLOT`, `ADDRESS_DEVICE`, and `CONFIGURE_ENDPOINT` commands, which the controller acknowledged with "Success" completion codes.

**However, the driver never received data packets (Transfer Events) from the keyboard.** The Transfer Ring remained silent despite valid transfers being queued.

### Likely Technical Root Causes

Based on the behavior observed (successful init, silent data path), the failure stems from one of three specific complexities:

#### A. Firmware "Theft" (Legacy Emulation Persistence)
In the QEMU/OVMF environment, the firmware puts the USB controller into "Legacy Mode" to emulate a PS/2 keyboard for bootloaders.
- **The Issue:** Even though the driver implemented the BIOS Handoff (checking `USB Legacy Support` capability and toggling the OS Owned semaphore), the specific combination of QEMU's `usb-kbd` device and the OVMF firmware likely did not fully release the device state.
- **The Result:** The firmware continued to intercept the physical USB signals to feed its emulated PS/2 mechanism. This "starved" the native XHCI ring of events, as the controller cannot deliver the same packet to both the Legacy Logic and the OS Ring simultaneously.

#### B. The "Boot Protocol" vs. "Report Protocol" Mismatch
USB keyboards operate in two distinct modes:
1.  **Boot Protocol:** A simplified 8-byte format required by BIOSes (and our driver expectation).
2.  **Report Protocol:** The full, descriptor-based HID format.
- **The Issue:** Our driver performed `SET_CONFIGURATION` and `CONFIGURE_ENDPOINT`, but it did not explicitly send the specific HID Class Request (`SET_PROTOCOL`) to force the keyboard into the Boot Protocol.
- **The Result:** The device may have remained in a state where it expected complex Report Protocol handshake or descriptors, or the firmware forced it into a specific mode that our simple "read 8 bytes" Transfer Request (TRB) did not satisfy.

#### C. MSI-X Misconfiguration
The driver attempted to enable MSI-X interrupts (Vector 0x40).
- **The Issue:** If the Local APIC (LAPIC) was not perfectly configured to receive that specific vector, or if QEMU's emulated MSI-X routing (which bypasses the IOAPIC) did not match our expectation, the CPU never received the "doorbell" interrupt indicating data was waiting.
- **The Result:** Even if the controller wrote data to the Event Ring, the CPU never woke up to process it.

## 2. The Mechanics of the PS/2 Fallback (Why it worked)

The test ultimately passed when we switched back to the standard PS/2 driver (`src/kbd.c`) because we leveraged the PC Platform's **System Management Mode (SMM)**.

The flow that enables this "Magic" is:
1.  **Input:** A key is pressed on the real USB keyboard (or injected via QMP).
2.  **Hardware:** The USB Controller hardware receives the raw signal.
3.  **Trap:** The BIOS/Firmware (OVMF) has configured the controller to trigger a hidden System Management Interrupt (SMI) on key events.
4.  **Emulation:** The CPU pauses the OS, enters SMM, and executes a hidden BIOS routine. This routine reads the USB packet, translates it to a PS/2 Scancode (Set 1), and manually writes it to the legacy I/O Port `0x60`.
5.  **IRQ Injection:** The write to Port `0x60` triggers the legacy Interrupt Controller (PIC) to fire **IRQ1**.
6.  **OS Processing:** Our kernel (`kbd.c`), oblivious to the USB drama, receives IRQ1, reads Port `0x60`, and sees a valid keystroke.

## 3. Future Requirements for Native XHCI

To make the native XHCI driver work in a future prototype, we must:
1.  **Aggressively Disable Legacy Support:** Implement the full USB Legacy Support Capability register handshake to forcibly disable SMIs (`USBLEGCTLSTS` register), ensuring the firmware stops stealing events.
2.  **Implement HID Protocol:** Send the correct `SET_PROTOCOL` control commands to the device during enumeration.
3.  **Hub Support:** Most ports (even on emulators) are technically behind a Root Hub. A robust driver needs to enumerate the Root Hub ports and handle Hub Status Change events before attaching the keyboard device.

## Conclusion
For Prototype 12, allowing the BIOS to perform the USB-to-PS/2 translation via SMM is the correct architectural decision. It adheres to the design philosophy of "Simplicity over Abstraction" by avoiding the massive complexity of a compliant USB stack at this early stage.
