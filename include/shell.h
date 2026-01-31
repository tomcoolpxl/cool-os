#ifndef SHELL_H
#define SHELL_H

#define SHELL_MAX_LINE   128    /* Maximum input line length */
#define SHELL_MAX_ARGS   8      /* Maximum arguments per command */

/* Shell command return codes */
#define SHELL_OK           0    /* Command executed successfully */
#define SHELL_ERR_EMPTY   -1    /* Empty command line */
#define SHELL_ERR_UNKNOWN -2    /* Unknown command */
#define SHELL_ERR_ARGS    -3    /* Invalid arguments */
#define SHELL_ERR_FILE    -4    /* File operation failed */

/* Initialize and start the shell (called from kmain) */
void shell_init(void);

/* Shell main loop (runs as kernel task) */
void shell_main(void);

#ifdef REGTEST_BUILD
/* Execute a command directly (for testing, bypasses keyboard input) */
int shell_exec(const char *cmdline);

/* Parse a command line into argc/argv (for testing) */
int shell_parse_line(const char *line, int *argc, char **argv, char *buf, int bufsize);
#endif

#endif
