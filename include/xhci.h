#ifndef XHCI_H
#define XHCI_H

#include <stdint.h>

/* Capability Registers Offsets (Relative to BAR0) */
#define XHCI_CAP_CAPLENGTH      0x00    /* 8-bit */
#define XHCI_CAP_HCIVERSION     0x02    /* 16-bit */
#define XHCI_CAP_HCSPARAMS1     0x04    /* 32-bit */
#define XHCI_CAP_HCSPARAMS2     0x08    /* 32-bit */
#define XHCI_CAP_HCSPARAMS3     0x0C    /* 32-bit */
#define XHCI_CAP_HCCPARAMS1     0x10    /* 32-bit */
#define XHCI_CAP_DBOFF          0x14    /* 32-bit */
#define XHCI_CAP_RTSOFF         0x18    /* 32-bit */
#define XHCI_CAP_HCCPARAMS2     0x1C    /* 32-bit */

/* Operational Registers Offsets (Relative to Operational Base) */
#define XHCI_OP_USBCMD          0x00    /* 32-bit */
#define XHCI_OP_USBSTS          0x04    /* 32-bit */
#define XHCI_OP_PAGESIZE        0x08    /* 32-bit */
#define XHCI_OP_DNCTRL          0x14    /* 32-bit */
#define XHCI_OP_CRCR            0x18    /* 64-bit Command Ring Control */
#define XHCI_OP_DCBAAP          0x30    /* 64-bit Device Context Base Addr Array Ptr */
#define XHCI_OP_CONFIG          0x38    /* 32-bit */

/* USBCMD Bits */
#define XHCI_CMD_RUN            (1 << 0)
#define XHCI_CMD_RESET          (1 << 1)
#define XHCI_CMD_INTE           (1 << 2)
#define XHCI_CMD_HSEE           (1 << 3)

/* USBSTS Bits */
#define XHCI_STS_HCH            (1 << 0)    /* HC Halted */
#define XHCI_STS_HSE            (1 << 2)    /* Host System Error */
#define XHCI_STS_EINT           (1 << 3)    /* Event Interrupt */
#define XHCI_STS_PCD            (1 << 4)    /* Port Change Detect */
#define XHCI_STS_CNR            (1 << 11)   /* Controller Not Ready */

/* Port Status and Control (PORTSC) Bits */
#define XHCI_PORTSC_CCS         (1 << 0)    /* Current Connect Status */
#define XHCI_PORTSC_PED         (1 << 1)    /* Port Enabled/Disabled */
#define XHCI_PORTSC_PR          (1 << 4)    /* Port Reset */
#define XHCI_PORTSC_PP          (1 << 9)    /* Port Power */
#define XHCI_PORTSC_CSC         (1 << 17)   /* Connect Status Change */
#define XHCI_PORTSC_PRC         (1 << 21)   /* Port Reset Change */

/* Runtime Register Offsets (Relative to Runtime Base) */
#define XHCI_RT_IR0_IMAN        0x20
#define XHCI_RT_IR0_IMOD        0x24
#define XHCI_RT_IR0_ERSTSZ      0x28
#define XHCI_RT_IR0_ERSTBA      0x30
#define XHCI_RT_IR0_ERDP        0x38

/* TRB Types */
#define TRB_NORMAL              1
#define TRB_SETUP_STAGE         2
#define TRB_DATA_STAGE          3
#define TRB_STATUS_STAGE        4
#define TRB_LINK                6
#define TRB_NOOP                23
#define TRB_ENABLE_SLOT         9
#define TRB_ADDRESS_DEVICE      11
#define TRB_CONFIGURE_ENDPOINT  12
#define TRB_CMD_COMPLETION      33
#define TRB_PORT_STATUS_CHANGE  34

/* TRB Control Bits */
#define TRB_C                   (1 << 0)    /* Cycle Bit */
#define TRB_TC                  (1 << 1)    /* Toggle Cycle */
#define TRB_ISP                 (1 << 2)    /* Interrupt on Short Packet */
#define TRB_CH                  (1 << 4)    /* Chain */
#define TRB_IOC                 (1 << 5)    /* Interrupt On Completion */
#define TRB_IDT                 (1 << 6)    /* Immediate Data */

#define TRB_TYPE(p)             ((p) << 10)
#define TRB_GET_TYPE(control)   (((control) >> 10) & 0x3F)
#define TRB_GET_CODE(status)    (((status) >> 24) & 0xFF)

/* Data Structures */

/* Transfer Request Block (TRB) */
typedef struct {
    uint32_t param_low;
    uint32_t param_high;
    uint32_t status;
    uint32_t control;
} __attribute__((packed)) xhci_trb_t;

/* Event Ring Segment Table Entry */
typedef struct {
    uint64_t base_address;
    uint32_t size;
    uint32_t reserved;
} __attribute__((packed)) xhci_erst_entry_t;

/* Context Structures (32-byte CSZ=0 assumed) */
typedef struct {
    uint32_t info1;         /* Route String (0:19), Speed (20:23), MTT (25), Hub (26), Context Entries (27:31) */
    uint32_t info2;         /* Max Exit Latency, Root Hub Port Number, Number of Ports */
    uint32_t tt_id;
    uint32_t state;         /* Slot State (27:31), USB Device Address (0:7) */
    uint32_t reserved[4];
} __attribute__((packed)) xhci_slot_ctx_t;

typedef struct {
    uint32_t info1;         /* EP State (0:2), Mult (8:9), MaxPStreams (10:14), LSA (15), Interval (16:23) */
    uint32_t info2;         /* CErr (1:2), EP Type (3:5), HID (7), Max Burst (8:15), Max Packet Size (16:31) */
    uint64_t tr_dequeue;    /* Dequeue Pointer (4:63), DCS (0) */
    uint32_t avg_trb_len;
    uint32_t reserved[3];
} __attribute__((packed)) xhci_ep_ctx_t;

typedef struct {
    uint32_t drop_flags;
    uint32_t add_flags;
    uint32_t reserved[6];
    xhci_slot_ctx_t slot_ctx;
    xhci_ep_ctx_t ep_ctx[31];
} __attribute__((packed)) xhci_input_ctx_t;

/* Slot Context Info1 Helpers */
#define SLOT_CTX_ENTRIES(n)     (((n) & 0x1F) << 27)
#define SLOT_CTX_SPEED(n)       (((n) & 0xF) << 20)
#define SLOT_CTX_ROUTE(n)       ((n) & 0xFFFFF)

/* Slot Context Info2 Helpers */
#define SLOT_CTX_ROOT_PORT(n)   (((n) & 0xFF) << 16)

/* Endpoint Context Info2 Helpers */
#define EP_CTX_TYPE(n)          (((n) & 0x7) << 3)
#define EP_CTX_MAX_P_SIZE(n)    (((n) & 0xFFFF) << 16)
#define EP_CTX_CERR(n)          (((n) & 0x3) << 1)

/* Endpoint Types */
#define EP_TYPE_CONTROL         4
#define EP_TYPE_ISO_OUT         1
#define EP_TYPE_BULK_OUT        2
#define EP_TYPE_INT_OUT         3
#define EP_TYPE_ISO_IN          5
#define EP_TYPE_BULK_IN         6
#define EP_TYPE_INT_IN          7

void xhci_init(uint8_t bus, uint8_t device, uint8_t function);
void xhci_poll(void);
void xhci_handle_irq(void);

#endif
