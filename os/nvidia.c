#include "nvidia.h"
#include "gpu.h"
#include "pci.h"
#include "io.h"
#include "vga.h"
#include "klib.h"

/* ---- PCI IDs ---- */

#define NV_PCI_VENDOR 0x10DE

static const uint16_t known_devices[] = {
    0x1F82,  /* GTX 1650 (GDDR5, TU117) */
    0x1F91,  /* GTX 1650 Mobile */
    0x1F95,  /* GTX 1650 Ti Mobile */
    0x2184,  /* GTX 1650 (GDDR6, TU116) */
    0x2187,  /* GTX 1650 SUPER (TU116) */
    0
};

/* ---- MMIO register definitions (see nvidia.md for full map) ---- */

/* PMC */
#define NV_PMC_BOOT_0            0x000000
#define NV_PMC_ENABLE            0x000200

/* BAR1 — Turing uses 0xB80F40, NOT 0x001704 (GV100 and earlier) */
#define NV_BAR1_INST             0xB80F40

/* TLB invalidation — Turing-specific */
#define NV_TLB_ADDR_LO           0xB830A0
#define NV_TLB_ADDR_HI           0xB830A4
#define NV_TLB_TRIGGER           0xB830B0

/* VBIOS */
#define NV_PROM                  0x300000

/* Display engine (PDISPLAY) */
#define NV_PDISP_INST_TARGET     0x610010   /* 0x9 = enable | VRAM */
#define NV_PDISP_INST_ADDR       0x610014   /* inst_vram_addr >> 16 */
#define NV_PDISP_MASTER_CTRL     0x610078   /* bit 0 = enable */
#define NV_PDISP_OWNERSHIP       0x6254E8   /* bit 1 = owned */

/* Channel control: ctrl register = base + ctrl_id * 4 */
#define NV_PDISP_CHAN_CTRL(c)    (0x6104E0 + (c) * 4)
#define NV_PDISP_CORE_STAT       0x610630
#define NV_PDISP_WIN_STAT(w)     (0x610664 + (w) * 4)

/* Push buffer registers: base + ctrl_id * 0x10 */
#define NV_PDISP_PB_HI(c)       (0x610B20 + (c) * 0x10)
#define NV_PDISP_PB_LO(c)       (0x610B24 + (c) * 0x10)
#define NV_PDISP_PB_VALID(c)    (0x610B28 + (c) * 0x10)
#define NV_PDISP_PB_LIMIT(c)    (0x610B2C + (c) * 0x10)

/* Channel user areas — PUT pointers */
#define NV_PDISP_CORE_PUT        0x680000
#define NV_PDISP_WIN_PUT(w)      (0x690000 + (w) * 0x1000)

/* Push buffer address encoding: target=1 (VRAM) | addr >> 8 */
#define PB_ENCODE(addr)          (0x00000001 | ((addr) >> 8))

/* Push buffer size */
#define PB_SIZE                  4096

/*
 * Display instance memory layout (at VRAM 0x210000, 64KB-aligned):
 *   0x0000-0x1FFF  RAMHT (8KB, 1024 entries of 8 bytes, bits=10)
 *   0x2000-0x201F  DMA context object for VRAM access
 *
 * VRAM layout:
 *   0x000000  Page 0 framebuffer (~5MB for 1280x1024x4)
 *   0x500000  Page 1 framebuffer (~5MB)
 *   0xA00000  Core push buffer (4KB)
 *   0xA01000  Window push buffer (4KB)
 *   0xA02000  WIMM push buffer (4KB)
 *   0xA10000  Display instance memory (64KB)
 */
#define PAGE0_VRAM       0x000000
#define PAGE1_VRAM       0x500000
#define CORE_PB_VRAM     0xA00000
#define WIN_PB_VRAM      0xA01000
#define WIMM_PB_VRAM_OFF 0xA02000
#define DISP_INST_VRAM   0xA10000  /* 64KB-aligned */
#define DISP_INST_SIZE   0x10000   /* 64KB */
#define RAMHT_SIZE       0x2000    /* 8KB = 1024 entries */
#define RAMHT_BITS       10        /* log2(1024) */
#define DMA_CTX_OFFSET   0x2000    /* offset within instance memory */

/* ---- EVO method definitions (NVC57D core, NVC57E window) ---- */

/* Core channel methods (NVC57D) — per-head base = 0x2000 + head * 0x400 */
#define CORE_HEAD(h, m)                     (0x2000 + (h) * 0x400 + (m))
#define HEAD_SET_RASTER_SIZE                 0x0064
#define HEAD_SET_RASTER_SYNC_END             0x0068
#define HEAD_SET_RASTER_BLANK_END            0x006C
#define HEAD_SET_RASTER_BLANK_START          0x0070
#define HEAD_SET_CONTROL                     0x0440
#define CORE_SET_UPDATE                      0x0080

/* Core channel window methods — per-window base = 0x1000 + win * 0x80 */
#define CORE_WIN(w, m)                      (0x1000 + (w) * 0x80 + (m))
#define WINDOW_SET_CONTROL                   0x0000  /* owner head ID */
#define WINDOW_SET_FORMAT_USAGE_BOUNDS       0x0004  /* enable RGB formats */
#define WINDOW_SET_ROTATED_FORMAT_BOUNDS     0x0008
#define WINDOW_SET_USAGE_BOUNDS              0x0010  /* fetch limit, scaler */

