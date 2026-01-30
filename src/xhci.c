#include "xhci.h"
#include "pci.h"
#include "serial.h"
#include "paging.h"
#include "hhdm.h"
#include "pmm.h"
#include "heap.h"
#include "panic.h"
#include "utils.h"
#include <stddef.h>

#include "kbd.h"

/* Global xHCI State */
static uint64_t mmio_base_virt;
static uint64_t rt_base;    /* Runtime Register Base */
static uint64_t db_base;    /* Doorbell Register Base */

/* HID to Scancode Set 1 Translation Table (Partial) */
static const uint8_t hid_to_scancode[] = {
    0, 0, 0, 0,
    30, 48, 46, 32, 18, 33, 34, 35, 23, 36, 37, 38, /* A-M */
    50, 49, 24, 25, 16, 19, 31, 20, 22, 47, 17, 45, /* N-Z */
    2, 3, 4, 5, 6, 7, 8, 9, 10, 11, /* 1-0 */
    28, 1, 14, 15, 57, 12, 13, 26, 27, 43, /* Enter, Esc, Bksp, Tab, Space, -, =, [, ], \ */
    39, 40, 41, 51, 52, 53, 58, 59, 60, 61, /* ;, ', `, ,, ., /, Caps, F1, F2, F3 */
    62, 63, 64, 65, 66, 67, 68, 87, 88,     /* F4-F12 */
};

static uint8_t last_report[8] = {0};

static void xhci_handle_keyboard_report(uint8_t *report) {
    /* Check for key presses (present in new, not in old) */
    for (int i = 2; i < 8; i++) {
        uint8_t key = report[i];
        if (key == 0) continue;
        
        /* Check if it was present before */
        int present = 0;
        for (int j = 2; j < 8; j++) {
            if (last_report[j] == key) {
                present = 1;
                break;
            }
        }
        
        if (!present) {
            /* Key Pressed */
            if (key < sizeof(hid_to_scancode)) {
                uint8_t sc = hid_to_scancode[key];
                if (sc) {
                    serial_puts("XHCI: Key Pressed Scancode: ");
                    serial_print_hex(sc);
                    serial_puts("\n");
                    kbd_process_scancode(sc, 1);
                }
            }
        }
    }
    
    /* Check for key releases (present in old, not in new) */
    for (int i = 2; i < 8; i++) {
        uint8_t key = last_report[i];
        if (key == 0) continue;
        
        int present = 0;
        for (int j = 2; j < 8; j++) {
            if (report[j] == key) {
                present = 1;
                break;
            }
        }
        
        if (!present) {
            /* Key Released */
            if (key < sizeof(hid_to_scancode)) {
                uint8_t sc = hid_to_scancode[key];
                if (sc) kbd_process_scancode(sc, 0);
            }
        }
    }
    
    /* Handle Modifiers (Byte 0) */
    /* This is simplified. Ideally map modifier bits to scancodes. */
    /* LCtrl (0), LShift (1), LAlt (2), LGui (3), RCtrl (4), RShift (5), RAlt (6), RGui (7) */
    /* Check Shifts */
    if ((report[0] & 2) && !(last_report[0] & 2)) kbd_process_scancode(0x2A, 1); /* LShift Press */
    if (!(report[0] & 2) && (last_report[0] & 2)) kbd_process_scancode(0x2A, 0); /* LShift Release */
    if ((report[0] & 32) && !(last_report[0] & 32)) kbd_process_scancode(0x36, 1); /* RShift Press */
    if (!(report[0] & 32) && (last_report[0] & 32)) kbd_process_scancode(0x36, 0); /* RShift Release */
    
    /* Check Ctrls */
    if ((report[0] & 1) && !(last_report[0] & 1)) kbd_process_scancode(0x1D, 1); /* LCtrl Press */
    if (!(report[0] & 1) && (last_report[0] & 1)) kbd_process_scancode(0x1D, 0); /* LCtrl Release */
    if ((report[0] & 16) && !(last_report[0] & 16)) kbd_process_scancode(0x1D, 1); /* RCtrl Press (Mapping both to LCtrl for now) */
    if (!(report[0] & 16) && (last_report[0] & 16)) kbd_process_scancode(0x1D, 0); /* RCtrl Release */
    
    /* Update last report */
    for (int i = 0; i < 8; i++) last_report[i] = report[i];
}

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

