#ifndef PCI_H
#define PCI_H

#include <stdint.h>

/* PCI Configuration Ports */
#define PCI_CONFIG_ADDRESS  0xCF8
#define PCI_CONFIG_DATA     0xCFC

/* PCI Class Codes */
#define PCI_CLASS_UNDEF     0x00
#define PCI_CLASS_MASS      0x01
#define PCI_CLASS_NET       0x02
#define PCI_CLASS_DISP      0x03
#define PCI_CLASS_MM        0x04
#define PCI_CLASS_MEM       0x05
#define PCI_CLASS_BRIDGE    0x06
#define PCI_CLASS_COMM      0x07
#define PCI_CLASS_BASE      0x08
#define PCI_CLASS_INPUT     0x09
#define PCI_CLASS_DOCK      0x0A
#define PCI_CLASS_PROC      0x0B
#define PCI_CLASS_SERIAL    0x0C
#define PCI_CLASS_WIRELESS  0x0D
#define PCI_CLASS_INTEL     0x0E
#define PCI_CLASS_SAT       0x0F
#define PCI_CLASS_CRYPT     0x10
#define PCI_CLASS_SIG       0x11

/* PCI Serial Bus Subclasses (Class 0x0C) */
#define PCI_SUBCLASS_USB    0x03

/* USB Programming Interfaces */
#define PCI_PROGIF_UHCI     0x00
#define PCI_PROGIF_OHCI     0x10
#define PCI_PROGIF_EHCI     0x20
#define PCI_PROGIF_XHCI     0x30

/* Common PCI Offsets */
#define PCI_OFFSET_VENDOR_ID    0x00
#define PCI_OFFSET_DEVICE_ID    0x02
#define PCI_OFFSET_COMMAND      0x04
#define PCI_OFFSET_STATUS       0x06
#define PCI_OFFSET_REVISION_ID  0x08
#define PCI_OFFSET_PROG_IF      0x09
#define PCI_OFFSET_SUBCLASS     0x0A
#define PCI_OFFSET_CLASS        0x0B
#define PCI_OFFSET_CACHE_LINE   0x0C
#define PCI_OFFSET_LATENCY      0x0D
#define PCI_OFFSET_HEADER_TYPE  0x0E
#define PCI_OFFSET_BIST         0x0F
#define PCI_OFFSET_BAR0         0x10
#define PCI_OFFSET_BAR1         0x14
#define PCI_OFFSET_BAR2         0x18
#define PCI_OFFSET_BAR3         0x1C
#define PCI_OFFSET_BAR4         0x20
#define PCI_OFFSET_BAR5         0x24
#define PCI_OFFSET_IRQ          0x3C

typedef struct {
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t class_id;
    uint8_t subclass_id;
    uint8_t prog_if;
    uint8_t header_type;
} pci_device_t;

/* Initialize and scan PCI bus */
void pci_init(void);

/* Read/Write PCI config space */
uint16_t pci_read_config_16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
uint32_t pci_read_config_32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
void pci_write_config_32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value);

#endif