/* Window channel methods (NVC57E) — offsets from clc57e.h */
#define WIN_SET_UPDATE                       0x0200
#define WIN_SET_SIZE                         0x0224
#define WIN_SET_STORAGE                      0x0228
#define WIN_SET_PARAMS                       0x022C
#define WIN_SET_PLANAR_STORAGE               0x0230  /* indexed: +b*4 */
#define WIN_SET_CONTEXT_DMA_ISO              0x0240  /* indexed: +b*4 */
#define WIN_SET_OFFSET                       0x0260  /* indexed: +b*4 — was 0x244! */
#define WIN_SET_POINT_IN                     0x0290  /* indexed: +b*4 */
#define WIN_SET_SIZE_IN                      0x0298
#define WIN_SET_SIZE_OUT                     0x02A4  /* was 0x29C! */
#define WIN_SET_COMPOSITION_CONTROL          0x02EC
#define WIN_SET_PRESENT_CONTROL              0x0308

/* Window Immediate channel (NVC37B) — for SET_POINT_OUT */
#define WIMM_CTRL               33
#define WIMM_SET_POINT_OUT      0x0208
#define WIMM_SET_UPDATE         0x0200

/* WIMM user area PUT pointer */
#define NV_PDISP_WIMM_PUT(w)    (0x690000 + ((WIMM_CTRL - 1) + (w)) * 0x1000)

/* VRAM for WIMM push buffer */
#define WIMM_PB_VRAM            WIMM_PB_VRAM_OFF

/* ---- State ---- */

static uint8_t  nv_bus, nv_slot, nv_func;
static volatile uint32_t *mmio;
static uint8_t  *fb_bar;
static uint32_t  fb_bar_size;
static uint16_t  device_id;
static int       detected;

/* GOP framebuffer */
static uint8_t  *gop_fb;
static uint32_t  gop_width;
static uint32_t  gop_height;
static uint32_t  gop_pitch;

/* EVO channels */
static volatile uint32_t *core_pb;
static uint32_t  core_pb_put;
static volatile uint32_t *win_pb;
static uint32_t  win_pb_put;
static int       evo_alive;
static int       win_alive;
static volatile uint32_t *wimm_pb;
static uint32_t  wimm_pb_put;

/* Page flipping state */
static uint32_t  page0_vram;    /* VRAM offset of page 0 */
static uint32_t  page1_vram;    /* VRAM offset of page 1 */
static int       display_page;  /* which page is currently displayed */

/* ---- MMIO access ---- */

static uint32_t nv_rd32(uint32_t reg) {
    return mmio[reg / 4];
}

static void nv_wr32(uint32_t reg, uint32_t val) {
    mmio[reg / 4] = val;
}

/* ---- Push buffer helpers ---- */

/*
 * C37B push buffer header format:
 *   bits 31:29 = opcode (1 = METHOD)
 *   bits 28:18 = count
 *   bits 11:2  = method >> 2 (equivalent to raw method in bits 11:0 since bit 1:0 = 0)
 *   bits 1:0   = subchannel (0)
 * Header = (1 << 29) | (count << 18) | method_offset
 */
/* Core channel uses old NV50-style headers (confirmed working in D10) */
#define CORE_PB_HDR(count, method) (((count) << 18) | ((method) >> 2))

/* Window channel: opcode 2 (bit 30) — confirmed by D29 format sweep */
#define WIN_PB_HDR(count, method) ((2 << 29) | ((count) << 18) | (method))

static void core_push(uint32_t method, uint32_t data) {
    uint32_t off = core_pb_put / 4;
    core_pb[off + 0] = CORE_PB_HDR(1, method);
    core_pb[off + 1] = data;
    core_pb_put += 8;
}

static void core_kick(void) {
    asm volatile ("sfence" ::: "memory");
    nv_wr32(NV_PDISP_CORE_PUT, core_pb_put >> 2);
}

static void win_push(uint32_t method, uint32_t data) {
    uint32_t off = win_pb_put / 4;
    win_pb[off + 0] = WIN_PB_HDR(1, method);
    win_pb[off + 1] = data;
    win_pb_put += 8;
}

static void win_push_multi(uint32_t base_method, const uint32_t *data, int count) {
    uint32_t off = win_pb_put / 4;
    win_pb[off] = WIN_PB_HDR(count, base_method);
    for (int i = 0; i < count; i++)
        win_pb[off + 1 + i] = data[i];
    win_pb_put += 4 + count * 4;
}

static void win_kick(void) {
    asm volatile ("sfence" ::: "memory");
    nv_wr32(NV_PDISP_WIN_PUT(0), win_pb_put >> 2);
}

static void win_wait_idle(void) {
    for (int i = 0; i < 200000; i++) {
        if ((nv_rd32(NV_PDISP_WIN_STAT(0)) & 0x000F0000) == 0x00040000) break;
    }
}

/* ---- Helpers ---- */

static const char *chip_name(uint32_t boot0) {
    uint32_t chipset = (boot0 >> 20) & 0x1FF;
    switch (chipset) {
    case 0x167: return "TU117";
    case 0x166: return "TU116";
    case 0x164: return "TU104";
    case 0x162: return "TU102";
    default:    return "unknown";
    }
}