/* Keyboard Buffer */
static uint64_t kbd_buf_virt;
static uint64_t kbd_buf_phys;

static xhci_trb_t* xhci_wait_for_event(uint32_t type);

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

static void xhci_send_command(uint32_t type, uint32_t param_low, uint32_t param_high, uint32_t control_flags) {
    xhci_trb_t *trb = &cmd_ring_base[cmd_ring_enqueue_idx];
    
    trb->param_low = param_low;
    trb->param_high = param_high;
    trb->status = 0;
    trb->control = TRB_TYPE(type) | (cmd_ring_cycle_state ? TRB_C : 0) | control_flags;
    
    /* Flush cache for this TRB so xHCI sees it in RAM */
    asm volatile("clflush (%0)" :: "r"(trb));
    
    /* Advance Enqueue Pointer */
    cmd_ring_enqueue_idx++;
    if (cmd_ring_enqueue_idx >= 256) { 
        cmd_ring_enqueue_idx = 0;
        cmd_ring_cycle_state ^= 1;
    }
    
    /* Ensure TRB is written before ringing doorbell */
    asm volatile("": : :"memory");
    
    /* Ring Doorbell for Host Controller (Target 0) */
    xhci_ring_doorbell(0, 0);
}

static int xhci_enable_slot(void) {
    xhci_send_command(TRB_ENABLE_SLOT, 0, 0, 0);
    xhci_trb_t *ev = xhci_wait_for_event(TRB_CMD_COMPLETION);
    if (!ev) {
        serial_puts("XHCI: Enable Slot timed out\n");
        return -1;
    }
    
    uint8_t code = TRB_GET_CODE(ev->status);
    if (code != 1) {
        serial_puts("XHCI: Enable Slot failed. Code: ");
        serial_print_dec(code);
        serial_puts("\n");
        return -1;
    }
    
    uint8_t slot_id = (ev->control >> 24) & 0xFF;
    return slot_id;
}

/* EP0 Ring State (for Slot 1) */
static xhci_trb_t *ep0_ring_base;
static uint64_t ep0_ring_enqueue_idx;
static uint8_t ep0_ring_cycle_state;

