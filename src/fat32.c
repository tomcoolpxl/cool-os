#include <stddef.h>
#include "fat32.h"
#include "block.h"
#include "heap.h"
#include "serial.h"

/*
 * FAT32 Filesystem Driver
 *
 * Read-only implementation supporting:
 * - Superfloppy layout (no partition table)
 * - Root directory only (no subdirectory traversal)
 * - 8.3 filenames
 */

/* Filesystem state */
static int fat_mounted = 0;
static uint32_t bytes_per_sector;
static uint32_t sectors_per_cluster;
static uint32_t reserved_sectors;
static uint32_t fat_start_sector;      /* First sector of FAT */
static uint32_t fat_size_sectors;      /* Sectors per FAT */
static uint32_t data_start_sector;     /* First sector of data region */
static uint32_t root_cluster;          /* First cluster of root directory */
static uint32_t bytes_per_cluster;

/* Open file table */
static fat_file_t open_files[FAT_MAX_OPEN];

/* Sector buffer for reads */
static uint8_t sector_buf[512];

/*
 * Convert cluster number to first sector of that cluster.
 */
static uint32_t cluster_to_sector(uint32_t cluster) {
    return data_start_sector + (cluster - 2) * sectors_per_cluster;
}

/*
 * Read a FAT entry for a given cluster.
 * Returns the next cluster number, or FAT32_EOC_MIN+ if end of chain.
 */
static uint32_t fat_get_entry(uint32_t cluster) {
    /* Each FAT entry is 4 bytes */
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = fat_start_sector + (fat_offset / bytes_per_sector);
    uint32_t entry_offset = fat_offset % bytes_per_sector;

    if (block_read(fat_sector, 1, sector_buf) != 0) {
        return FAT32_EOC_MIN;  /* Error, treat as end of chain */
    }

    uint32_t entry = *(uint32_t *)(sector_buf + entry_offset);
    return entry & 0x0FFFFFFF;  /* FAT32 uses only lower 28 bits */
}

/*
 * Compare 8.3 filename.
 * name: 11-character space-padded name from directory entry
 * path: User-provided filename (e.g., "INIT.ELF")
 * Returns 1 if match, 0 otherwise.
 */
static int fat_name_match(const uint8_t *name, const char *path) {
    /* Build 11-character 8.3 name from path */
    uint8_t target[11];
    int i, j;

    /* Initialize with spaces */
    for (i = 0; i < 11; i++) {
        target[i] = ' ';
    }

    /* Copy name part (up to 8 chars, before dot) */
    for (i = 0, j = 0; path[i] && path[i] != '.' && j < 8; i++, j++) {
        char c = path[i];
        /* Convert to uppercase */
        if (c >= 'a' && c <= 'z') {
            c -= 32;
        }
        target[j] = c;
    }

    /* Skip to extension if present */
    while (path[i] && path[i] != '.') {
        i++;
    }
    if (path[i] == '.') {
        i++;  /* Skip the dot */
    }

    /* Copy extension (up to 3 chars) */
    for (j = 8; path[i] && j < 11; i++, j++) {
        char c = path[i];
        if (c >= 'a' && c <= 'z') {
            c -= 32;
        }
        target[j] = c;
    }

    /* Compare */
    for (i = 0; i < 11; i++) {
        if (name[i] != target[i]) {
            return 0;
        }
    }
    return 1;
}

int fat_mount(void) {
    serial_puts("fat32: Mounting filesystem\n");

    /* Read the boot sector (sector 0) */
    if (block_read(0, 1, sector_buf) != 0) {
        serial_puts("fat32: Failed to read boot sector\n");
        return -1;
    }

    fat32_bpb_t *bpb = (fat32_bpb_t *)sector_buf;

    /* Basic validation */
    if (bpb->bytes_per_sector != 512) {
        serial_puts("fat32: Unsupported sector size\n");
        return -1;
    }

    /* Check for FAT32 signature (should have FAT32 in fs_type or non-zero fat_size_32) */
    if (bpb->fat_size_32 == 0) {
        serial_puts("fat32: Not a FAT32 filesystem\n");
        return -1;
    }

    /* Store filesystem parameters */
    bytes_per_sector = bpb->bytes_per_sector;
    sectors_per_cluster = bpb->sectors_per_cluster;
    reserved_sectors = bpb->reserved_sectors;
    fat_size_sectors = bpb->fat_size_32;
    root_cluster = bpb->root_cluster;
    bytes_per_cluster = bytes_per_sector * sectors_per_cluster;

    /* Calculate region offsets */
    fat_start_sector = reserved_sectors;
    data_start_sector = reserved_sectors + (bpb->num_fats * fat_size_sectors);

    serial_puts("fat32: sectors_per_cluster=");
    /* Simple decimal print for small numbers */
    serial_putc('0' + sectors_per_cluster);
    serial_puts(" root_cluster=");
    serial_putc('0' + (root_cluster % 10));
    serial_puts("\n");

    /* Initialize file table */
    for (int i = 0; i < FAT_MAX_OPEN; i++) {
        open_files[i].in_use = 0;
    }

    fat_mounted = 1;
    serial_puts("fat32: Filesystem mounted successfully\n");
    return 0;
}