static uint32_t pci_bar_size(uint8_t bus, uint8_t slot, uint8_t func, int bar) {
    uint32_t offset = 0x10 + bar * 4;
    uint32_t original = pci_read(bus, slot, func, offset);
    pci_write(bus, slot, func, offset, 0xFFFFFFFF);
    uint32_t size_mask = pci_read(bus, slot, func, offset);
    pci_write(bus, slot, func, offset, original);
    size_mask &= 0xFFFFFFF0;
    return (~size_mask) + 1;
}

/* ---- RAMHT hash function (from nouveau nvkm/core/ramht.c) ---- */

static uint32_t ramht_hash(int chid, uint32_t handle) {
    uint32_t hash = 0;
    while (handle) {
        hash ^= (handle & ((1 << RAMHT_BITS) - 1));
        handle >>= RAMHT_BITS;
    }
    hash ^= chid << (RAMHT_BITS - 4);
    return hash & ((1 << RAMHT_BITS) - 1);
}

/* ---- Display instance memory setup ---- */

static void disp_inst_init(void) {
    volatile uint32_t *inst = (volatile uint32_t *)(fb_bar + DISP_INST_VRAM);
    memset((void *)inst, 0, DISP_INST_SIZE);

    /*
     * Create a DMA context object at offset DMA_CTX_OFFSET within instance memory.
     * This is the simplest possible object — just marks VRAM as accessible.
     * The exact format depends on the DMA class, but for display:
     * word 0: flags (target + access)
     * word 1-4: base/limit
     */
    /*
     * DMA context object format (from nouveau nv50_dma_new):
     *   word 0: class | target | access
     *           class  = 0x003D (NV_DMA_IN_MEMORY)
     *           target = 0x00010000 (VRAM)
     *           access = 0x000C0000 (RDWR)
     *   word 1: limit low
     *   word 2: start low
     *   word 3: (limit_hi << 24) | start_hi
     */
    volatile uint32_t *dma = (volatile uint32_t *)(fb_bar + DISP_INST_VRAM + DMA_CTX_OFFSET);
    dma[0] = 0x000D003D;   /* class=0x3D | target=VRAM | access=RDWR */
    dma[1] = 0xFFFFFFFF;   /* limit low (all VRAM) */
    dma[2] = 0x00000000;   /* start low */
    dma[3] = 0x00000000;   /* (limit_hi << 24) | start_hi */

    /*
     * Insert RAMHT entry for this DMA context.
     *
     * The window channel calls SET_CONTEXT_DMA_ISO with a handle.
     * The display engine hashes (handle, chid) to find the RAMHT entry.
     *
     * From nouveau gv100_disp_dmac_bind:
     *   nvkm_ramht_insert(ramht, object, chid.user, -9, handle, 0x00000000)
     *
     * The -9 means: context = initial_context | (inst_offset << 9)
     * Where inst_offset is the DMA object's offset within the instance block.
     * initial_context = 0 for display DMA binds.
     *
     * So for our DMA object at offset 0x2000:
     *   context = 0x2000 << 9 = 0x00400000
     */
    uint32_t handle = 0x00000001;
    uint32_t chid = 1;  /* window channel user ID */
    uint32_t slot = ramht_hash(chid, handle);
    uint32_t context = DMA_CTX_OFFSET << 9;  /* inst_offset << 9, per nouveau */

    /* Write entry at slot * 8 bytes */
    volatile uint32_t *entry = (volatile uint32_t *)((uint8_t *)inst + slot * 8);
    entry[0] = handle;
    entry[1] = context;

    terminal_printf(" NVIDIA: RAMHT handle=0x%x slot=%d ctx=0x%x\n",
                    handle, slot, context);

    /* Tell PDISPLAY where instance memory is */
    nv_wr32(NV_PDISP_INST_TARGET, 0x00000009);  /* enable | VRAM */
    nv_wr32(NV_PDISP_INST_ADDR, DISP_INST_VRAM >> 16);

    terminal_printf(" NVIDIA: disp inst at VRAM 0x%x\n", DISP_INST_VRAM);
}

/* ---- Full display engine init (from nouveau tu102_disp_init) ---- */

