#ifndef PAT_H
#define PAT_H

#include <stdint.h>

/* Set up PAT with Write-Combining in slot 1 */
void pat_init(void);

/* Mark a physical address range as Write-Combining in the page tables */
void pat_set_write_combining(uint64_t phys_addr, uint64_t size);

#endif
