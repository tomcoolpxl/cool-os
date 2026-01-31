/*
 * spinner.c - Fairness test program (Proto 17)
 *
 * Prints its PID repeatedly to test scheduling fairness.
 * Run multiple instances to verify they get interleaved output.
 *
 * Expected behavior:
 * - Each instance prints "S<PID> " repeatedly
 * - Multiple instances should show interleaved output
 * - Exits after a fixed number of iterations
 */

#include <stdio.h>
#include <unistd.h>

#define ITERATIONS 20

int main(void) {
    uint32_t pid = getpid();

    for (int i = 0; i < ITERATIONS; i++) {
        printf("S%u ", (unsigned)pid);

        /* Busy-wait between prints (don't yield) */
        volatile unsigned long count = 0;
        while (count < 500000UL) {
            count++;
        }
    }

    printf("\nS%u done\n", (unsigned)pid);
    return 0;
}