static void disp_engine_init(void) {
    uint32_t tmp;
    int i, j;

    /* Ensure PDISPLAY is enabled in PMC */
    uint32_t pmc = nv_rd32(NV_PMC_ENABLE);
    if (!(pmc & 0x40000000))
        nv_wr32(NV_PMC_ENABLE, pmc | 0x40000000);

    /* Step 1: Claim display ownership */
    if (nv_rd32(0x6254E8) & 0x00000002) {
        nv_wr32(0x6254E8, nv_rd32(0x6254E8) & ~0x00000001);
        for (i = 0; i < 200000; i++) {
            if (!(nv_rd32(0x6254E8) & 0x00000002)) break;
        }
    }

    /* Step 2: Pin capabilities (TU102 uses fixed value) */
    nv_wr32(0x640008, 0x00000021);

    /* Step 3: SOR capabilities — read from hardware, write to config */
    for (i = 0; i < 4; i++) {
        tmp = nv_rd32(0x61C000 + i * 0x800);
        if (tmp == 0 || (tmp & 0xFFFF0000) == 0xBADF0000) continue;
        nv_wr32(0x640000, nv_rd32(0x640000) | (0x100 << i));
        nv_wr32(0x640144 + i * 8, tmp);
    }

    /* Step 4: Head capabilities (TU102 offsets) */
    for (i = 0; i < 2; i++) {
        tmp = nv_rd32(0x616300 + i * 0x800);
        nv_wr32(0x640048 + i * 0x20, tmp);
        for (j = 0; j < 5 * 4; j += 4) {
            tmp = nv_rd32(0x616140 + i * 0x800 + j);
            nv_wr32(0x640680 + i * 0x20 + j, tmp);
        }
    }

    /* Step 5: Window capabilities (TU102 offsets) */
    for (i = 0; i < 8; i++) {
        nv_wr32(0x640004, nv_rd32(0x640004) | (1 << i));
        for (j = 0; j < 6 * 4; j += 4) {
            tmp = nv_rd32(0x630100 + i * 0x800 + j);
            nv_wr32(0x640780 + i * 0x20 + j, tmp);
        }
        nv_wr32(0x64000C, nv_rd32(0x64000C) | 0x100);
    }

    /* Step 6: IHUB capabilities (TU102: 3 dwords) */
    for (i = 0; i < 3; i++) {
        tmp = nv_rd32(0x62E000 + i * 4);
        nv_wr32(0x640010 + i * 4, tmp);
    }

    /* Step 7: Master control enable */
    nv_wr32(0x610078, nv_rd32(0x610078) | 0x00000001);

    /* Step 8: Instance memory */
    disp_inst_init();

    /* Step 9: Interrupt masks */
    nv_wr32(0x611CF0, 0x00000187);  /* CTRL_DISP mask */
    nv_wr32(0x611DB0, 0x00000187);  /* CTRL_DISP enable */
    nv_wr32(0x611CEC, 0x00030001);  /* EXC_OTHER mask (2 heads << 16 | core) */
    nv_wr32(0x611DAC, 0x00000000);  /* EXC_OTHER enable */
    nv_wr32(0x611CE8, 0x000000FF);  /* EXC_WINIM mask (8 windows) */
    nv_wr32(0x611DA8, 0x00000000);
    nv_wr32(0x611CE4, 0x000000FF);  /* EXC_WIN mask (8 windows) */
    nv_wr32(0x611DA4, 0x00000000);
    for (i = 0; i < 2; i++) {
        nv_wr32(0x611CC0 + i * 4, 0x00000004);  /* HEAD_TIMING mask */
        nv_wr32(0x611D80 + i * 4, 0x00000000);
    }
    nv_wr32(0x611CF4, 0x00000000);  /* OR mask */
    nv_wr32(0x611DB4, 0x00000000);

    terminal_print(" NVIDIA: full disp init (tu102)\n");
}

/* ---- EVO core channel init ---- */

static int evo_core_init(void) {
    core_pb = (volatile uint32_t *)(fb_bar + CORE_PB_VRAM);
    core_pb_put = 0;
    memset((void *)core_pb, 0, PB_SIZE);

    /* Full display engine init (ownership, caps, master ctrl, interrupts) */
    disp_engine_init();

    /* Re-verify BAR1 identity mapping after display init —
     * disp_engine_init touches ownership, master control, and instance memory
     * which could theoretically trigger firmware to reprogram BAR1 */
    uint32_t bar1_post = nv_rd32(0xB80F40);
    if (bar1_post != 0)
        terminal_printf(" NVIDIA: WARNING: BAR1 changed to 0x%x after disp init!\n", bar1_post);

    terminal_printf(" NVIDIA: core PB at VRAM 0x%x\n", CORE_PB_VRAM);

    /* Disable core channel */
    nv_wr32(NV_PDISP_CHAN_CTRL(0), 0x00000000);
    for (int i = 0; i < 100000; i++) {
        if ((nv_rd32(NV_PDISP_CORE_STAT) & 0x000F0000) == 0) break;
    }

    /* Set push buffer (ctrl=0 for core) */
    nv_wr32(NV_PDISP_PB_HI(0), 0x00000000);
    nv_wr32(NV_PDISP_PB_LO(0), PB_ENCODE(CORE_PB_VRAM));
    nv_wr32(NV_PDISP_PB_VALID(0), 0x00000001);
    nv_wr32(NV_PDISP_PB_LIMIT(0), 0x00000040);

    /* Idle request → reset PUT → enable */
    nv_wr32(NV_PDISP_CHAN_CTRL(0), 0x00000010);
    for (int i = 0; i < 100000; i++) {
        if ((nv_rd32(NV_PDISP_CORE_STAT) & 0x000F0000) == 0x00040000) break;
    }
    nv_wr32(NV_PDISP_CORE_PUT, 0x00000000);
    nv_wr32(NV_PDISP_CHAN_CTRL(0), 0x00000013);

    /* Wait for alive */
    int alive = 0;
    for (int i = 0; i < 200000; i++) {
        uint32_t state = (nv_rd32(NV_PDISP_CORE_STAT) >> 16) & 0xF;
        if (state == 0x4 || state == 0xB) { alive = 1; break; }
    }

    terminal_printf(" NVIDIA: core %s (stat=0x%x)\n",
                    alive ? "ALIVE" : "FAILED", nv_rd32(NV_PDISP_CORE_STAT));
    return alive;
}

/* ---- Mode setting via core channel ---- */

