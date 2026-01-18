#include <stddef.h>
#include "vfs.h"
#include "fat32.h"
#include "serial.h"

/*
 * Virtual Filesystem Layer
 *
 * Currently a thin wrapper around FAT32.
 * Could be extended to support multiple filesystems/mount points.
 */

/* File descriptor table - maps VFS fd to underlying FAT fd */
static struct {
    int in_use;
    int fat_fd;  /* Underlying FAT32 file descriptor */
} vfs_fds[VFS_MAX_FD];

void vfs_init(void) {
    serial_puts("vfs: Initializing\n");

    /* Clear file descriptor table */
    for (int i = 0; i < VFS_MAX_FD; i++) {
        vfs_fds[i].in_use = 0;
        vfs_fds[i].fat_fd = -1;
    }

    serial_puts("vfs: Initialized\n");
}

int vfs_open(const char *path) {
    if (path == NULL) {
        return -1;
    }

    /* Find free VFS descriptor */
    int vfd = -1;
    for (int i = 0; i < VFS_MAX_FD; i++) {
        if (!vfs_fds[i].in_use) {
            vfd = i;
            break;
        }
    }
    if (vfd < 0) {
        serial_puts("vfs: No free file descriptors\n");
        return -1;
    }

    /* Dispatch to FAT32 */
    int fat_fd = fat_open(path);
    if (fat_fd < 0) {
        return -1;
    }

    vfs_fds[vfd].in_use = 1;
    vfs_fds[vfd].fat_fd = fat_fd;

    return vfd;
}

int vfs_read(int fd, void *buf, uint32_t count) {
    if (fd < 0 || fd >= VFS_MAX_FD || !vfs_fds[fd].in_use) {
        return -1;
    }

    return fat_read(vfs_fds[fd].fat_fd, buf, count);
}

int vfs_seek(int fd, uint32_t offset) {
    if (fd < 0 || fd >= VFS_MAX_FD || !vfs_fds[fd].in_use) {
        return -1;
    }

    return fat_seek(vfs_fds[fd].fat_fd, offset);
}

int vfs_close(int fd) {
    if (fd < 0 || fd >= VFS_MAX_FD || !vfs_fds[fd].in_use) {
        return -1;
    }

    int ret = fat_close(vfs_fds[fd].fat_fd);
    vfs_fds[fd].in_use = 0;
    vfs_fds[fd].fat_fd = -1;

    return ret;
}

uint32_t vfs_size(int fd) {
    if (fd < 0 || fd >= VFS_MAX_FD || !vfs_fds[fd].in_use) {
        return 0;
    }

    return fat_get_size(vfs_fds[fd].fat_fd);
}
