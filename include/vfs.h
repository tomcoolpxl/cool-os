#ifndef VFS_H
#define VFS_H

#include <stdint.h>

/*
 * Virtual Filesystem Layer
 *
 * Simple VFS providing a unified interface for file operations.
 * Currently dispatches all operations to the FAT32 driver.
 */

/* Maximum open files (shared across all filesystems) */
#define VFS_MAX_FD  16

/*
 * Initialize the VFS layer.
 */
void vfs_init(void);

/*
 * Open a file by path.
 * path: File path (currently only supports root directory, e.g., "INIT.ELF")
 * Returns file descriptor (>=0) on success, -1 on error.
 */
int vfs_open(const char *path);

/*
 * Read bytes from an open file.
 * fd: File descriptor from vfs_open()
 * buf: Destination buffer
 * count: Number of bytes to read
 * Returns number of bytes read, or -1 on error.
 */
int vfs_read(int fd, void *buf, uint32_t count);

/*
 * Seek to position in file.
 * fd: File descriptor
 * offset: Absolute byte offset from start of file
 * Returns 0 on success, -1 on error.
 */
int vfs_seek(int fd, uint32_t offset);

/*
 * Close an open file.
 * fd: File descriptor
 * Returns 0 on success, -1 on error.
 */
int vfs_close(int fd);

/*
 * Get file size.
 * fd: File descriptor
 * Returns file size in bytes, or 0 on error.
 */
uint32_t vfs_size(int fd);

#endif
