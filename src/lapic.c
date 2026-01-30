#include "lapic.h"
#include "msr.h"
#include "paging.h"
#include "serial.h"

#define IA32_APIC_BASE_MSR 0x1B
#define IA32_APIC_BASE_MSR_ENABLE 0x800
#define LAPIC_SVR 0xF0
#define LAPIC_SVR_ENABLE 0x100

static uint64_t lapic_base;

void lapic_init(void) {
    uint64_t apic_base_msr = rdmsr(IA32_APIC_BASE_MSR);
    lapic_base = apic_base_msr & 0xFFFFF000;
    
    /* Map LAPIC (4KB) */
    /* We map it to a high virtual address */
    uint64_t lapic_virt = 0xFFFFFFFFFEE00000;
    paging_map_page(lapic_virt, lapic_base, PTE_PRESENT | PTE_WRITABLE | PTE_CACHE_DIS);
    
    serial_puts("LAPIC: Base (Phys) = ");
    serial_print_hex(lapic_base);
    serial_puts("\n");
    
    /* Enable LAPIC via SVR (Spurious Interrupt Vector Register) */
    /* Map Spurious Vector to 0xFF (255) and set Bit 8 (Software Enable) */
    uint32_t svr = *(volatile uint32_t*)(lapic_virt + LAPIC_SVR);
    *(volatile uint32_t*)(lapic_virt + LAPIC_SVR) = svr | LAPIC_SVR_ENABLE | 0xFF;
    
    /* Read APIC ID */
    uint32_t id_reg = *(volatile uint32_t*)(lapic_virt + 0x20);
    uint32_t apic_id = (id_reg >> 24) & 0xFF;
    
    serial_puts("LAPIC: Enabled (SVR set). APIC ID: ");
    serial_print_hex(apic_id);
    serial_puts("\n");
}