static int xhci_address_device(uint8_t slot_id, uint8_t root_port, uint8_t speed) {
    /* 1. Allocate Output Device Context */
    /* Size is determined by 64-byte context size bit in HCCPARAMS1, assuming 32 for now */
    /* 32 bytes * 32 contexts = 1024 bytes. 4KB page is enough. */
    uint64_t out_ctx_phys = xhci_alloc_page(NULL);
    dcbaa_base[slot_id] = out_ctx_phys;
    
    /* 2. Allocate Input Context */
    /* Input context has an extra Control Context at the beginning. */
    /* So it is 33 contexts? No, it's Drop+Add flags + 32 contexts. */
    xhci_input_ctx_t *in_ctx;
    uint64_t in_ctx_phys = xhci_alloc_page((uint64_t*)&in_ctx);
    
    /* 3. Setup Input Control Context */
    in_ctx->add_flags = (1 << 0) | (1 << 1); /* A0 (Slot) and A1 (EP0) */
    in_ctx->drop_flags = 0;
    
    /* 4. Setup Slot Context */
    in_ctx->slot_ctx.info1 = SLOT_CTX_ENTRIES(1) | SLOT_CTX_SPEED(speed) | SLOT_CTX_ROUTE(0);
    in_ctx->slot_ctx.info2 = SLOT_CTX_ROOT_PORT(root_port);
    in_ctx->slot_ctx.state = 0;
    
    /* 5. Setup Endpoint 0 Context (Index 1 in array, but EP0 is Context 1) */
    /* Context Index 0 = Slot Context */
    /* Context Index 1 = EP0 Context */
    /* Context Index 2 = EP1 OUT */
    /* ... */
    
    /* Allocate Transfer Ring for EP0 */
    uint64_t ep0_ring_phys = xhci_alloc_page((uint64_t*)&ep0_ring_base);
    ep0_ring_enqueue_idx = 0;
    ep0_ring_cycle_state = 1;
    
    in_ctx->ep_ctx[1].info1 = EP_CTX_CERR(3);
    in_ctx->ep_ctx[1].info2 = EP_CTX_TYPE(EP_TYPE_CONTROL) | EP_CTX_MAX_P_SIZE(64); /* Assume 64 bytes for EP0 */
    in_ctx->ep_ctx[1].tr_dequeue = ep0_ring_phys | 1; /* DCS = 1 */
    in_ctx->ep_ctx[1].avg_trb_len = 8;
    
    /* 6. Send Address Device Command */
    xhci_send_command(TRB_ADDRESS_DEVICE, in_ctx_phys, 0, (slot_id << 24));
    
    xhci_trb_t *ev = xhci_wait_for_event(TRB_CMD_COMPLETION);
    if (!ev) {
        serial_puts("XHCI: Address Device timed out\n");
        return -1;
    }
    
    uint8_t code = TRB_GET_CODE(ev->status);
    if (code != 1) {
        serial_puts("XHCI: Address Device failed. Code: ");
        serial_print_dec(code);
        serial_puts("\n");
        return -1;
    }
    
    serial_puts("XHCI: Device Addressed successfully! Slot: ");
    serial_print_dec(slot_id);
    serial_puts("\n");
    return 0;
}

static void xhci_ring_ep_doorbell(uint8_t slot, uint8_t target);

static int xhci_send_control_transfer(uint8_t slot_id, uint8_t req_type, uint8_t req, uint16_t val, uint16_t idx, uint16_t len) {
    /* Setup Stage TRB */
    xhci_trb_t *setup = &ep0_ring_base[ep0_ring_enqueue_idx];
    
    setup->param_low = req_type | (req << 8) | (val << 16);
    setup->param_high = idx | (len << 16);
    setup->status = 8; /* Length of Setup Packet is always 8 */
    setup->control = TRB_TYPE(TRB_SETUP_STAGE) | 
                     (ep0_ring_cycle_state ? TRB_C : 0) | 
                     TRB_IDT; /* Immediate Data */
                     
    /* If len > 0, we need Data Stage. For SetConfiguration (len=0), it's No Data. */
    if (len == 0) {
        setup->control |= (0 << 16); /* TRT = 0 (No Data Stage) */
    } else {
        /* Assume IN for now */
        setup->control |= (3 << 16); /* TRT = 3 (IN Data Stage) */
    }
    
    ep0_ring_enqueue_idx++;
    if (ep0_ring_enqueue_idx >= 256) {
        ep0_ring_enqueue_idx = 0;
        ep0_ring_cycle_state ^= 1;
    }
    
    /* Status Stage TRB */
    xhci_trb_t *status = &ep0_ring_base[ep0_ring_enqueue_idx];
    status->param_low = 0;
    status->param_high = 0;
    status->status = 0;
    status->control = TRB_TYPE(TRB_STATUS_STAGE) |
                      (ep0_ring_cycle_state ? TRB_C : 0) |
                      TRB_IOC | /* Interrupt on Completion */
                      (1 << 16); /* DIR = 1 (IN) for Status Stage of OUT (No Data) transfer */
                      
    /* If No Data Stage, Status Stage direction is IN */
    
    asm volatile("clflush (%0)" :: "r"(setup));
    asm volatile("clflush (%0)" :: "r"(status));
    
    ep0_ring_enqueue_idx++;
    if (ep0_ring_enqueue_idx >= 256) {
        ep0_ring_enqueue_idx = 0;
        ep0_ring_cycle_state ^= 1;
    }
    
    asm volatile("":::"memory");
    /* Ring Doorbell for Slot 1, Endpoint 0 (Target = 1) */
    xhci_ring_ep_doorbell(slot_id, 1);
    
    /* Wait for completion */
    xhci_trb_t *ev = xhci_wait_for_event(32); /* Transfer Event */
    if (!ev || TRB_GET_CODE(ev->status) != 1) {
        serial_puts("XHCI: Control Transfer Failed. Code: ");
        if (ev) serial_print_dec(TRB_GET_CODE(ev->status));
        else serial_puts("Timeout");
        serial_puts("\n");
        return -1;
    }
    return 0;
}

