#include <stddef.h>
#include "block.h"
#include "ports.h"
#include "serial.h"

/*
 * ATA PIO Mode Driver
 *
 * Simple polling-based ATA driver for reading sectors from the primary
 * IDE drive. Uses LBA28 addressing (supports up to 128 GiB).
 */

static int ata_drive_present = 0;

/*
 * Wait for the drive to be ready (BSY clear).
 * Returns 0 on success, -1 on timeout/error.
 */
static int ata_wait_ready(void) {
    /* Poll status until BSY clears */
    for (int i = 0; i < 100000; i++) {
        uint8_t status = inb(ATA_PRIMARY_STATUS);
        if (!(status & ATA_SR_BSY)) {
            return 0;
        }
    }
    return -1;
}

/*
 * Wait for data request ready (DRQ set).
 * Returns 0 on success, -1 on error/timeout.
 */
static int ata_wait_drq(void) {
    for (int i = 0; i < 100000; i++) {
        uint8_t status = inb(ATA_PRIMARY_STATUS);
        if (status & ATA_SR_ERR) {
            return -1;
        }
        if (status & ATA_SR_DF) {
            return -1;
        }
        if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ)) {
            return 0;
        }
    }
    return -1;
}

/*
 * Software reset the ATA controller.
 */
static void ata_soft_reset(void) {
    /* Write to control register (0x3F6) with SRST bit set */
    outb(0x3F6, 0x04);
    io_wait();
    io_wait();
    io_wait();
    io_wait();
    /* Clear SRST */
    outb(0x3F6, 0x00);
    io_wait();
    io_wait();
    io_wait();
    io_wait();
}

int block_init(void) {
    serial_puts("block: Initializing ATA PIO driver\n");

    /* Soft reset the controller */
    ata_soft_reset();

    /* Select slave drive (drive 1) - data disk is on IDE index 1 */
    outb(ATA_PRIMARY_DRIVE, 0xB0);
    io_wait();

    /* Wait for drive to be ready */
    if (ata_wait_ready() != 0) {
        serial_puts("block: Timeout waiting for drive\n");
        return -1;
    }

    /* Send IDENTIFY command */
    outb(ATA_PRIMARY_SECCOUNT, 0);
    outb(ATA_PRIMARY_LBA_LO, 0);
    outb(ATA_PRIMARY_LBA_MID, 0);
    outb(ATA_PRIMARY_LBA_HI, 0);
    outb(ATA_PRIMARY_CMD, ATA_CMD_IDENTIFY);

    /* Read status - if 0, no drive */
    uint8_t status = inb(ATA_PRIMARY_STATUS);
    if (status == 0) {
        serial_puts("block: No drive on primary controller\n");
        return -1;
    }

    /* Wait for BSY to clear */
    if (ata_wait_ready() != 0) {
        serial_puts("block: Timeout during IDENTIFY\n");
        return -1;
    }

    /* Check if it's an ATA drive (LBA_MID and LBA_HI should be 0) */
    if (inb(ATA_PRIMARY_LBA_MID) != 0 || inb(ATA_PRIMARY_LBA_HI) != 0) {
        serial_puts("block: Not an ATA drive (possibly ATAPI)\n");
        return -1;
    }

    /* Wait for DRQ or error */
    if (ata_wait_drq() != 0) {
        serial_puts("block: IDENTIFY failed\n");
        return -1;
    }

    /* Read and discard IDENTIFY data (256 words = 512 bytes) */
    uint16_t identify_data[256];
    for (int i = 0; i < 256; i++) {
        identify_data[i] = inw(ATA_PRIMARY_DATA);
    }

    /* Check if LBA is supported (bit 9 of word 49) */
    if (!(identify_data[49] & (1 << 9))) {
        serial_puts("block: Drive does not support LBA\n");
        return -1;
    }

    ata_drive_present = 1;
    serial_puts("block: ATA drive detected, LBA supported\n");
    return 0;
}

int block_read(uint64_t lba, uint16_t count, void *dst) {
    if (!ata_drive_present) {
        return -1;
    }

    if (dst == NULL) {
        return -1;
    }

    /* LBA28 can only address up to 2^28 sectors */
    if (lba >= (1ULL << 28)) {
        serial_puts("block: LBA out of range for LBA28\n");
        return -1;
    }

    /* Count of 0 means 256 sectors in ATA spec, but we don't support that here */
    if (count == 0 || count > 256) {
        return -1;
    }

    /* Wait for drive to be ready */
    if (ata_wait_ready() != 0) {
        return -1;
    }

    /* Select drive 1 (slave) with LBA mode and high 4 bits of LBA */
    outb(ATA_PRIMARY_DRIVE, 0xF0 | ((lba >> 24) & 0x0F));
    io_wait();

    /* Set sector count */
    outb(ATA_PRIMARY_SECCOUNT, (uint8_t)(count == 256 ? 0 : count));

    /* Set LBA low/mid/high */
    outb(ATA_PRIMARY_LBA_LO, (uint8_t)(lba & 0xFF));
    outb(ATA_PRIMARY_LBA_MID, (uint8_t)((lba >> 8) & 0xFF));
    outb(ATA_PRIMARY_LBA_HI, (uint8_t)((lba >> 16) & 0xFF));

    /* Send READ SECTORS command */
    outb(ATA_PRIMARY_CMD, ATA_CMD_READ_PIO);

    /* Read each sector */
    uint16_t *buf = (uint16_t *)dst;
    for (uint16_t i = 0; i < count; i++) {
        /* Wait for data to be ready */
        if (ata_wait_drq() != 0) {
            serial_puts("block: Read error waiting for DRQ\n");
            return -1;
        }

        /* Read 256 words (512 bytes) using rep insw */
        insw(ATA_PRIMARY_DATA, buf, 256);
        buf += 256;
    }

    return 0;
}
