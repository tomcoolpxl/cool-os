#ifndef BLOCK_H
#define BLOCK_H

#include <stdint.h>

/*
 * Block Device Interface - ATA PIO Mode Driver
 *
 * Provides sector-level read access to the primary IDE drive.
 * Uses ports 0x1F0-0x1F7 for the primary ATA controller.
 */

/* ATA PIO port definitions */
#define ATA_PRIMARY_DATA        0x1F0
#define ATA_PRIMARY_ERROR       0x1F1
#define ATA_PRIMARY_SECCOUNT    0x1F2
#define ATA_PRIMARY_LBA_LO      0x1F3
#define ATA_PRIMARY_LBA_MID     0x1F4
#define ATA_PRIMARY_LBA_HI      0x1F5
#define ATA_PRIMARY_DRIVE       0x1F6
#define ATA_PRIMARY_STATUS      0x1F7
#define ATA_PRIMARY_CMD         0x1F7

/* ATA status register bits */
#define ATA_SR_BSY   0x80    /* Busy */
#define ATA_SR_DRDY  0x40    /* Drive ready */
#define ATA_SR_DF    0x20    /* Drive write fault */
#define ATA_SR_DSC   0x10    /* Drive seek complete */
#define ATA_SR_DRQ   0x08    /* Data request ready */
#define ATA_SR_CORR  0x04    /* Corrected data */
#define ATA_SR_IDX   0x02    /* Index */
#define ATA_SR_ERR   0x01    /* Error */

/* ATA commands */
#define ATA_CMD_READ_PIO        0x20
#define ATA_CMD_READ_PIO_EXT    0x24
#define ATA_CMD_IDENTIFY        0xEC

/* Sector size */
#define ATA_SECTOR_SIZE  512

/*
 * Initialize the block device driver.
 * Probes for an ATA drive on the primary controller.
 * Returns 0 on success, -1 if no drive found.
 */
int block_init(void);

/*
 * Read sectors from the block device using LBA28 addressing.
 * lba: Starting logical block address
 * count: Number of sectors to read (1-256, 0 means 256)
 * dst: Destination buffer (must be at least count * 512 bytes)
 * Returns 0 on success, -1 on error.
 */
int block_read(uint64_t lba, uint16_t count, void *dst);

#endif