static void evo_mode_set(void) {
    /* 1280x1024@60Hz VESA timing (matches UEFI GOP) */
    core_push(CORE_HEAD(0, HEAD_SET_RASTER_SIZE),        (1066 << 16) | 1688);
    core_push(CORE_HEAD(0, HEAD_SET_RASTER_SYNC_END),    (1028 << 16) | 1440);
    core_push(CORE_HEAD(0, HEAD_SET_RASTER_BLANK_END),   (39 << 16) | 48);
    core_push(CORE_HEAD(0, HEAD_SET_RASTER_BLANK_START), (1024 << 16) | 1280);
    core_push(CORE_HEAD(0, HEAD_SET_CONTROL), 0x00000001);
    core_push(CORE_SET_UPDATE, 0x00000001);
    core_kick();

    for (int i = 0; i < 200000; i++) {
        if ((nv_rd32(NV_PDISP_CORE_STAT) & 0x000F0000) == 0x00040000) break;
    }
    terminal_printf(" NVIDIA: mode set (stat=0x%x)\n", nv_rd32(NV_PDISP_CORE_STAT));
}

/* ---- Window channel init ---- */

static int evo_window_init(void) {
    win_pb = (volatile uint32_t *)(fb_bar + WIN_PB_VRAM);
    win_pb_put = 0;
    memset((void *)win_pb, 0, PB_SIZE);

    terminal_printf(" NVIDIA: win PB at VRAM 0x%x\n", WIN_PB_VRAM);

    /* Set push buffer (ctrl=1 for window 0) */
    nv_wr32(NV_PDISP_PB_HI(1), 0x00000000);
    nv_wr32(NV_PDISP_PB_LO(1), PB_ENCODE(WIN_PB_VRAM));
    nv_wr32(NV_PDISP_PB_VALID(1), 0x00000001);
    nv_wr32(NV_PDISP_PB_LIMIT(1), 0x00000040);

    /*
     * Configure window 0 on core channel.
     * These bounds are validated against the window channel's surface methods.
     * FORMAT_USAGE_BOUNDS must allow our pixel format (A8R8G8B8 = 4BPP RGB packed).
     * USAGE_BOUNDS MAX_PIXELS_FETCHED_PER_LINE must be >= surface width.
     */
    core_push(CORE_WIN(0, WINDOW_SET_CONTROL), 0x00000000);  /* owner = head 0 */
    core_push(CORE_WIN(0, WINDOW_SET_FORMAT_USAGE_BOUNDS),
              (1 << 0)   /* RGB_PACKED1BPP */
              | (1 << 1) /* RGB_PACKED2BPP */
              | (1 << 2) /* RGB_PACKED4BPP — needed for A8R8G8B8 */
              | (1 << 3) /* RGB_PACKED8BPP */);
    core_push(CORE_WIN(0, WINDOW_SET_ROTATED_FORMAT_BOUNDS), 0x00000000);
    core_push(CORE_WIN(0, WINDOW_SET_USAGE_BOUNDS),
              0x00007FFF   /* MAX_PIXELS_FETCHED_PER_LINE (must be >= width) */
              | (2 << 20)  /* INPUT_SCALER_TAPS = TAPS_2 */);
              /* Note: ILUT_ALLOWED removed — no ILUT configured */
    core_push(CORE_SET_UPDATE, 0x00000001);
    core_kick();
    for (int i = 0; i < 100000; i++) {
        if ((nv_rd32(NV_PDISP_CORE_STAT) & 0x000F0000) == 0x00040000) break;
    }
    terminal_printf(" NVIDIA: win bounds set (core stat=0x%x)\n",
                    nv_rd32(NV_PDISP_CORE_STAT));

    /* Idle request → reset PUT → enable */
    nv_wr32(NV_PDISP_CHAN_CTRL(1), 0x00000010);
    for (int i = 0; i < 100000; i++) {
        if ((nv_rd32(NV_PDISP_WIN_STAT(0)) & 0x000F0000) == 0x00040000) break;
    }
    nv_wr32(NV_PDISP_WIN_PUT(0), 0x00000000);
    nv_wr32(NV_PDISP_CHAN_CTRL(1), 0x00000013);

    /* Wait for alive */
    int alive = 0;
    for (int i = 0; i < 200000; i++) {
        uint32_t state = (nv_rd32(NV_PDISP_WIN_STAT(0)) >> 16) & 0xF;
        if (state == 0x4 || state == 0xB) { alive = 1; break; }
    }

    terminal_printf(" NVIDIA: win %s (stat=0x%x)\n",
                    alive ? "ALIVE" : "FAILED", nv_rd32(NV_PDISP_WIN_STAT(0)));
    return alive;
}

/* ---- WIMM channel (window immediate — for SET_POINT_OUT) ---- */

static void wimm_push(uint32_t method, uint32_t data) {
    uint32_t off = wimm_pb_put / 4;
    wimm_pb[off + 0] = WIN_PB_HDR(1, method);
    wimm_pb[off + 1] = data;
    wimm_pb_put += 8;
}

static void wimm_kick(void) {
    asm volatile ("sfence" ::: "memory");
    nv_wr32(NV_PDISP_WIMM_PUT(0), wimm_pb_put >> 2);
}

