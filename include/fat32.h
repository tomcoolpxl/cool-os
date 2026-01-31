#ifndef FAT32_H
#define FAT32_H

#include <stdint.h>

/*
 * FAT32 Filesystem Driver
 *
 * Read-only driver supporting superfloppy layout (no partition table).
 * Supports root directory only with 8.3 filenames.
 */

/* FAT32 Boot Parameter Block (BPB) - offset 0 in sector 0 */
typedef struct __attribute__((packed)) {
    uint8_t  jmp[3];            /* Jump instruction */
    uint8_t  oem[8];            /* OEM identifier */
    uint16_t bytes_per_sector;  /* Usually 512 */
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;  /* Sectors before first FAT */
    uint8_t  num_fats;          /* Usually 2 */
    uint16_t root_entry_count;  /* 0 for FAT32 */
    uint16_t total_sectors_16;  /* 0 for FAT32 */
    uint8_t  media_type;
    uint16_t fat_size_16;       /* 0 for FAT32 */
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    /* FAT32-specific fields */
    uint32_t fat_size_32;       /* Sectors per FAT */
    uint16_t ext_flags;
    uint16_t fs_version;
    uint32_t root_cluster;      /* First cluster of root directory */
    uint16_t fs_info;
    uint16_t backup_boot_sector;
    uint8_t  reserved[12];
    uint8_t  drive_number;
    uint8_t  reserved1;
    uint8_t  boot_signature;
    uint32_t volume_id;
    uint8_t  volume_label[11];
    uint8_t  fs_type[8];        /* "FAT32   " */
} fat32_bpb_t;

/* FAT32 Directory Entry (32 bytes) */
typedef struct __attribute__((packed)) {
    uint8_t  name[11];          /* 8.3 filename (space-padded) */
    uint8_t  attr;              /* File attributes */
    uint8_t  nt_reserved;
    uint8_t  create_time_tenth;
    uint16_t create_time;
    uint16_t create_date;
    uint16_t access_date;
    uint16_t first_cluster_hi;  /* High 16 bits of first cluster */
    uint16_t modify_time;
    uint16_t modify_date;
    uint16_t first_cluster_lo;  /* Low 16 bits of first cluster */
    uint32_t file_size;
} fat32_dirent_t;

/* File attributes */
#define FAT_ATTR_READ_ONLY  0x01
#define FAT_ATTR_HIDDEN     0x02
#define FAT_ATTR_SYSTEM     0x04
#define FAT_ATTR_VOLUME_ID  0x08
#define FAT_ATTR_DIRECTORY  0x10
#define FAT_ATTR_ARCHIVE    0x20
#define FAT_ATTR_LFN        0x0F  /* Long filename entry */

/* Special cluster values */
#define FAT32_EOC_MIN  0x0FFFFFF8  /* End of chain (minimum value) */
#define FAT32_BAD      0x0FFFFFF7  /* Bad cluster */
#define FAT32_FREE     0x00000000  /* Free cluster */

/* Maximum open files */
#define FAT_MAX_OPEN  16

/* File handle structure */
typedef struct {
    int in_use;
    uint32_t first_cluster;
    uint32_t file_size;
    uint32_t position;       /* Current read position */
    uint32_t current_cluster; /* Current cluster being read */
    uint32_t cluster_offset; /* Offset within current cluster */
} fat_file_t;

/*
 * Mount the FAT32 filesystem.
 * Reads the BPB and initializes filesystem state.
 * Returns 0 on success, -1 on error.
 */
int fat_mount(void);

/*
 * Open a file by name (8.3 format, e.g., "INIT.ELF").
 * Only supports root directory.
 * Returns file descriptor (>=0) on success, -1 if not found.
 */
int fat_open(const char *path);

/*
 * Read bytes from an open file.
 * fd: File descriptor from fat_open()
 * buf: Destination buffer
 * n: Number of bytes to read
 * Returns number of bytes read, or -1 on error.
 */
int fat_read(int fd, void *buf, uint32_t n);

/*
 * Seek to position in file.
 * fd: File descriptor
 * offset: Absolute byte offset from start of file
 * Returns 0 on success, -1 on error.
 */
int fat_seek(int fd, uint32_t offset);

/*
 * Close an open file.
 * fd: File descriptor
 * Returns 0 on success, -1 on error.
 */
int fat_close(int fd);

/*
 * Get file size.
 * fd: File descriptor
 * Returns file size in bytes, or 0 on error.
 */
uint32_t fat_get_size(int fd);

/* Callback type for directory iteration */
typedef void (*fat_dir_callback_t)(const char *name, uint32_t size, uint8_t attr);

/*
 * Iterate over root directory entries, calling cb for each valid file.
 * cb: Callback function receiving filename (8.3 format), size, and attributes.
 * Returns 0 on success, -1 on error.
 */
int fat_list_root(fat_dir_callback_t cb);

#endif
