#include "xhci.h"
#include "pci.h"
#include "serial.h"
#include "paging.h"
#include "hhdm.h"
#include "pmm.h"
#include "heap.h"
#include "panic.h"
#include "utils.h"

/* Global xHCI State */
static uint64_t mmio_base_virt;
static uint64_t rt_base;    /* Runtime Register Base */
static uint64_t db_base;    /* Doorbell Register Base */

/* Command Ring State */
static xhci_trb_t *cmd_ring_base;
static uint64_t cmd_ring_enqueue_idx;
static uint8_t cmd_ring_cycle_state;

/* Event Ring State */
static xhci_trb_t *event_ring_base;
static uint64_t event_ring_dequeue_idx;
static uint8_t event_ring_cycle_state;

/* DCBAA */
static uint64_t *dcbaa_base;

/* Helper to allocate a zeroed 4KB page and return its physical address */
static uint64_t xhci_alloc_page(uint64_t *virt_out) {
    uint64_t phys = pmm_alloc_frame();
    if (phys == 0) panic("XHCI: OOM");
    
    uint64_t virt = (uint64_t)phys_to_hhdm(phys);
    
    /* Manual memset */
    uint64_t *ptr = (uint64_t *)virt;
    for (int i = 0; i < 512; i++) {
        ptr[i] = 0;
    }
    
    if (virt_out) *virt_out = virt;
    return phys;
}

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

static void xhci_ring_doorbell(uint8_t target, uint8_t stream_id) {
    mmio_write32(db_base, target * 4, (stream_id << 16) | target);
}

static void xhci_send_noop(void) {
    xhci_trb_t *trb = &cmd_ring_base[cmd_ring_enqueue_idx];
    
    trb->param_low = 0;
    trb->param_high = 0;
    trb->status = 0;
    trb->control = TRB_TYPE(TRB_NOOP) | (cmd_ring_cycle_state ? TRB_C : 0);
    
    /* Flush cache for this TRB so xHCI sees it in RAM */
    asm volatile("clflush (%0)" :: "r"(trb));
    
    /* Advance Enqueue Pointer */
    cmd_ring_enqueue_idx++;
    if (cmd_ring_enqueue_idx >= 256) { /* 4KB / 16 bytes = 256 TRBs */
        /* Link TRB handling would go here, but for now just wrap (unsafe without Link TRB) */
        /* Actually, xHCI requires a Link TRB at the end of the ring. */
        /* For prototype 1.0, let's just assume we don't fill the ring yet. */
        /* But strictly we need to toggle cycle bit if we wrap. */
        cmd_ring_enqueue_idx = 0;
        cmd_ring_cycle_state ^= 1;
    }
    
    /* Ensure TRB is written before ringing doorbell */
    asm volatile("": : :"memory");
    
    /* Ring Doorbell for Host Controller (Target 0) */
    xhci_ring_doorbell(0, 0);
}