static xhci_trb_t *ep1_ring_base;
static uint64_t ep1_ring_enqueue_idx;
static uint8_t ep1_ring_cycle_state;

static int xhci_configure_endpoint(uint8_t slot_id, uint8_t root_port, uint8_t speed) {
    /* 1. Allocate Input Context */
    xhci_input_ctx_t *in_ctx;
    uint64_t in_ctx_phys = xhci_alloc_page((uint64_t*)&in_ctx);
    
    /* 2. Setup Input Control Context */
    in_ctx->add_flags = (1 << 3) | (1 << 0); /* A3 (EP1 IN) and A0 (Slot Context) */
    in_ctx->drop_flags = 0;
    
    /* 3. Setup Slot Context */
    /* Must replicate existing Slot Context values */
    in_ctx->slot_ctx.info1 = SLOT_CTX_ENTRIES(3) | SLOT_CTX_SPEED(speed) | SLOT_CTX_ROUTE(0);
    in_ctx->slot_ctx.info2 = SLOT_CTX_ROOT_PORT(root_port);
    in_ctx->slot_ctx.state = 0;
       
    /* 4. Setup Endpoint 1 IN Context (Index 3) */
    /* Allocate Ring for EP1 */
    uint64_t ep1_phys = xhci_alloc_page((uint64_t*)&ep1_ring_base);
    ep1_ring_enqueue_idx = 0;
    ep1_ring_cycle_state = 1;
    
    in_ctx->ep_ctx[3].info1 = EP_CTX_CERR(3);
    in_ctx->ep_ctx[3].info2 = EP_CTX_TYPE(EP_TYPE_INT_IN) | EP_CTX_MAX_P_SIZE(8);
    in_ctx->ep_ctx[3].tr_dequeue = ep1_phys | 1; /* DCS = 1 */
    in_ctx->ep_ctx[3].avg_trb_len = 8;
    in_ctx->ep_ctx[3].info1 |= (10 << 16); /* Interval */
    
    /* 5. Send Configure Endpoint Command */
    xhci_send_command(TRB_CONFIGURE_ENDPOINT, in_ctx_phys, 0, (slot_id << 24));
    
    xhci_trb_t *ev = xhci_wait_for_event(TRB_CMD_COMPLETION);
    if (!ev || TRB_GET_CODE(ev->status) != 1) {
        serial_puts("XHCI: Configure Endpoint failed. Code: ");
        if (ev) serial_print_dec(TRB_GET_CODE(ev->status));
        else serial_puts("Timeout");
        serial_puts("\n");
        return -1;
    }
    
    serial_puts("XHCI: Endpoint Configured!\n");
    return 0;
}

static void xhci_ring_ep_doorbell(uint8_t slot, uint8_t target) {
    mmio_write32(db_base, slot * 4, target);
}

