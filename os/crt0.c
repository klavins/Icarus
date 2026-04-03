/*
 * crt0.c - C runtime startup for ICARUS user programs
 *
 * This is linked into every user program. It receives the API
 * lookup function from the kernel, initializes it, and calls main().
 */

#include "icarus_api.h"

/* The global API lookup function — referenced by include/ header macros */
icarus_lookup_fn _icarus_api;

extern int main(int argc, char **argv);

void _start(icarus_lookup_fn api, int argc, char **argv) {
    _icarus_api = api;
    main(argc, argv);
    /* If main returns, we just return to the kernel */
}