static int evo_wimm_init(void) {
    wimm_pb = (volatile uint32_t *)(fb_bar + WIMM_PB_VRAM);
    wimm_pb_put = 0;
    memset((void *)wimm_pb, 0, PB_SIZE);

    /* Set push buffer (ctrl=33 for WIMM) */
    nv_wr32(NV_PDISP_PB_HI(WIMM_CTRL), 0x00000000);
    nv_wr32(NV_PDISP_PB_LO(WIMM_CTRL), PB_ENCODE(WIMM_PB_VRAM));
    nv_wr32(NV_PDISP_PB_VALID(WIMM_CTRL), 0x00000001);
    nv_wr32(NV_PDISP_PB_LIMIT(WIMM_CTRL), 0x00000040);

    /* WIMM status register: 0x610664 + (ctrl-1)*4 = 0x610664 + 32*4 = 0x610744 */
    uint32_t wimm_stat_reg = 0x610664 + (WIMM_CTRL - 1) * 4;

    /* Idle request → reset PUT → enable */
    nv_wr32(NV_PDISP_CHAN_CTRL(WIMM_CTRL), 0x00000010);
    for (int i = 0; i < 100000; i++) {
        if ((nv_rd32(wimm_stat_reg) & 0x000F0000) == 0x00040000) break;
    }
    nv_wr32(NV_PDISP_WIMM_PUT(0), 0x00000000);
    nv_wr32(NV_PDISP_CHAN_CTRL(WIMM_CTRL), 0x00000013);

    int alive = 0;
    for (int i = 0; i < 200000; i++) {
        uint32_t state = (nv_rd32(wimm_stat_reg) >> 16) & 0xF;
        if (state == 0x4 || state == 0xB) { alive = 1; break; }
    }

    terminal_printf(" NVIDIA: wimm %s (stat=0x%x)\n",
                    alive ? "ALIVE" : "FAILED", nv_rd32(wimm_stat_reg));

    if (alive) {
        /* Set window position to (0,0) */
        wimm_push(WIMM_SET_POINT_OUT, 0x00000000);
        wimm_push(WIMM_SET_UPDATE, 0x00000001);
        wimm_kick();
    }

    return alive;
}

/* ---- Phase D: Surface setup and page flip ---- */

