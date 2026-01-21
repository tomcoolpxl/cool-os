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

void xhci_init(uint8_t bus, uint8_t device, uint8_t function);

#endif
