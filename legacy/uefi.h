#ifndef UEFI_H
#define UEFI_H

#include <stdint.h>
#include <stddef.h>

/*
 * Minimal UEFI type definitions — just enough to boot and get a framebuffer.
 * Based on the UEFI 2.x specification for IA32 (32-bit).
 */

typedef uint32_t UINTN;
typedef int32_t  INTN;
typedef void    *EFI_HANDLE;
typedef UINTN    EFI_STATUS;
typedef uint8_t  BOOLEAN;
typedef uint16_t CHAR16;
typedef uint64_t EFI_PHYSICAL_ADDRESS;

#define EFIAPI __attribute__((ms_abi))

#define EFI_SUCCESS 0

/* GUIDs */
typedef struct {
    uint32_t Data1;
    uint16_t Data2;
    uint16_t Data3;
    uint8_t  Data4[8];
} EFI_GUID;

#define EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID \
    {0x9042a9de,0x23dc,0x4a38,{0x96,0xfb,0x7a,0xde,0xd0,0x80,0x51,0x6a}}

/* Graphics Output Protocol */
typedef enum {
    PixelRedGreenBlueReserved8BitPerColor,
    PixelBlueGreenRedReserved8BitPerColor,
    PixelBitMask,
    PixelBltOnly,
    PixelFormatMax
} EFI_GRAPHICS_PIXEL_FORMAT;

typedef struct {
    uint32_t                   Version;
    uint32_t                   HorizontalResolution;
    uint32_t                   VerticalResolution;
    EFI_GRAPHICS_PIXEL_FORMAT  PixelFormat;
    uint32_t                   RedMask;
    uint32_t                   GreenMask;
    uint32_t                   BlueMask;
    uint32_t                   ReservedMask;
    uint32_t                   PixelsPerScanLine;
} EFI_GRAPHICS_OUTPUT_MODE_INFORMATION;

typedef struct {
    uint32_t                              MaxMode;
    uint32_t                              Mode;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;
    UINTN                                 SizeOfInfo;
    EFI_PHYSICAL_ADDRESS                  FrameBufferBase;
    UINTN                                 FrameBufferSize;
} EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE;

typedef struct EFI_GRAPHICS_OUTPUT_PROTOCOL EFI_GRAPHICS_OUTPUT_PROTOCOL;

struct EFI_GRAPHICS_OUTPUT_PROTOCOL {
    void                              *QueryMode;
    void                              *SetMode;
    void                              *Blt;
    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *Mode;
};

/* Boot Services (subset) */
typedef enum {
    AllHandles,
    ByRegisterNotify,
    ByProtocol
} EFI_LOCATE_SEARCH_TYPE;

typedef struct {
    char     _pad1[40];                     /* table header */
    void    *RaiseTPL;
    void    *RestoreTPL;
    void    *AllocatePages;
    void    *FreePages;
    EFI_STATUS (EFIAPI *GetMemoryMap)(
        UINTN *MemoryMapSize,
        void  *MemoryMap,
        UINTN *MapKey,
        UINTN *DescriptorSize,
        uint32_t *DescriptorVersion
    );
    void    *AllocatePool;
    void    *FreePool;
    void    *CreateEvent;
    void    *SetTimer;
    void    *WaitForEvent;
    void    *SignalEvent;
    void    *CloseEvent;
    void    *CheckEvent;
    void    *InstallProtocolInterface;
    void    *ReinstallProtocolInterface;
    void    *UninstallProtocolInterface;
    void    *HandleProtocol;
    void    *Reserved;
    void    *RegisterProtocolNotify;
    void    *LocateHandle;
    void    *LocateDevicePath;
    void    *InstallConfigurationTable;
    void    *LoadImage;
    void    *StartImage;
    void    *Exit;
    void    *UnloadImage;
    EFI_STATUS (EFIAPI *ExitBootServices)(
        EFI_HANDLE ImageHandle,
        UINTN      MapKey
    );
    /* ... more follows but we don't need it */
    char    _pad2[24];
    void    *SetWatchdogTimer;
    char    _pad3[8];
    void    *OpenProtocol;
    void    *CloseProtocol;
    void    *OpenProtocolInformation;
    void    *ProtocolsPerHandle;
    void    *LocateHandleBuffer;
    EFI_STATUS (EFIAPI *LocateProtocol)(
        EFI_GUID *Protocol,
        void     *Registration,
        void    **Interface
    );
} EFI_BOOT_SERVICES;

/* Simple Text Output Protocol (for early debug) */
typedef struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;

struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL {
    void *Reset;
    EFI_STATUS (EFIAPI *OutputString)(
        EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
        CHAR16                         *String
    );
    /* ... more follows */
};

/* System Table */
typedef struct {
    char                              _pad1[44];  /* table header */
    CHAR16                           *FirmwareVendor;
    uint32_t                          FirmwareRevision;
    EFI_HANDLE                        ConsoleInHandle;
    void                             *ConIn;
    EFI_HANDLE                        ConsoleOutHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL   *ConOut;
    EFI_HANDLE                        StandardErrorHandle;
    void                             *StdErr;
    void                             *RuntimeServices;
    EFI_BOOT_SERVICES                *BootServices;
} EFI_SYSTEM_TABLE;

#endif
