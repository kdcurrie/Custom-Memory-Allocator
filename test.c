/**
 * @file
 * 
 * this is a test file
 */

#include <string.h>
#include <stdio.h>

#include "allocator.h"

/**
 * main function, makes a feww allocations and frees
 *
 * void
 */
int main(void) {
    int *a = malloc(45);
    //print_memory();
    char *b = malloc(500);
    void *c = malloc(72);
    void *d = malloc(16);

    a[0] = 45;
    //strcpy(b, "hello world, how is life, what the fuck did you say to me you little shit, I'll have you know");

    //printf("the string is: %s\n", b);
    free(c);
    free(d);
    free(a);
    free(b);

    print_memory();

    return 0;
}