static void xhci_queue_transfer(uint64_t buffer, uint32_t len) {
    xhci_trb_t *trb = &ep1_ring_base[ep1_ring_enqueue_idx];
    
    trb->param_low = buffer & 0xFFFFFFFF;
    trb->param_high = (buffer >> 32);
    trb->status = len; /* Transfer Length */
    trb->control = TRB_TYPE(TRB_NORMAL) | 
                   (ep1_ring_cycle_state ? TRB_C : 0) | 
                   TRB_IOC | /* Interrupt On Completion */
                   TRB_ISP; /* Interrupt on Short Packet */
                   /* IDT must be 0 for Normal TRB with data buffer pointer */
                                
    asm volatile("clflush (%0)" :: "r"(trb));
    
    ep1_ring_enqueue_idx++;
    if (ep1_ring_enqueue_idx >= 256) {
        ep1_ring_enqueue_idx = 0;
        ep1_ring_cycle_state ^= 1;
    }
    
    asm volatile("":::"memory");
    /* Ring Doorbell for Slot 1, Endpoint 1 IN (Target = 3) */
    /* Doorbell Register [SlotID]. Value = Target (3). */
    /* We assume Slot 1 for now (global ep1_ring). Ideally pass Slot ID. */
    xhci_ring_ep_doorbell(1, 3);
}

static xhci_trb_t* xhci_poll_event(void) {
    xhci_trb_t *trb = &event_ring_base[event_ring_dequeue_idx];
    
    /* Ensure we read fresh data from RAM */
    asm volatile("clflush (%0)" :: "r"(trb));
    
    /* Check if Cycle Bit matches our Consumer Cycle State */
    uint8_t trb_cycle = (trb->control & TRB_C) ? 1 : 0;
    
    if (trb_cycle == event_ring_cycle_state) {
        /* Advance Dequeue Pointer */
        event_ring_dequeue_idx++;
        if (event_ring_dequeue_idx >= 256) {
            event_ring_dequeue_idx = 0;
            event_ring_cycle_state ^= 1;
        }
        
        uint64_t erdp_phys = hhdm_to_phys((void*)&event_ring_base[event_ring_dequeue_idx]);
        mmio_write64(rt_base, XHCI_RT_IR0_ERDP, erdp_phys | (1 << 3));
        
        return trb;
    }
    return NULL;
}

static xhci_trb_t* xhci_wait_for_event(uint32_t type) {
    /* Timeout */
    for (int i = 0; i < 50000000; i++) {
        xhci_trb_t *ev = xhci_poll_event();
        if (ev) {
            if (TRB_GET_TYPE(ev->control) == type) {
                return ev;
            }
            /* If it's a Port Status Change, just log it and keep waiting */
            if (TRB_GET_TYPE(ev->control) == TRB_PORT_STATUS_CHANGE) {
                 serial_puts("XHCI: Ignored Port Status Change Event\n");
            } else {
                 serial_puts("XHCI: Unexpected Event Type: ");
                 serial_print_dec(TRB_GET_TYPE(ev->control));
                 serial_puts("\n");
            }
        }
        asm volatile("pause");
    }
    return NULL;
}

/* Polling Function - Replaced by IRQ handler */
void xhci_handle_irq(void) {
    /* Check if this interrupter fired */
    uint32_t iman = mmio_read32(rt_base, XHCI_RT_IR0_IMAN);
    
    /* Also clear EINT in USBSTS to keep things clean */
    mmio_write32(mmio_base_virt, 0x44, XHCI_STS_EINT);
    
    if (!(iman & 1)) return; /* IP bit not set */

    /* Acknowledge Interrupt by writing 1 to IP */
    mmio_write32(rt_base, XHCI_RT_IR0_IMAN, iman | 1);

    /* Process all events in the ring */
    xhci_trb_t *ev;
    while ((ev = xhci_poll_event()) != NULL) {
        if (TRB_GET_TYPE(ev->control) == 32) { /* Transfer Event */
            /* Check completion code */
            uint8_t code = TRB_GET_CODE(ev->status);
            if (code == 1 || code == 13) { /* Success or Short Packet */
                 xhci_handle_keyboard_report((uint8_t*)kbd_buf_virt);
            } else {
                 serial_puts("XHCI: Transfer Error Code: ");
                 serial_print_dec(code);
                 serial_puts("\n");
            }
            
            /* Re-queue transfer */
            xhci_queue_transfer(kbd_buf_phys, 8);
        }
    }
}

