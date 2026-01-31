/*
 * hello.c - Test C user program
 *
 * Demonstrates the use of the minimal libc.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    printf("Hello from C!\n");
    printf("Testing: %s %d 0x%x\n", "world", 42, 255);

    /* Test string functions */
    const char *test = "Hello";
    printf("strlen(\"%s\") = %u\n", test, (unsigned int)strlen(test));

    /* Test malloc */
    char *buf = malloc(32);
    if (buf) {
        strcpy(buf, "Allocated!");
        printf("malloc test: %s\n", buf);
    } else {
        printf("malloc failed!\n");
    }

    return 0;
}