static void evo_surface_set(uint32_t vram_offset) {
    /*
     * Surface configuration for the window channel.
     *
     * The display engine validates relationships between these values:
     * - pitch must equal width * bpp (for pitch-linear)
     * - pitch must be 64-byte aligned (pitch >> 6 must round-trip)
     * - offset must be 256-byte aligned (offset >> 8 must round-trip)
     * - size_in/size_out must not exceed usage bounds (0x7FFF pixels)
     * - format must be allowed by FORMAT_USAGE_BOUNDS on core channel
     * - window must be assigned to an active head via WINDOW_SET_CONTROL
     *
     * If any constraint fails, the batch is rejected (state 5).
     */
    uint32_t width = gop_width;
    uint32_t height = gop_height;
    uint32_t pitch = gop_pitch;
    uint32_t bpp = 4;  /* A8R8G8B8 */

    /* Verify consistency before pushing */
    if (pitch != width * bpp) {
        terminal_printf(" NVIDIA: pitch %d != width*bpp %d, using width*bpp\n",
                        pitch, width * bpp);
        pitch = width * bpp;
    }
    if (pitch & 0x3F) {
        terminal_printf(" NVIDIA: pitch %d not 64B aligned\n", pitch);
    }
    if (vram_offset & 0xFF) {
        terminal_printf(" NVIDIA: offset 0x%x not 256B aligned\n", vram_offset);
    }

    terminal_printf(" NVIDIA: surface %dx%d pitch=%d off=0x%x\n",
                    width, height, pitch, vram_offset);

    /*
     * Nouveau pushes SIZE, STORAGE, PARAMS, PLANAR_STORAGE as
     * consecutive methods in one PUSH_MTHD (they're at 0x224-0x230).
     * The display engine may validate them as a group.
     *
     * Test groups of increasing size to find the minimum accepted batch.
     */

    /*
     * Push geometry as ONE multi-word command (nouveau's format).
     * Methods 0x224, 0x228, 0x22C, 0x230 are consecutive —
     * one header with count=4, then 4 data words.
     * The display engine validates them as an atomic group.
     */
    uint32_t geom[4] = {
        (height << 16) | width,   /* 0x224 SET_SIZE */
        (1 << 4),                 /* 0x228 SET_STORAGE: PITCH */
        0x000000CF,               /* 0x22C SET_PARAMS: A8R8G8B8 */
        pitch >> 6,               /* 0x230 SET_PLANAR_STORAGE */
    };
    /*
     * Single atomic batch: ALL methods, ONE update, no intermediate commits.
     * Geometry as multi-word (0x224-0x230), rest individual.
     * This is the complete surface configuration — the engine validates
     * the whole thing at once on UPDATE.
     */
    /*
     * D30: Full flip test with F3 headers + all fixes.
     * WIN_PB_HDR is now (2 << 29) | (count << 18) | method.
     * Core channel stays on old format.
     */

    int tree_pass = 0;

    #define WIN_RESET() do { \
        nv_wr32(NV_PDISP_CHAN_CTRL(1), 0x00000000); \
        for (int _i = 0; _i < 100000; _i++) \
            if ((nv_rd32(NV_PDISP_WIN_STAT(0)) & 0x000F0000) == 0) break; \
        nv_wr32(0x61102C, 0x90000000); \
        win_pb_put = 0; \
        nv_wr32(NV_PDISP_CHAN_CTRL(1), 0x00000010); \
        for (int _i = 0; _i < 100000; _i++) \
            if ((nv_rd32(NV_PDISP_WIN_STAT(0)) & 0x000F0000) == 0x00040000) break; \
        nv_wr32(NV_PDISP_WIN_PUT(0), 0x00000000); \
        nv_wr32(NV_PDISP_CHAN_CTRL(1), 0x00000013); \
        for (int _i = 0; _i < 100000; _i++) \
            if ((nv_rd32(NV_PDISP_WIN_STAT(0)) & 0x000F0000) == 0x00040000) break; \
    } while(0)

    #define WIN_CHECK(name) do { \
        win_wait_idle(); \
        uint32_t _ws = nv_rd32(NV_PDISP_WIN_STAT(0)); \
        int _pass = ((_ws >> 16) & 0xF) == 4; \
        if (_pass) { \
            terminal_printf(" %s: PASS\n", name); \
            tree_pass = 1; \
        } else { \
            uint32_t _es = nv_rd32(0x61102C); \
            uint32_t _ed = nv_rd32(0x611030); \
            uint32_t _type = (_es >> 12) & 7; \
            uint32_t _mthd = (_es & 0xFFF) << 2; \
            terminal_printf(" %s: FAIL t=%d m=0x%x d=0x%x\n", \
                            name, _type, _mthd, _ed); \
        } \
    } while(0)

    /* E1: Full nouveau flip sequence with F3 headers */
    win_push(WIN_SET_PRESENT_CONTROL, 0x00000001);
    win_push_multi(WIN_SET_SIZE, geom, 4);
    uint32_t dma_e1[2] = { 0x00000001, 0x00000001 };
    win_push_multi(WIN_SET_CONTEXT_DMA_ISO, dma_e1, 2);
    win_push(WIN_SET_OFFSET, vram_offset >> 8);
    win_push(WIN_SET_POINT_IN, 0x00000000);
    win_push(WIN_SET_SIZE_IN, (height << 16) | width);
    win_push(WIN_SET_SIZE_OUT, (height << 16) | width);
    win_push(WIN_SET_UPDATE, 0x00000001);
    win_kick();
    WIN_CHECK("E1-fullFlip");

    /* E2: If E1 passes — write test pattern and flip to page 1 */
    if (tree_pass) {
        terminal_print(" NVIDIA: SURFACE ACCEPTED!\n");

        /* Fill page 1 with bright red */
        uint32_t *p1 = (uint32_t *)(fb_bar + PAGE1_VRAM);
        uint32_t page_pixels = width * height;
        for (uint32_t i = 0; i < page_pixels; i++)
            p1[i] = 0x00FF0000;  /* red */

        /* Flip to page 1 */
        tree_pass = 0;
        win_push(WIN_SET_OFFSET, PAGE1_VRAM >> 8);
        win_push(WIN_SET_UPDATE, 0x00000001);
        win_kick();
        WIN_CHECK("E2-flipRed");
    }

    /* E3: If E2 passes — flip back to page 0 */
    if (tree_pass) {
        tree_pass = 0;
        win_push(WIN_SET_OFFSET, vram_offset >> 8);
        win_push(WIN_SET_UPDATE, 0x00000001);
        win_kick();
        WIN_CHECK("E3-flipBack");
    }

    /* E4: If E1 fails — try F4 format as fallback */
    if (!tree_pass) {
        terminal_print(" NVIDIA: trying F4 format...\n");
        WIN_RESET();

        /* F4: (2 << 29) | (count << 18) | (method >> 2) */
        #define F4_HDR(c, m) ((2 << 29) | ((c) << 18) | ((m) >> 2))

        uint32_t off = 0;
        /* PRESENT_CONTROL */
        win_pb[off++] = F4_HDR(1, WIN_SET_PRESENT_CONTROL);
        win_pb[off++] = 0x00000001;
        /* Geometry multi-word */
        win_pb[off++] = F4_HDR(4, WIN_SET_SIZE);
        win_pb[off++] = geom[0]; win_pb[off++] = geom[1];
        win_pb[off++] = geom[2]; win_pb[off++] = geom[3];
        /* CTX_DMA_ISO count=2 */
        win_pb[off++] = F4_HDR(2, WIN_SET_CONTEXT_DMA_ISO);
        win_pb[off++] = 0x00000001; win_pb[off++] = 0x00000001;
        /* OFFSET */
        win_pb[off++] = F4_HDR(1, WIN_SET_OFFSET);
        win_pb[off++] = vram_offset >> 8;
        /* POINT_IN */
        win_pb[off++] = F4_HDR(1, WIN_SET_POINT_IN);
        win_pb[off++] = 0x00000000;
        /* SIZE_IN */
        win_pb[off++] = F4_HDR(1, WIN_SET_SIZE_IN);
        win_pb[off++] = (height << 16) | width;
        /* SIZE_OUT */
        win_pb[off++] = F4_HDR(1, WIN_SET_SIZE_OUT);
        win_pb[off++] = (height << 16) | width;
        /* UPDATE */
        win_pb[off++] = F4_HDR(1, WIN_SET_UPDATE);
        win_pb[off++] = 0x00000001;

        asm volatile ("sfence" ::: "memory");
        nv_wr32(NV_PDISP_WIN_PUT(0), off);
        WIN_CHECK("E4-F4full");
        #undef F4_HDR
    }

    terminal_print(" NVIDIA: done\n");
}

static void nvidia_flip(int page) {
    uint32_t offset = page ? page1_vram : page0_vram;
    win_push(WIN_SET_OFFSET, offset >> 8);
    win_push(WIN_SET_UPDATE, 0x00000001);
    win_kick();
    win_wait_idle();
    display_page = page;
}

/* ---- Detection and init ---- */