void xhci_init(uint8_t bus, uint8_t device, uint8_t function) {
    serial_puts("XHCI: Initializing...\n");

    /* 1. Get MMIO Base Address from BAR0 */
    uint32_t bar0 = pci_read_config_32(bus, device, function, PCI_OFFSET_BAR0);
    uint32_t bar1 = pci_read_config_32(bus, device, function, PCI_OFFSET_BAR1);
    
    /* Enable Bus Master and MMIO in PCI Command Register */
    uint16_t cmd = pci_read_config_16(bus, device, function, PCI_OFFSET_COMMAND);
    pci_write_config_16(bus, device, function, PCI_OFFSET_COMMAND, cmd | (1 << 1) | (1 << 2));
    
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

    /* Enable MSI or MSI-X */
    uint8_t msi_ptr = pci_find_capability(bus, device, function, PCI_CAP_ID_MSI);
    if (msi_ptr) {
        serial_puts("XHCI: Configuring MSI...\n");
        uint16_t msg_ctrl = pci_read_config_16(bus, device, function, msi_ptr + PCI_MSI_CTRL);
        
        /* 
         * MSI Message Address: 0xFEE00000 (standard for local APIC)
         * MSI Message Data: 0x40 (Vector, moved from 0x22 to avoid PIC conflict)
         */
        pci_write_config_32(bus, device, function, msi_ptr + PCI_MSI_ADDR_LOW, 0xFEE00000);
        if (msg_ctrl & PCI_MSI_CTRL_64BIT) {
            pci_write_config_32(bus, device, function, msi_ptr + PCI_MSI_ADDR_HIGH, 0);
            pci_write_config_16(bus, device, function, msi_ptr + PCI_MSI_DATA_64, 0x40);
        } else {
            pci_write_config_16(bus, device, function, msi_ptr + PCI_MSI_DATA_32, 0x40);
        }
        
        /* Enable MSI in Control Register */
        pci_write_config_16(bus, device, function, msi_ptr + PCI_MSI_CTRL, msg_ctrl | PCI_MSI_CTRL_ENABLE);
    } else {
        /* Try MSI-X */
        uint8_t msix_ptr = pci_find_capability(bus, device, function, PCI_CAP_ID_MSIX);
        if (msix_ptr) {
             serial_puts("XHCI: Configuring MSI-X...\n");
             uint16_t msg_ctrl = pci_read_config_16(bus, device, function, msix_ptr + 2); // Message Control
             uint32_t table_off = pci_read_config_32(bus, device, function, msix_ptr + 4); // Table Offset
             
             uint8_t bir = table_off & 0x7;
             uint32_t offset = table_off & ~0x7;
             
             /* Check if BIR is 0 (BAR0) */
             if (bir == 0) {
                 /* We already mapped BAR0 at mmio_virt */
                 /* MSI-X Table Entry 0 is at mmio_virt + offset */
                 uint64_t table_entry_addr = mmio_virt + offset;
                 
                 /* Entry 0: Msg Addr Low (0xFEE00000) */
                 mmio_write32(table_entry_addr, 0, 0xFEE00000);
                 /* Entry 0: Msg Addr High (0) */
                 mmio_write32(table_entry_addr, 4, 0);
                 /* Entry 0: Msg Data (0x40) */
                 mmio_write32(table_entry_addr, 8, 0x40);
                 /* Entry 0: Vector Control (0 = Unmasked) */
                 mmio_write32(table_entry_addr, 12, 0);
                 
                 /* Enable MSI-X in Control Register (Bit 15) */
                 /* Also Clear Mask All (Bit 14) just in case */
                 pci_write_config_16(bus, device, function, msix_ptr + 2, (msg_ctrl & ~0x4000) | 0x8000); // Bit 15: Enable, Bit 14: Mask
                 
                 serial_puts("XHCI: MSI-X Enabled.\n");
             } else {
                 serial_puts("XHCI: MSI-X Table in unsupported BAR: ");
                 serial_print_dec(bir);
                 serial_puts("\n");
             }
        } else {
             serial_puts("XHCI: MSI/MSI-X not supported by controller!\n");
        }
    }
    
    /* Set Interrupt Moderation to 0 (Immediate) */
    mmio_write32(rt_base, XHCI_RT_IR0_IMOD, 0);

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
    
    /* Enable Interrupts (IMAN) */
    mmio_write32(rt_base, XHCI_RT_IR0_IMAN, 2); /* Bit 1: IE (Interrupt Enable) */
    mmio_write32(op_base, XHCI_OP_USBCMD, XHCI_CMD_INTE); /* Global Interrupt Enable */

    /* 7. Start Controller */
    serial_puts("XHCI: Starting controller...\n");
    mmio_write32(op_base, XHCI_OP_USBCMD, XHCI_CMD_RUN | XHCI_CMD_INTE);
    
    /* Send NO_OP to verify rings */
    serial_puts("XHCI: Sending NO_OP...\n");
    xhci_send_command(TRB_NOOP, 0, 0, 0);
    
    /* Poll for completion */
    xhci_trb_t *noop_ev = xhci_wait_for_event(TRB_CMD_COMPLETION);
    if (noop_ev && TRB_GET_CODE(noop_ev->status) == 1) {
        serial_puts("XHCI: NO_OP Success!\n");
    } else {
        serial_puts("XHCI: NO_OP Failed!\n");
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
            
            /* Reset Port to enable it */
            mmio_write32(port_reg, 0, portsc | XHCI_PORTSC_PR);
            
            /* Wait for Reset Change (PRC) */
            while (!(mmio_read32(port_reg, 0) & XHCI_PORTSC_PRC)) {
                asm volatile("pause");
            }
            
            /* Clear PRC (Write 1 to clear) */
            mmio_write32(port_reg, 0, XHCI_PORTSC_PRC | XHCI_PORTSC_CSC | XHCI_PORTSC_PP); /* Clear CSC too just in case */
            
            /* Check if enabled */
            portsc = mmio_read32(port_reg, 0);
            if (portsc & XHCI_PORTSC_PED) {
                serial_puts("XHCI: Port Enabled.\n");
                
                uint8_t speed = (portsc >> 10) & 0xF;
                serial_puts("XHCI: Port Speed: ");
                serial_print_dec(speed);
                serial_puts("\n");
                
                int slot_id = xhci_enable_slot();
                if (slot_id > 0) {
                    serial_puts("XHCI: Slot Enabled: ");
                    serial_print_dec(slot_id);
                    serial_puts("\n");
                    
                    if (xhci_address_device(slot_id, i, speed) == 0) {
                        /* Set Configuration 1 */
                        /* ReqType = 0 (Host to Device, Standard, Device Recipient) */
                        /* Request = 9 (SET_CONFIGURATION) */
                        /* Value = 1 (Config Value) */
                        /* Index = 0 */
                        /* Length = 0 */
                        serial_puts("XHCI: Sending SetConfiguration(1)...\n");
                        xhci_send_control_transfer(slot_id, 0, 9, 1, 0, 0);
                        
                        /* Ignore failure and try to configure endpoint anyway */
                        serial_puts("XHCI: Proceeding to Configure Endpoint...\n");
                            
                        if (xhci_configure_endpoint(slot_id, i, speed) == 0) {
                            /* Queue a transfer to read 8 bytes */
                            kbd_buf_phys = xhci_alloc_page(&kbd_buf_virt);
                            
                            serial_puts("XHCI: Queuing transfer...\n");
                            xhci_queue_transfer(kbd_buf_phys, 8);
                            
                            serial_puts("XHCI: Waiting for transfer event (Press a key in QEMU window)...\n");
                            /* We won't wait forever here since it blocks boot. */
                            /* In a real OS, this would be interrupt driven. */
                            /* For now, we leave the transfer queued. */
                            
                            serial_puts("XHCI: USB Keyboard Ready & Listening!\n");
                        }
                    }
                }
            } else {
                serial_puts("XHCI: Port Reset failed to enable port.\n");
            }
        }
    }
}