#include "xhci.h"
#include "pci.h"
#include "serial.h"
#include "paging.h"
#include "hhdm.h"
#include "pmm.h"
#include "heap.h"
#include "panic.h"

/* Helper for MMIO access */
static inline uint32_t mmio_read32(uint64_t base, uint32_t offset) {
    return *(volatile uint32_t *)(base + offset);
}

static inline void mmio_write32(uint64_t base, uint32_t offset, uint32_t val) {
    *(volatile uint32_t *)(base + offset) = val;
}

static inline uint64_t mmio_read64(uint64_t base, uint32_t offset) {
    return *(volatile uint64_t *)(base + offset);
}

static inline void mmio_write64(uint64_t base, uint32_t offset, uint64_t val) {
    *(volatile uint64_t *)(base + offset) = val;
}

void xhci_init(uint8_t bus, uint8_t device, uint8_t function) {
    serial_puts("XHCI: Initializing...\n");

    /* 1. Get MMIO Base Address from BAR0 */
    uint32_t bar0 = pci_read_config_32(bus, device, function, PCI_OFFSET_BAR0);
    uint32_t bar1 = pci_read_config_32(bus, device, function, PCI_OFFSET_BAR1);
    
    /* Check if it's 64-bit BAR */
    uint64_t mmio_phys = bar0 & 0xFFFFFFF0;
    if ((bar0 & 0x6) == 0x4) {
        mmio_phys |= ((uint64_t)bar1 << 32);
    }
    
    serial_puts("XHCI: BAR0 Physical Address: ");
    serial_print_hex(mmio_phys);
    serial_puts("\n");

    /* Map 64KB for registers at a high kernel address */
    uint64_t mmio_virt = 0xFFFFFFA000000000;
    for (int i = 0; i < 16; i++) {
        paging_map_page(mmio_virt + (i * 4096), mmio_phys + (i * 4096), PTE_PRESENT | PTE_WRITABLE | PTE_CACHE_DIS);
    }
    
    serial_puts("XHCI: Mapped to Virtual Address: ");
    serial_print_hex(mmio_virt);
    serial_puts("\n");
    
    /* 2. Read Capability Registers */
    uint32_t caps_0 = mmio_read32(mmio_virt, XHCI_CAP_CAPLENGTH);
    uint8_t cap_length = caps_0 & 0xFF;
    uint16_t hci_version = (caps_0 >> 16) & 0xFFFF;
    
    uint32_t hcs_params1 = mmio_read32(mmio_virt, XHCI_CAP_HCSPARAMS1);
    
    serial_puts("XHCI: CapLength: ");
    serial_print_dec(cap_length);
    serial_puts(" Version: ");
    serial_print_hex(hci_version);
    serial_puts("\n");
    
    uint8_t max_slots = hcs_params1 & 0xFF;
    uint16_t max_intrs = (hcs_params1 >> 8) & 0x7FF;
    uint8_t max_ports = (hcs_params1 >> 24) & 0xFF;
    
    serial_puts("XHCI: MaxSlots: ");
    serial_print_dec(max_slots);
    serial_puts(" MaxIntrs: ");
    serial_print_dec(max_intrs);
    serial_puts(" MaxPorts: ");
    serial_print_dec(max_ports);
    serial_puts("\n");
    
    /* 3. Operational Registers */
    uint64_t op_base = mmio_virt + cap_length;
    
    /* Reset Controller */
    uint32_t usbcmd = mmio_read32(op_base, XHCI_OP_USBCMD);
    serial_puts("XHCI: Resetting controller...\n");
    mmio_write32(op_base, XHCI_OP_USBCMD, usbcmd | XHCI_CMD_RESET);
    
    /* Wait for reset to complete (HCRST bit clears) */
    while (mmio_read32(op_base, XHCI_OP_USBCMD) & XHCI_CMD_RESET) {
        asm volatile("pause");
    }
    serial_puts("XHCI: Reset complete.\n");
    
    /* Wait for Controller Not Ready (CNR) to clear */
    while (mmio_read32(op_base, XHCI_OP_USBSTS) & XHCI_STS_CNR) {
        asm volatile("pause");
    }
    serial_puts("XHCI: Controller Ready.\n");
    
    /* Check ports for connection */
    /* Port registers are at op_base + 0x400 + (port_num-1)*0x10 */
    /* wait, op_base + 0x400 is strictly only if RTSOFF/DBOFF are standard? */
    /* We should check capabilities properly, but for standard xHCI 0x400 is typical offset for Port Regs set */
    /* Actually Port Register Set is at offset 0x400 from OP base usually, but let's check standard. */
    /* It is NOT fixed. It is specified in Supported Protocol Capability usually or just implied? */
    /* The spec says: PORTSC are in the Operational Register Space. */
    /* They start at Offset 0x400. */
    
    uint64_t port_base = op_base + 0x400;
    
    for (int i = 1; i <= max_ports; i++) {
        uint64_t port_reg = port_base + (i - 1) * 0x10;
        uint32_t portsc = mmio_read32(port_reg, 0); /* Offset 0 of port struct is PORTSC */
        
        if (portsc & XHCI_PORTSC_CCS) {
            serial_puts("XHCI: Port ");
            serial_print_dec(i);
            serial_puts(" connected!\n");
        }
    }
    
    /* 
     * TODO: Next steps for USB Keyboard Support:
     * 1. Initialize Device Context Base Address Array (DCBAA).
     * 2. Initialize Command Ring (CRCR).
     * 3. Initialize Event Ring (ERSTBA, ERDP).
     * 4. Enable Slots and Address Devices.
     * 5. Set up Interrupts (MSI or Legacy).
     * 6. Implement USB HID Class Driver to parse keyboard reports.
     */
}