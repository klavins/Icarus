/*
 * uefi_boot64.c - 64-bit UEFI bootloader with GOP framebuffer setup
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

#include <efi.h>
#include <efilib.h>
#include <stdint.h>

/*
 * Framebuffer info — stored at a fixed address so the 32-bit kernel can find it.
 * We use 0x00080000 (512KB) which is safe conventional memory.
 */
#define FB_INFO_ADDR 0x00080000

struct framebuffer_info {
    uint32_t addr_lo;   /* framebuffer address (low 32 bits) */
    uint32_t width;
    uint32_t height;
    uint32_t pitch;     /* pixels per scan line */
    uint32_t bpp;       /* bytes per pixel */
    uint32_t total_memory_kb;
    char     firmware_vendor[64];
    uint32_t firmware_revision;
    uint32_t pixel_format;
    uint32_t heap_base;
    uint32_t heap_size;
};

/* kernel_main — compiled as 64-bit for this build */
extern void kernel_main(uint32_t magic, void *info);

/* Convert UCS-2 to ASCII */
static void ucs2_to_ascii(const CHAR16 *src, char *dst, int max) {
    int i;
    for (i = 0; src[i] && i < max - 1; i++)
        dst[i] = (char)(src[i] & 0x7F);
    dst[i] = '\0';
}