int fat_open(const char *path) {
    if (!fat_mounted || path == NULL) {
        return -1;
    }

    /* Find free file descriptor */
    int fd = -1;
    for (int i = 0; i < FAT_MAX_OPEN; i++) {
        if (!open_files[i].in_use) {
            fd = i;
            break;
        }
    }
    if (fd < 0) {
        serial_puts("fat32: No free file descriptors\n");
        return -1;
    }

    /* Search root directory for the file */
    uint32_t cluster = root_cluster;

    while (cluster < FAT32_EOC_MIN) {
        uint32_t sector = cluster_to_sector(cluster);

        /* Read each sector in this cluster */
        for (uint32_t s = 0; s < sectors_per_cluster; s++) {
            if (block_read(sector + s, 1, sector_buf) != 0) {
                return -1;
            }

            /* Scan directory entries in this sector */
            fat32_dirent_t *dirent = (fat32_dirent_t *)sector_buf;
            for (int i = 0; i < 16; i++) {  /* 16 entries per 512-byte sector */
                /* Check for end of directory */
                if (dirent[i].name[0] == 0x00) {
                    return -1;  /* File not found */
                }

                /* Skip deleted entries */
                if (dirent[i].name[0] == 0xE5) {
                    continue;
                }

                /* Skip LFN entries and volume labels */
                if (dirent[i].attr == FAT_ATTR_LFN) {
                    continue;
                }
                if (dirent[i].attr & FAT_ATTR_VOLUME_ID) {
                    continue;
                }

                /* Check if name matches */
                if (fat_name_match(dirent[i].name, path)) {
                    /* Found it! */
                    uint32_t first_cluster = ((uint32_t)dirent[i].first_cluster_hi << 16) |
                                            dirent[i].first_cluster_lo;

                    open_files[fd].in_use = 1;
                    open_files[fd].first_cluster = first_cluster;
                    open_files[fd].file_size = dirent[i].file_size;
                    open_files[fd].position = 0;
                    open_files[fd].current_cluster = first_cluster;
                    open_files[fd].cluster_offset = 0;

                    return fd;
                }
            }
        }

        /* Follow cluster chain */
        cluster = fat_get_entry(cluster);
    }

    return -1;  /* File not found */
}

int fat_read(int fd, void *buf, uint32_t n) {
    if (fd < 0 || fd >= FAT_MAX_OPEN || !open_files[fd].in_use) {
        return -1;
    }
    if (buf == NULL || n == 0) {
        return 0;
    }

    fat_file_t *file = &open_files[fd];
    uint8_t *dst = (uint8_t *)buf;
    uint32_t bytes_read = 0;

    /* Limit read to remaining file size */
    if (file->position + n > file->file_size) {
        n = file->file_size - file->position;
    }

    while (bytes_read < n) {
        /* Check if we've reached end of file */
        if (file->current_cluster >= FAT32_EOC_MIN) {
            break;
        }

        /* Calculate position within current cluster */
        uint32_t cluster_pos = file->cluster_offset;
        uint32_t sector_in_cluster = cluster_pos / bytes_per_sector;
        uint32_t offset_in_sector = cluster_pos % bytes_per_sector;

        /* Read the sector */
        uint32_t sector = cluster_to_sector(file->current_cluster) + sector_in_cluster;
        if (block_read(sector, 1, sector_buf) != 0) {
            return -1;
        }

        /* Copy data from sector to buffer */
        uint32_t bytes_in_sector = bytes_per_sector - offset_in_sector;
        uint32_t bytes_to_copy = n - bytes_read;
        if (bytes_to_copy > bytes_in_sector) {
            bytes_to_copy = bytes_in_sector;
        }

        for (uint32_t i = 0; i < bytes_to_copy; i++) {
            dst[bytes_read + i] = sector_buf[offset_in_sector + i];
        }

        bytes_read += bytes_to_copy;
        file->position += bytes_to_copy;
        file->cluster_offset += bytes_to_copy;

        /* Move to next cluster if needed */
        if (file->cluster_offset >= bytes_per_cluster) {
            file->current_cluster = fat_get_entry(file->current_cluster);
            file->cluster_offset = 0;
        }
    }

    return bytes_read;
}

int fat_seek(int fd, uint32_t offset) {
    if (fd < 0 || fd >= FAT_MAX_OPEN || !open_files[fd].in_use) {
        return -1;
    }

    fat_file_t *file = &open_files[fd];

    /* Clamp to file size */
    if (offset > file->file_size) {
        offset = file->file_size;
    }

    /* Reset to start of file */
    file->position = 0;
    file->current_cluster = file->first_cluster;
    file->cluster_offset = 0;

    /* Skip whole clusters */
    while (offset >= bytes_per_cluster && file->current_cluster < FAT32_EOC_MIN) {
        file->current_cluster = fat_get_entry(file->current_cluster);
        offset -= bytes_per_cluster;
        file->position += bytes_per_cluster;
    }

    /* Set remaining offset within cluster */
    file->cluster_offset = offset;
    file->position += offset;

    return 0;
}

int fat_close(int fd) {
    if (fd < 0 || fd >= FAT_MAX_OPEN || !open_files[fd].in_use) {
        return -1;
    }

    open_files[fd].in_use = 0;
    return 0;
}

uint32_t fat_get_size(int fd) {
    if (fd < 0 || fd >= FAT_MAX_OPEN || !open_files[fd].in_use) {
        return 0;
    }

    return open_files[fd].file_size;
}
