#ifndef BOOT_INFO_H
#define BOOT_INFO_H

#include <stdint.h>
#include "multiboot.h"

void boot_info_print(uint32_t magic, struct multiboot_info *mboot);

#endif