/* Sum usable memory from UEFI memory map */
static uint32_t sum_memory(uint8_t *memmap, UINTN mapSize, UINTN descSize) {
    uint64_t total = 0;
    for (UINTN off = 0; off < mapSize; off += descSize) {
        EFI_MEMORY_DESCRIPTOR *desc = (EFI_MEMORY_DESCRIPTOR *)(memmap + off);
        if (desc->Type == EfiConventionalMemory ||
            desc->Type == EfiBootServicesCode ||
            desc->Type == EfiBootServicesData) {
            total += desc->NumberOfPages * 4096;
        }
    }
    return (uint32_t)(total / 1024);
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    InitializeLib(ImageHandle, SystemTable);
    EFI_BOOT_SERVICES *BS = SystemTable->BootServices;

    Print(L"ICARUS UEFI boot (x64)...\r\n");
    Print(L"Looking for GOP...\r\n");

    /* Get GOP */
    EFI_GUID gopGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;
    Print(L"Calling LocateProtocol...\r\n");
    EFI_STATUS status = uefi_call_wrapper(BS->LocateProtocol, 3, &gopGuid, NULL, (void **)&gop);
    Print(L"LocateProtocol returned: %d\r\n", status);
    if (status != EFI_SUCCESS || !gop) {
        Print(L"ERROR: GOP not found (status=%d)\r\n", status);
        Print(L"Halting.\r\n");
        for (;;) ;
    }

    /* Find the best resolution mode (capped so BGA can double-buffer in 16MB VRAM) */
    {
        UINTN numModes = gop->Mode->MaxMode;
        UINTN bestMode = gop->Mode->Mode;  /* default to current */
        UINTN bestPixels = 0;
        for (UINTN m = 0; m < numModes; m++) {
            EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info;
            UINTN infoSize;
            status = uefi_call_wrapper(gop->QueryMode, 4, gop, m, &infoSize, &info);
            if (status != EFI_SUCCESS) continue;
            if (info->PixelFormat == PixelBltOnly) continue;
            UINTN w = info->HorizontalResolution;
            UINTN h = info->VerticalResolution;
            Print(L"  Mode %d: %dx%d\r\n", m, w, h);
            if (w > 1920 || h > 1200) continue;
            UINTN pixels = w * h;
            if (pixels > bestPixels) {
                bestPixels = pixels;
                bestMode = m;
            }
        }
        if (bestMode != gop->Mode->Mode) {
            Print(L"Switching to mode %d...\r\n", bestMode);
            uefi_call_wrapper(gop->SetMode, 2, gop, bestMode);
        }
    }

    Print(L"Display: %dx%d\r\n",
          gop->Mode->Info->HorizontalResolution,
          gop->Mode->Info->VerticalResolution);

    /* Store framebuffer info at fixed address */
    struct framebuffer_info *fb = (struct framebuffer_info *)FB_INFO_ADDR;
    fb->addr_lo  = (uint32_t)(UINTN)gop->Mode->FrameBufferBase;
    fb->width    = gop->Mode->Info->HorizontalResolution;
    fb->height   = gop->Mode->Info->VerticalResolution;
    fb->pitch    = gop->Mode->Info->PixelsPerScanLine;
    fb->bpp      = 4;
    fb->pixel_format = gop->Mode->Info->PixelFormat;
    fb->firmware_revision = SystemTable->FirmwareRevision;
    ucs2_to_ascii(SystemTable->FirmwareVendor, fb->firmware_vendor, 64);

    Print(L"Framebuffer at 0x%x, %dx%d\r\n",
          (UINTN)gop->Mode->FrameBufferBase,
          gop->Mode->Info->HorizontalResolution,
          gop->Mode->Info->VerticalResolution);

    /* Get memory map */
    UINTN mapSize = 0, mapKey = 0, descSize = 0;
    UINT32 descVer = 0;
    UINT8 memmap[16384];

    mapSize = sizeof(memmap);
    status = uefi_call_wrapper(BS->GetMemoryMap, 5,
                               &mapSize, (EFI_MEMORY_DESCRIPTOR *)memmap,
                               &mapKey, &descSize, &descVer);
    if (status != EFI_SUCCESS) {
        Print(L"ERROR: GetMemoryMap failed (%d)\r\n", status);
        for (;;) ;
    }

    fb->total_memory_kb = sum_memory(memmap, mapSize, descSize);

    /* Find largest conventional memory region above 1MB for kernel heap */
    {
        uint64_t best_base = 0, best_size = 0;
        for (UINTN off = 0; off < mapSize; off += descSize) {
            EFI_MEMORY_DESCRIPTOR *desc = (EFI_MEMORY_DESCRIPTOR *)(memmap + off);
            if (desc->Type == EfiConventionalMemory) {
                uint64_t base = desc->PhysicalStart;
                uint64_t size = desc->NumberOfPages * 4096;
                /* Skip regions below 1MB (used by kernel/BIOS) */
                if (base < 0x100000) continue;
                /* Use portion above 16MB to avoid kernel code/data */
                if (base < 0x1000000) {
                    if (base + size <= 0x1000000) continue;
                    size -= (0x1000000 - base);
                    base = 0x1000000;
                }
                /* Keep within 32-bit addressable range */
                if (base >= 0x100000000ULL) continue;
                if (base + size > 0x100000000ULL)
                    size = 0x100000000ULL - base;
                if (size > best_size) {
                    best_base = base;
                    best_size = size;
                }
            }
        }
        fb->heap_base = (uint32_t)best_base;
        fb->heap_size = (uint32_t)best_size;
        Print(L"Heap: 0x%x, %d MB\r\n", (UINTN)best_base, (UINTN)(best_size / (1024*1024)));
    }

    Print(L"Memory: %d MB\r\n", fb->total_memory_kb / 1024);
    Print(L"Exiting boot services...\r\n");

    status = uefi_call_wrapper(BS->ExitBootServices, 2, ImageHandle, mapKey);
    if (status != EFI_SUCCESS) {
        mapSize = sizeof(memmap);
        uefi_call_wrapper(BS->GetMemoryMap, 5,
                          &mapSize, (EFI_MEMORY_DESCRIPTOR *)memmap,
                          &mapKey, &descSize, &descVer);
        fb->total_memory_kb = sum_memory(memmap, mapSize, descSize);
        status = uefi_call_wrapper(BS->ExitBootServices, 2, ImageHandle, mapKey);
        if (status != EFI_SUCCESS) {
            for (;;) ;
        }
    }

    /* Call kernel directly in 64-bit mode */
    kernel_main(0, (void *)0);

    for (;;) asm volatile ("hlt");
    return EFI_SUCCESS;
}
