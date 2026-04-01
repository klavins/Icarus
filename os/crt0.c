/*
 * crt0.c - C runtime startup for ICARUS user programs
 *
 * This is linked into every user program. It receives the API
 * lookup function from the kernel, initializes it, and calls main().
 */

#include "icarus_api.h"

/* The global API lookup function — referenced by icarus.h macros */
icarus_lookup_fn _icarus_api;

extern int main(void);

void _start(icarus_lookup_fn api) {
    _icarus_api = api;
    main();
    /* If main returns, we just return to the kernel */
}