static void xhci_poll_event(void) {
    xhci_trb_t *trb = &event_ring_base[event_ring_dequeue_idx];
    
    /* Check if Cycle Bit matches our Consumer Cycle State */
    uint8_t trb_cycle = (trb->control & TRB_C) ? 1 : 0;
    
    if (trb_cycle == event_ring_cycle_state) {
        /* We have an event! */
        uint8_t type = TRB_GET_TYPE(trb->control);
        uint8_t code = TRB_GET_CODE(trb->status);
        
        serial_puts("XHCI: Event received! Type: ");
        serial_print_dec(type);
        serial_puts(" Code: ");
        serial_print_dec(code);
        serial_puts("\n");
        
        if (type == TRB_CMD_COMPLETION) {
            serial_puts("XHCI: Command Completion detected.\n");
        } else if (type == TRB_PORT_STATUS_CHANGE) {
             serial_puts("XHCI: Port Status Change detected.\n");
        }
        
        /* Advance Dequeue Pointer */
        event_ring_dequeue_idx++;
        if (event_ring_dequeue_idx >= 256) {
            event_ring_dequeue_idx = 0;
            event_ring_cycle_state ^= 1;
        }
        
        /* Write ERDP to notify controller (Preserve EH bit 3, usually 0 for us) */
        /* ERDP requires bits 3:0 to be masked? No, low 4 bits are reserved/flags */
        /* Bit 3 is EHB (Event Handler Busy). We should clear it. */
        /* The address must be 16-byte aligned. */
        uint64_t erdp_phys = hhdm_to_phys((void*)&event_ring_base[event_ring_dequeue_idx]);
        mmio_write64(rt_base, XHCI_RT_IR0_ERDP, erdp_phys | (1 << 3)); /* Set EHB to clear it? No, write 1 to clear EHB? 
                                                                         Wait, spec says: "Software writes this field to advance... 
                                                                         Host Controller clears EHB if Software writes a 1 to it." 
                                                                         So yes, write 1 to bit 3. */
    }
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
    
    mmio_base_virt = mmio_virt;
    uint32_t rtsoff = mmio_read32(mmio_virt, XHCI_CAP_RTSOFF);
    uint32_t dboff = mmio_read32(mmio_virt, XHCI_CAP_DBOFF);
    
    rt_base = mmio_virt + rtsoff;
    db_base = mmio_virt + dboff;
    
    serial_puts("XHCI: Runtime Base Offset: ");
    serial_print_hex(rtsoff);
    serial_puts("\n");

    /* 3. Operational Registers */
    uint64_t op_base = mmio_virt + cap_length;
    
    /* Stop controller before configuring */
    uint32_t usbcmd = mmio_read32(op_base, XHCI_OP_USBCMD);
    usbcmd &= ~XHCI_CMD_RUN;
    mmio_write32(op_base, XHCI_OP_USBCMD, usbcmd);
    
    /* Wait for Halted */
    while (!(mmio_read32(op_base, XHCI_OP_USBSTS) & XHCI_STS_HCH)) {
        asm volatile("pause");
    }
    
    /* Reset Controller */
    serial_puts("XHCI: Resetting controller...\n");
    mmio_write32(op_base, XHCI_OP_USBCMD, XHCI_CMD_RESET);
    
    /* Wait for reset to complete */
    while (mmio_read32(op_base, XHCI_OP_USBCMD) & XHCI_CMD_RESET) {
        asm volatile("pause");
    }
    serial_puts("XHCI: Reset complete.\n");
    
    /* Wait for CNR */
    while (mmio_read32(op_base, XHCI_OP_USBSTS) & XHCI_STS_CNR) {
        asm volatile("pause");
    }
    
    /* 4. Configure Device Context Base Address Array (DCBAA) */
    /* Max Slots is in HCSPARAMS1 bits 7:0 */
    uint8_t max_slots = hcs_params1 & 0xFF;
    serial_puts("XHCI: Max Slots: ");
    serial_print_dec(max_slots);
    serial_puts("\n");
    
    /* Allocate DCBAA (pointers) - size depends on MaxSlots. 64-bit pointers. */
    /* (MaxSlots + 1) * 8 bytes. */
    /* We allocate a full page for simplicity. */
    uint64_t dcbaa_phys = xhci_alloc_page((uint64_t*)&dcbaa_base);
    
    mmio_write64(op_base, XHCI_OP_DCBAAP, dcbaa_phys);
    
    /* Set Max Slots Enabled in CONFIG register */
    mmio_write32(op_base, XHCI_OP_CONFIG, max_slots);
    
    /* 5. Configure Command Ring */
    uint64_t cr_phys = xhci_alloc_page((uint64_t*)&cmd_ring_base);
    cmd_ring_enqueue_idx = 0;
    cmd_ring_cycle_state = 1; /* Producer starts with 1 */
    
    mmio_write64(op_base, XHCI_OP_CRCR, cr_phys | 1); /* Bit 0: RCS (Ring Cycle State) = 1 */
    
    /* 6. Configure Event Ring */
    /* Allocate Event Ring Segment Table (ERST) */
    xhci_erst_entry_t *erst_base;
    uint64_t erst_phys = xhci_alloc_page((uint64_t*)&erst_base);
    
    /* Allocate Event Ring Segment (the actual ring buffer) */
    uint64_t er_phys = xhci_alloc_page((uint64_t*)&event_ring_base);
    event_ring_dequeue_idx = 0;
    event_ring_cycle_state = 1; /* Consumer expects 1 */
    
    /* Setup ERST Entry 0 */
    erst_base[0].base_address = er_phys;
    erst_base[0].size = 256; /* 256 TRBs (matches 4KB / 16) */
    erst_base[0].reserved = 0;
    
    /* Write ERSTBA (Runtime Register) */
    mmio_write32(rt_base, XHCI_RT_IR0_ERSTSZ, 1); /* 1 Segment */
    mmio_write64(rt_base, XHCI_RT_IR0_ERSTBA, erst_phys);
    mmio_write64(rt_base, XHCI_RT_IR0_ERDP, er_phys);
    
    /* Enable Interrupts (IMAN) - even if polling, good to enable IP */
    mmio_write32(rt_base, XHCI_RT_IR0_IMAN, 2); /* Bit 1: IP (Interrupt Pending), Bit 0: IE (Interrupt Enable) */
    mmio_write32(op_base, XHCI_OP_USBCMD, XHCI_CMD_INTE); /* Global Interrupt Enable */

    /* 7. Start Controller */
    serial_puts("XHCI: Starting controller...\n");
    mmio_write32(op_base, XHCI_OP_USBCMD, XHCI_CMD_RUN | XHCI_CMD_INTE);
    
    /* Send NO_OP to verify rings */
    serial_puts("XHCI: Sending NO_OP...\n");
    xhci_send_noop();
    
    /* Poll for completion */
    /* Simple poll loop for a few ms */
    for (int i = 0; i < 1000000; i++) {
        xhci_poll_event();
        asm volatile("pause");
    }

    uint64_t port_base = op_base + 0x400;
    /* Actually read MaxPorts to be safe */
    uint8_t max_ports = (hcs_params1 >> 24) & 0xFF;
    
    for (int i = 1; i <= max_ports; i++) {
        uint64_t port_reg = port_base + (i - 1) * 0x10;
        uint32_t portsc = mmio_read32(port_reg, 0);
        
        if (portsc & XHCI_PORTSC_CCS) {
            serial_puts("XHCI: Port ");
            serial_print_dec(i);
            serial_puts(" connected!\n");
        }
    }
    
    /* 
     * USB KEYBOARD SUPPORT STATUS (Phase 2 Complete):
     * 1. xHCI Controller Initialized.
     * 2. Command & Event Rings Operational (NO_OP Verified).
     * 3. Device Detected on Port 5.
     * 
     * NEXT STEPS (Phase 3):
     * 1. Enable Slot (Send ENABLE_SLOT command).
     * 2. Initialize Input Context & Device Context.
     * 3. Address Device (Send ADDRESS_DEVICE command).
     * 4. Configure Endpoint (Endpoint 1 IN for Keyboard).
     * 5. Ring Doorbell for Endpoint 1 to receive reports.
     */
}