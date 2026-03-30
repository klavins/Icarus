/*
 * uefi_boot.c - UEFI bootloader for 32-bit kernel entry
 *
 * Copyright (C) 2026 Eric Klavins
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#ifdef GNU_EFI
#include <efi.h>
#include <efilib.h>
#else
#include "uefi.h"
#endif

#include <stdint.h>

/*
 * Framebuffer info passed to the kernel.
 */
struct framebuffer_info {
    uint32_t *addr;
    uint32_t  width;
    uint32_t  height;
    uint32_t  pitch;    /* pixels per scan line */
    uint32_t  bpp;      /* bytes per pixel */
};

/*
 * Boot info collected from UEFI, passed to the kernel.
 */
struct uefi_boot_info {
    uint32_t fb_addr;
    uint32_t fb_width;
    uint32_t fb_height;
    uint32_t fb_pitch;
    uint32_t fb_bpp;
    uint32_t total_memory_kb;
    char     firmware_vendor[64];
    uint32_t firmware_revision;
    uint32_t pixel_format;
};

/* Globals read by fbconsole and kernel */
struct framebuffer_info fb_info;
struct uefi_boot_info uefi_info;

/* Provided by kernel.c */
extern void kernel_main(uint32_t magic, void *info);

/* Convert UCS-2 to ASCII (lossy) */
static void ucs2_to_ascii(const CHAR16 *src, char *dst, int max) {
    int i;
    for (i = 0; src[i] && i < max - 1; i++)
        dst[i] = (char)(src[i] & 0x7F);
    dst[i] = '\0';
}

/* Sum usable memory from the UEFI memory map */
static uint32_t sum_memory(uint8_t *memmap, UINTN mapSize, UINTN descSize) {
    uint64_t total = 0;
    for (UINTN off = 0; off < mapSize; off += descSize) {
        EFI_MEMORY_DESCRIPTOR *desc = (EFI_MEMORY_DESCRIPTOR *)(memmap + off);
        /* Count conventional and boot services memory */
        if (desc->Type == 7 ||  /* EfiConventionalMemory */
            desc->Type == 3 ||  /* EfiBootServicesCode */
            desc->Type == 4)    /* EfiBootServicesData */
        {
            total += desc->NumberOfPages * 4096;
        }
    }
    return (uint32_t)(total / 1024);
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {

#ifdef GNU_EFI
    InitializeLib(ImageHandle, SystemTable);
#endif

    EFI_BOOT_SERVICES *BS = SystemTable->BootServices;

    SystemTable->ConOut->OutputString(SystemTable->ConOut, (CHAR16 *)L"ICARUS UEFI boot...\r\n");

    /* Collect firmware info */
    ucs2_to_ascii(SystemTable->FirmwareVendor, uefi_info.firmware_vendor, 64);
    uefi_info.firmware_revision = SystemTable->FirmwareRevision;

    /* Locate the Graphics Output Protocol */
    EFI_GUID gopGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = 0;
    EFI_STATUS status = BS->LocateProtocol(&gopGuid, 0, (void **)&gop);
    if (status != EFI_SUCCESS || !gop) {
        SystemTable->ConOut->OutputString(SystemTable->ConOut,
            (CHAR16 *)L"ERROR: Could not find GOP\r\n");
        for (;;) ;
    }

    /* Store framebuffer info */
    fb_info.addr   = (uint32_t *)(UINTN)gop->Mode->FrameBufferBase;
    fb_info.width  = gop->Mode->Info->HorizontalResolution;
    fb_info.height = gop->Mode->Info->VerticalResolution;
    fb_info.pitch  = gop->Mode->Info->PixelsPerScanLine;
    fb_info.bpp    = 4;

    uefi_info.fb_addr   = (uint32_t)(UINTN)fb_info.addr;
    uefi_info.fb_width  = fb_info.width;
    uefi_info.fb_height = fb_info.height;
    uefi_info.fb_pitch  = fb_info.pitch;
    uefi_info.fb_bpp    = fb_info.bpp;
    uefi_info.pixel_format = gop->Mode->Info->PixelFormat;

    /* Get memory map and exit boot services */
    UINTN mapSize = 0, mapKey = 0, descSize = 0;
    UINT32 descVer = 0;
    UINT8 memmap[8192];

    mapSize = sizeof(memmap);
    status = BS->GetMemoryMap(&mapSize, (EFI_MEMORY_DESCRIPTOR *)memmap, &mapKey, &descSize, &descVer);
    if (status != EFI_SUCCESS) {
        SystemTable->ConOut->OutputString(SystemTable->ConOut,
            (CHAR16 *)L"ERROR: GetMemoryMap failed\r\n");
        for (;;) ;
    }

    /* Sum up memory before exiting boot services */
    uefi_info.total_memory_kb = sum_memory(memmap, mapSize, descSize);

    status = BS->ExitBootServices(ImageHandle, mapKey);
    if (status != EFI_SUCCESS) {
        mapSize = sizeof(memmap);
        BS->GetMemoryMap(&mapSize, (EFI_MEMORY_DESCRIPTOR *)memmap, &mapKey, &descSize, &descVer);
        uefi_info.total_memory_kb = sum_memory(memmap, mapSize, descSize);
        status = BS->ExitBootServices(ImageHandle, mapKey);
        if (status != EFI_SUCCESS) {
            for (;;) ;
        }
    }

    /* Call the kernel */
    kernel_main(0, (void *)0);

    for (;;) ;
    return EFI_SUCCESS;
}
