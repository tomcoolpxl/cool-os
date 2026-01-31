/*
 * loop.c - CPU hog test program (Proto 17)
 *
 * This program busy-loops forever without yielding.
 * Tests that preemptive scheduling allows other tasks to run.
 *
 * Expected behavior:
 * - Prints "LOOP: " messages occasionally
 * - Gets preempted after each time slice
 * - Shell should remain responsive while this runs
 */

#include <stdio.h>
#include <unistd.h>

int main(void) {
    printf("LOOP: Starting infinite loop (should be preempted)\n");

    volatile unsigned long counter = 0;
    unsigned long last_print = 0;

    for (;;) {
        counter++;

        /* Print every ~10 million iterations to show we're running */
        if (counter - last_print >= 10000000UL) {
            printf("LOOP: count=%lu (PID=%u)\n", counter, (unsigned)getpid());
            last_print = counter;
        }
    }

    /* Never reaches here */
    return 0;
}
