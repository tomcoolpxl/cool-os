#include "pci.h"
#include "ports.h"
#include "serial.h"
#include "xhci.h"

/* Helper to generate the config address for 0xCF8 */
static uint32_t pci_make_address(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    /*
     * Bit 31: Enable
     * Bits 30-24: Reserved
     * Bits 23-16: Bus Number
     * Bits 15-11: Device/Slot Number
     * Bits 10-8: Function Number
     * Bits 7-2: Register Number (offset & 0xFC)
     * Bits 1-0: 00
     */
    return (uint32_t)((1U << 31) |
                      ((uint32_t)bus << 16) |
                      ((uint32_t)slot << 11) |
                      ((uint32_t)func << 8) |
                      ((uint32_t)(offset & 0xFC)));
}

uint16_t pci_read_config_16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = pci_make_address(bus, slot, func, offset);
    outl(PCI_CONFIG_ADDRESS, address);
    
    /* 
     * Read 32-bits from data port, then shift/mask to get the 16 bits we want.
     * (offset & 2) * 8 calculates shift:
     * if offset % 4 == 0, shift 0
     * if offset % 4 == 2, shift 16
     */
    uint32_t tmp = inl(PCI_CONFIG_DATA);
    return (uint16_t)((tmp >> ((offset & 2) * 8)) & 0xFFFF);
}

uint32_t pci_read_config_32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = pci_make_address(bus, slot, func, offset);
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

void pci_write_config_32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value) {
    uint32_t address = pci_make_address(bus, slot, func, offset);
    outl(PCI_CONFIG_ADDRESS, address);
    outl(PCI_CONFIG_DATA, value);
}

void pci_write_config_16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t value) {
    uint32_t address = pci_make_address(bus, slot, func, offset);
    outl(PCI_CONFIG_ADDRESS, address);
    
    /* Read-modify-write for 16-bit access */
    uint32_t tmp = inl(PCI_CONFIG_DATA);
    uint32_t shift = (offset & 2) * 8;
    tmp &= ~(0xFFFF << shift);
    tmp |= ((uint32_t)value << shift);
    outl(PCI_CONFIG_DATA, tmp);
}

uint8_t pci_find_capability(uint8_t bus, uint8_t slot, uint8_t func, uint8_t cap_id) {
    uint16_t status = pci_read_config_16(bus, slot, func, PCI_OFFSET_STATUS);
    if (!(status & (1 << 4))) return 0; /* Capability List bit */

    uint8_t cap_ptr = (uint8_t)(pci_read_config_32(bus, slot, func, PCI_OFFSET_CAP_PTR) & 0xFF);
    
    while (cap_ptr != 0) {
        uint32_t cap_reg = pci_read_config_32(bus, slot, func, cap_ptr);
        uint8_t id = (uint8_t)(cap_reg & 0xFF);
        if (id == cap_id) return cap_ptr;
        cap_ptr = (uint8_t)((cap_reg >> 8) & 0xFF);
    }
    return 0;
}

static const char* pci_get_class_name(uint8_t class_id) {
    switch (class_id) {
        case PCI_CLASS_UNDEF: return "Undefined";
        case PCI_CLASS_MASS: return "Mass Storage";
        case PCI_CLASS_NET: return "Network";
        case PCI_CLASS_DISP: return "Display";
        case PCI_CLASS_MM: return "Multimedia";
        case PCI_CLASS_MEM: return "Memory";
        case PCI_CLASS_BRIDGE: return "Bridge";
        case PCI_CLASS_COMM: return "Communication";
        case PCI_CLASS_BASE: return "Base System";
        case PCI_CLASS_INPUT: return "Input";
        case PCI_CLASS_SERIAL: return "Serial Bus";
        default: return "Unknown";
    }
}

static void pci_check_function(uint8_t bus, uint8_t device, uint8_t function) {
    uint16_t vendor_id = pci_read_config_16(bus, device, function, PCI_OFFSET_VENDOR_ID);
    if (vendor_id == 0xFFFF) return;

    uint16_t device_id = pci_read_config_16(bus, device, function, PCI_OFFSET_DEVICE_ID);
    uint8_t class_id = (uint8_t)(pci_read_config_32(bus, device, function, PCI_OFFSET_CLASS) >> 24);
    uint8_t subclass_id = (uint8_t)(pci_read_config_32(bus, device, function, PCI_OFFSET_SUBCLASS) >> 16);
    uint8_t prog_if = (uint8_t)(pci_read_config_32(bus, device, function, PCI_OFFSET_PROG_IF) >> 8);

    /* Print log manually since we don't have printf */
    serial_puts("PCI: ");
    serial_print_hex(bus);
    serial_puts(":");
    serial_print_hex(device);
    serial_puts(".");
    serial_print_dec(function);
    serial_puts(" [");
    serial_print_hex(vendor_id);
    serial_puts(":");
    serial_print_hex(device_id);
    serial_puts("] Class: ");
    serial_print_hex(class_id);
    serial_puts(" (");
    serial_puts(pci_get_class_name(class_id));
    serial_puts(") Sub: ");
    serial_print_hex(subclass_id);
    serial_puts(" ProgIF: ");
    serial_print_hex(prog_if);
    serial_puts("\n");

    /* Check for USB Controllers */
    if (class_id == PCI_CLASS_SERIAL && subclass_id == PCI_SUBCLASS_USB) {
        const char* type = "Unknown";
        if (prog_if == PCI_PROGIF_UHCI) type = "UHCI (USB 1.1)";
        else if (prog_if == PCI_PROGIF_OHCI) type = "OHCI (USB 1.1)";
        else if (prog_if == PCI_PROGIF_EHCI) type = "EHCI (USB 2.0)";
        else if (prog_if == PCI_PROGIF_XHCI) type = "XHCI (USB 3.0)";
        
        serial_puts("PCI: Found USB Controller: ");
        serial_puts(type);
        serial_puts("\n");

        if (prog_if == PCI_PROGIF_XHCI) {
            xhci_init(bus, device, function);
        }
    }
}

void pci_init(void) {
    serial_puts("PCI: Enumerating bus...\n");

    /* Brute force scan: 256 buses, 32 devices, 8 functions */
    /* Optimization: Real OSes check header types for multifunction, but this is fine for now */
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t dev = 0; dev < 32; dev++) {
            /* Check function 0 first */
            uint16_t vendor_id = pci_read_config_16(bus, dev, 0, PCI_OFFSET_VENDOR_ID);
            if (vendor_id == 0xFFFF) continue;

            pci_check_function(bus, dev, 0);

            /* Check header type for multifunction bit (bit 7) */
            uint8_t header_type = (uint8_t)(pci_read_config_32(bus, dev, 0, PCI_OFFSET_HEADER_TYPE) >> 16);
            if (header_type & 0x80) {
                /* Multifunction device, check other functions */
                for (uint8_t func = 1; func < 8; func++) {
                    if (pci_read_config_16(bus, dev, func, PCI_OFFSET_VENDOR_ID) != 0xFFFF) {
                        pci_check_function(bus, dev, func);
                    }
                }
            }
        }
    }
    serial_puts("PCI: Enumeration complete.\n");
}