int nvidia_detect(void) {
    for (int i = 0; known_devices[i]; i++) {
        if (pci_find_vendor(NV_PCI_VENDOR, known_devices[i],
                            &nv_bus, &nv_slot, &nv_func)) {
            device_id = known_devices[i];
            goto found;
        }
    }
    return 0;

found:
    pci_enable_device(nv_bus, nv_slot, nv_func);

    uint32_t bar0 = pci_read_bar(nv_bus, nv_slot, nv_func, 0);
    mmio = (volatile uint32_t *)(uintptr_t)(bar0 & 0xFFFFFFF0);

    uint32_t bar1 = pci_read_bar(nv_bus, nv_slot, nv_func, 1);
    fb_bar = (uint8_t *)(uintptr_t)(bar1 & 0xFFFFFFF0);
    fb_bar_size = pci_bar_size(nv_bus, nv_slot, nv_func, 1);

    uint32_t boot0 = nv_rd32(NV_PMC_BOOT_0);

    detected = 1;
    terminal_printf(" NVIDIA: %s (%x) at %d:%d.%d\n",
                    chip_name(boot0), device_id,
                    nv_bus, nv_slot, nv_func);
    terminal_printf(" NVIDIA: MMIO=0x%x BAR1=0x%x (%dMB)\n",
                    (uint32_t)(uintptr_t)mmio,
                    (uint32_t)(uintptr_t)fb_bar,
                    fb_bar_size / (1024 * 1024));

    /* Verify BAR1 identity mapping (0xB80F40 = 0 means no page tables) */
    uint32_t bar1_inst = nv_rd32(0xB80F40);
    if (bar1_inst != 0)
        terminal_printf(" NVIDIA: WARNING: BAR1 has page tables (0x%x)\n", bar1_inst);

    return 1;
}

void nvidia_init(uint32_t width, uint32_t height) {
    if (!detected) return;

    /* Capture GOP framebuffer */
    uint8_t *addr;
    uint32_t w, h, p, b;
    terminal_get_fb(&addr, &w, &h, &p, &b);
    if (addr && w > 0) {
        gop_fb = addr;
        gop_width = w;
        gop_height = h;
        gop_pitch = p;
        terminal_printf(" NVIDIA: GOP fb 0x%x %dx%d\n",
                        (uint32_t)(uintptr_t)addr, w, h);
    }

    /* Phase A: core channel (reads BAR1 instance block to find safe VRAM) */
    evo_alive = evo_core_init();

    /* Phase B: mode setting */
    if (evo_alive)
        evo_mode_set();

    /* Phase C: window channel */
    if (evo_alive)
        win_alive = evo_window_init();

    /* WIMM channel for window positioning */
    if (win_alive)
        evo_wimm_init();

    /* Phase D: surface setup with two pages for flipping */
    if (win_alive) {
        /*
         * Allocate two framebuffer pages in VRAM through BAR1.
         * GOP fb is at CPU 0xF1000000 = BAR1 + 0x11000000.
         * With identity mapping, VRAM offset = CPU - BAR1 base.
         * But 0x11000000 is past the 256MB BAR1 window.
         *
         * Instead, allocate our own pages at known safe offsets
         * within BAR1's 256MB window. Use 4MB+ to avoid
         * push buffers (0x200000) and instance memory (0x210000).
         */
        uint32_t page_size = gop_pitch * gop_height;
        terminal_printf(" NVIDIA: exp: D30 full flip F3\n");
        page0_vram = PAGE0_VRAM;
        page1_vram = PAGE1_VRAM;

        terminal_printf(" NVIDIA: page0=0x%x page1=0x%x (%dKB each)\n",
                        page0_vram, page1_vram, page_size / 1024);

        /* Clear both pages */
        memset(fb_bar + page0_vram, 0, page_size);
        memset(fb_bar + page1_vram, 0, page_size);

        /* Configure window to scan from page 0 */
        /* Surface setup */
        evo_surface_set(page0_vram);
        display_page = 0;

        terminal_printf(" NVIDIA: surface (win stat=0x%x)\n",
                        nv_rd32(NV_PDISP_WIN_STAT(0)));
    }
}

void nvidia_save_dump(void) {
    /* Reserved for future diagnostic dumps */
}

/* ---- gpu_driver interface ---- */

static uint8_t *nvidia_framebuffer(void) {
    return win_alive ? (fb_bar + page0_vram) : gop_fb;
}
static uint32_t nvidia_width(void) { return gop_width; }
static uint32_t nvidia_height(void) { return gop_height; }
static uint32_t nvidia_pitch(void) { return gop_pitch; }
static int nvidia_can_flip(void) { return 0; /* disabled until surface works */ }

static uint8_t *nvidia_page_addr(int page) {
    if (!win_alive) return gop_fb;
    return fb_bar + (page ? page1_vram : page0_vram);
}

static void nvidia_set_page(int page) {
    if (win_alive)
        nvidia_flip(page);
}

struct gpu_driver nvidia_gpu_driver = {
    .name        = "NVIDIA",
    .detect      = nvidia_detect,
    .init        = nvidia_init,
    .framebuffer = nvidia_framebuffer,
    .width       = nvidia_width,
    .height      = nvidia_height,
    .pitch       = nvidia_pitch,
    .can_flip    = nvidia_can_flip,
    .page_addr   = nvidia_page_addr,
    .set_page    = nvidia_set_page,
};
