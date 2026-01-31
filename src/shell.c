#include <stddef.h>
#include "shell.h"
#include "console.h"
#include "kbd.h"
#include "vfs.h"
#include "fat32.h"
#include "task.h"
#include "scheduler.h"
#include "heap.h"
#include "serial.h"
#include "framebuffer.h"

/*
 * Kernel Shell
 *
 * Interactive command interface providing:
 * - Filesystem inspection (ls, cat)
 * - Program execution (run)
 * - Screen control (clear, help)
 */

/* Forward declarations for command handlers (return SHELL_OK or error code) */
static int cmd_help(int argc, char **argv);
static int cmd_clear(int argc, char **argv);
static int cmd_ls(int argc, char **argv);
static int cmd_cat(int argc, char **argv);
static int cmd_run(int argc, char **argv);

/* Command table entry */
typedef struct {
    const char *name;
    const char *help;
    int (*handler)(int argc, char **argv);
} shell_cmd_t;

/* Command table */
static const shell_cmd_t commands[] = {
    {"help",  "List available commands",      cmd_help},
    {"clear", "Clear the screen",             cmd_clear},
    {"ls",    "List files in root directory", cmd_ls},
    {"cat",   "Display file contents",        cmd_cat},
    {"run",   "Execute an ELF program",       cmd_run},
    {NULL, NULL, NULL}  /* Sentinel */
};

/* String comparison (returns 0 if equal) */
static int shell_strcmp(const char *a, const char *b) {
    while (*a && *b && *a == *b) {
        a++;
        b++;
    }
    return *a - *b;
}

/* Parse input line into argc/argv */
static int shell_parse(char *line, int *argc, char **argv) {
    *argc = 0;
    char *p = line;

    while (*p && *argc < SHELL_MAX_ARGS) {
        /* Skip leading spaces */
        while (*p == ' ') p++;
        if (*p == '\0') break;

        /* Mark start of token */
        argv[(*argc)++] = p;

        /* Find end of token */
        while (*p && *p != ' ') p++;

        /* Null-terminate token */
        if (*p) *p++ = '\0';
    }
    return *argc;
}

/* Dispatch command to appropriate handler */
static int shell_dispatch(int argc, char **argv) {
    if (argc == 0) {
        return SHELL_ERR_EMPTY;
    }

    /* Search command table */
    for (int i = 0; commands[i].name != NULL; i++) {
        if (shell_strcmp(argv[0], commands[i].name) == 0) {
            return commands[i].handler(argc, argv);
        }
    }

    /* Unknown command */
    console_puts("Unknown command: ");
    console_puts(argv[0]);
    console_puts("\n");
    return SHELL_ERR_UNKNOWN;
}

/* --- Command Handlers --- */

static int cmd_help(int argc, char **argv) {
    (void)argc;
    (void)argv;

    console_puts("Available commands:\n");
    for (int i = 0; commands[i].name != NULL; i++) {
        console_puts("  ");
        console_puts(commands[i].name);
        console_puts(" - ");
        console_puts(commands[i].help);
        console_puts("\n");
    }
    return SHELL_OK;
}

static int cmd_clear(int argc, char **argv) {
    (void)argc;
    (void)argv;

    console_clear();
    return SHELL_OK;
}

/* Callback for ls command */
static void ls_callback(const char *name, uint32_t size, uint8_t attr) {
    /* Print filename */
    console_puts(name);

    /* Pad to column 16 */
    int len = 0;
    for (const char *p = name; *p; p++) len++;
    for (int i = len; i < 16; i++) {
        console_putc(' ');
    }

    /* Print directory indicator or size */
    if (attr & FAT_ATTR_DIRECTORY) {
        console_puts("<DIR>");
    } else {
        console_print_dec(size);
    }
    console_puts("\n");
}

static int cmd_ls(int argc, char **argv) {
    (void)argc;
    (void)argv;

    if (fat_list_root(ls_callback) != 0) {
        console_puts("Error: Failed to list directory\n");
        return SHELL_ERR_FILE;
    }
    return SHELL_OK;
}

static int cmd_cat(int argc, char **argv) {
    if (argc < 2) {
        console_puts("Usage: cat <filename>\n");
        return SHELL_ERR_ARGS;
    }

    int fd = vfs_open(argv[1]);
    if (fd < 0) {
        console_puts("File not found: ");
        console_puts(argv[1]);
        console_puts("\n");
        return SHELL_ERR_FILE;
    }

    uint32_t size = vfs_size(fd);
    if (size == 0) {
        console_puts("(empty file)\n");
        vfs_close(fd);
        return SHELL_OK;
    }

    /* Cap at 64KB for safety */
    if (size > 65536) {
        size = 65536;
        console_puts("(truncated to 64KB)\n");
    }

    /* Allocate buffer */
    uint8_t *buf = kmalloc(size);
    if (buf == NULL) {
        console_puts("Error: Out of memory\n");
        vfs_close(fd);
        return SHELL_ERR_FILE;
    }

    /* Read file */
    int bytes_read = vfs_read(fd, buf, size);
    if (bytes_read < 0) {
        console_puts("Error: Read failed\n");
        kfree(buf);
        vfs_close(fd);
        return SHELL_ERR_FILE;
    }

    /* Output content (filter non-printable chars) */
    for (int i = 0; i < bytes_read; i++) {
        char c = buf[i];
        if (c >= 32 && c < 127) {
            console_putc(c);
        } else if (c == '\n' || c == '\r' || c == '\t') {
            console_putc(c);
        } else {
            console_putc('.');  /* Replace non-printable */
        }
    }
    console_puts("\n");

    kfree(buf);
    vfs_close(fd);
    return SHELL_OK;
}

static int cmd_run(int argc, char **argv) {
    if (argc < 2) {
        console_puts("Usage: run <program.elf>\n");
        return SHELL_ERR_ARGS;
    }

    /* Convert filename to uppercase for FAT32 compatibility */
    char upper_name[SHELL_MAX_LINE];
    int i;
    for (i = 0; argv[1][i] && i < SHELL_MAX_LINE - 1; i++) {
        char c = argv[1][i];
        if (c >= 'a' && c <= 'z') {
            c -= 32;  /* Convert to uppercase */
        }
        upper_name[i] = c;
    }
    upper_name[i] = '\0';

    task_t *task = task_create_from_path(upper_name);
    if (task == NULL) {
        console_puts("Failed to load: ");
        console_puts(upper_name);
        console_puts("\n");
        return SHELL_ERR_FILE;
    }

    console_puts("Started: ");
    console_puts(upper_name);
    console_puts("\n");
    return SHELL_OK;
}

/* Shell main loop */
void shell_main(void) {
    char line[SHELL_MAX_LINE];
    char *argv[SHELL_MAX_ARGS];
    int argc;

    serial_puts("shell: Shell task started\n");

    while (1) {
        console_puts("> ");
        fb_present();  /* Show prompt immediately */

        size_t len = kbd_readline(line, SHELL_MAX_LINE);
        if (len == 0) {
            /* Empty line, just yield and continue */
            task_yield();
            continue;
        }

        argc = shell_parse(line, &argc, argv);
        (void)shell_dispatch(argc, argv);  /* Ignore return in interactive mode */
        fb_present();  /* Show command output */

        /* Yield to allow any spawned tasks to run */
        task_yield();
    }
}

void shell_init(void) {
    serial_puts("shell: Initializing kernel shell\n");

    task_t *shell_task = task_create(shell_main);
    if (shell_task == NULL) {
        serial_puts("shell: Failed to create shell task\n");
        return;
    }

    scheduler_add(shell_task);
    serial_puts("shell: Shell task created and scheduled\n");
}

#ifdef REGTEST_BUILD
/*
 * Parse a command line into argc/argv for testing.
 * Uses provided buffer for token storage to avoid modifying original line.
 * Returns argc.
 */
int shell_parse_line(const char *line, int *argc, char **argv, char *buf, int bufsize) {
    /* Copy line to mutable buffer */
    int i;
    for (i = 0; line[i] && i < bufsize - 1; i++) {
        buf[i] = line[i];
    }
    buf[i] = '\0';

    /* Parse using existing function */
    return shell_parse(buf, argc, argv);
}

/*
 * Execute a command directly for testing (bypasses keyboard input).
 * Returns SHELL_OK on success, or an error code.
 */
int shell_exec(const char *cmdline) {
    char buf[SHELL_MAX_LINE];
    char *argv[SHELL_MAX_ARGS];
    int argc;

    /* Copy to mutable buffer */
    int i;
    for (i = 0; cmdline[i] && i < SHELL_MAX_LINE - 1; i++) {
        buf[i] = cmdline[i];
    }
    buf[i] = '\0';

    /* Parse and dispatch */
    shell_parse(buf, &argc, argv);
    return shell_dispatch(argc, argv);
}
#endif /* REGTEST_BUILD */
