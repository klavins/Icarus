#include "nvidia.h"
#include "gpu.h"
#include "pci.h"
/* pat.h and io.h not directly needed — PAT WC is set by gpu_init */
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

/* ---- MMIO registers (see nvidia.md for full map) ---- */

#define NV_PMC_BOOT_0            0x000000
#define NV_PMC_ENABLE            0x000200
/* NV_BAR1_INST (0xB80F40) — reads as 0 on TU117 (identity mapping) */

#define NV_PDISP_INST_TARGET     0x610010
#define NV_PDISP_INST_ADDR       0x610014
#define NV_PDISP_MASTER_CTRL     0x610078
#define NV_PDISP_OWNERSHIP       0x6254E8

#define NV_PDISP_CHAN_CTRL(c)    (0x6104E0 + (c) * 4)
#define NV_PDISP_CORE_STAT       0x610630
#define NV_PDISP_WIN_STAT(w)     (0x610664 + (w) * 4)

#define NV_PDISP_PB_HI(c)       (0x610B20 + (c) * 0x10)
#define NV_PDISP_PB_LO(c)       (0x610B24 + (c) * 0x10)
#define NV_PDISP_PB_VALID(c)    (0x610B28 + (c) * 0x10)
#define NV_PDISP_PB_LIMIT(c)    (0x610B2C + (c) * 0x10)

#define NV_PDISP_CORE_PUT        0x680000
#define NV_PDISP_WIN_PUT(w)      (0x690000 + (w) * 0x1000)

#define PB_ENCODE(addr)          (0x00000001 | ((addr) >> 8))
#define PB_SIZE                  4096

/* ---- Push buffer header formats ---- */

/* Core channel: old NV50-style (no opcode bits) */
#define CORE_HDR(count, method)  (((count) << 18) | ((method) >> 2))

/* Window/WIMM channels: opcode 2 (bit 30), confirmed by D29 format sweep */
#define WIN_HDR(count, method)   ((2 << 29) | ((count) << 18) | (method))

/* ---- EVO methods ---- */

/* Core channel (NVC57D) — per-head stride 0x400, per-window stride 0x80 */
#define CORE_HEAD(h, m)          (0x2000 + (h) * 0x400 + (m))
#define CORE_WIN(w, m)           (0x1000 + (w) * 0x80 + (m))
#define CORE_UPDATE              0x0080

/* Head methods */
#define HEAD_SET_RASTER_SIZE     0x0064
#define HEAD_SET_RASTER_SYNC_END 0x0068
#define HEAD_SET_RASTER_BLANK_END    0x006C
#define HEAD_SET_RASTER_BLANK_START  0x0070
#define HEAD_SET_CONTROL         0x0440

/* Window config methods (on core channel) */
#define WIN_SET_CONTROL          0x0000
#define WIN_SET_FORMAT_BOUNDS    0x0004
#define WIN_SET_ROTATED_BOUNDS   0x0008
#define WIN_SET_USAGE_BOUNDS     0x0010

/* Window channel methods (NVC57E, from clc57e.h) */
#define WIN_UPDATE               0x0200
#define WIN_SET_SIZE             0x0224
#define WIN_SET_STORAGE          0x0228
#define WIN_SET_PARAMS           0x022C
#define WIN_SET_PLANAR_STORAGE   0x0230
#define WIN_SET_CTX_DMA_ISO      0x0240
#define WIN_SET_OFFSET           0x0260
#define WIN_SET_POINT_IN         0x0290
#define WIN_SET_SIZE_IN          0x0298
#define WIN_SET_SIZE_OUT         0x02A4
#define WIN_SET_PRESENT_CONTROL  0x0308

/* WIMM channel methods (NVC37B) — currently unused, kept for future use */
#define WIMM_SET_POINT_OUT       0x0208
#define WIMM_UPDATE              0x0200

/* ---- VRAM layout ---- */
/* GOP address (0xF1000000) maps to VRAM 0 with WC from UEFI.
 * CPU writes go through gop_fb for speed. EVO uses VRAM offsets. */
#define PAGE0_VRAM       0x000000
#define PAGE1_VRAM       0x500000
#define CORE_PB_VRAM     0xA00000
#define WIN_PB_VRAM      0xA01000
#define WIMM_PB_VRAM     0xA02000  /* currently unused */
#define DISP_INST_VRAM   0xA10000
#define DISP_INST_SIZE   0x10000
#define DMA_CTX_OFFSET   0x2000
#define RAMHT_BITS       10

/* ---- State ---- */

static uint8_t  nv_bus, nv_slot, nv_func;
static volatile uint32_t *mmio;
static uint8_t  *fb_bar;
static uint32_t  fb_bar_size;
static uint16_t  device_id;
static int       detected;

static uint8_t  *gop_fb;
static uint32_t  gop_width;
static uint32_t  gop_height;
static uint32_t  gop_pitch;

static volatile uint32_t *core_pb;
static uint32_t  core_pb_put;

static volatile uint32_t *win_pb;
static uint32_t  win_pb_put;

static int       flip_ready;
static int       display_page;

/* ---- MMIO access ---- */

static uint32_t nv_rd32(uint32_t reg) { return mmio[reg / 4]; }
static void nv_wr32(uint32_t reg, uint32_t val) { mmio[reg / 4] = val; }

/* ---- Push buffer helpers ---- */

static void core_push(uint32_t method, uint32_t data) {
    uint32_t off = core_pb_put / 4;
    core_pb[off + 0] = CORE_HDR(1, method);
    core_pb[off + 1] = data;
    core_pb_put += 8;
}

static void core_kick(void) {
    asm volatile ("sfence" ::: "memory");
    nv_wr32(NV_PDISP_CORE_PUT, core_pb_put >> 2);
}

static void core_wait(void) {
    for (int i = 0; i < 200000; i++)
        if ((nv_rd32(NV_PDISP_CORE_STAT) & 0x000F0000) == 0x00040000) break;
}

/* Currently unused — EVO page flipping is dormant (see nvidia_can_flip) */
static void win_push(uint32_t method, uint32_t data) {
    uint32_t off = win_pb_put / 4;
    win_pb[off + 0] = WIN_HDR(1, method);
    win_pb[off + 1] = data;
    win_pb_put += 8;
}

/* Currently unused — EVO page flipping is dormant */
static void win_push_multi(uint32_t base_method, const uint32_t *data, int count) {
    uint32_t off = win_pb_put / 4;
    win_pb[off] = WIN_HDR(count, base_method);
    for (int i = 0; i < count; i++)
        win_pb[off + 1 + i] = data[i];
    win_pb_put += 4 + count * 4;
}

/* Currently unused — EVO page flipping is dormant */
static void win_kick(void) {
    asm volatile ("sfence" ::: "memory");
    nv_wr32(NV_PDISP_WIN_PUT(0), win_pb_put >> 2);
}

/* Currently unused — EVO page flipping is dormant */
static void win_wait(void) {
    for (int i = 0; i < 200000; i++)
        if ((nv_rd32(NV_PDISP_WIN_STAT(0)) & 0x000F0000) == 0x00040000) break;
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
    uint32_t orig = pci_read(bus, slot, func, offset);
    pci_write(bus, slot, func, offset, 0xFFFFFFFF);
    uint32_t mask = pci_read(bus, slot, func, offset);
    pci_write(bus, slot, func, offset, orig);
    return (~(mask & 0xFFFFFFF0)) + 1;
}

static uint32_t ramht_hash(int chid, uint32_t handle) {
    uint32_t hash = 0;
    while (handle) {
        hash ^= (handle & ((1 << RAMHT_BITS) - 1));
        handle >>= RAMHT_BITS;
    }
    hash ^= chid << (RAMHT_BITS - 4);
    return hash & ((1 << RAMHT_BITS) - 1);
}

/* ---- Display engine init (from nouveau tu102_disp_init) ---- */

static void disp_engine_init(void) {
    uint32_t tmp;
    int i, j;

    uint32_t pmc = nv_rd32(NV_PMC_ENABLE);
    if (!(pmc & 0x40000000))
        nv_wr32(NV_PMC_ENABLE, pmc | 0x40000000);

    /* Claim ownership */
    if (nv_rd32(NV_PDISP_OWNERSHIP) & 0x00000002) {
        nv_wr32(NV_PDISP_OWNERSHIP, nv_rd32(NV_PDISP_OWNERSHIP) & ~1);
        for (i = 0; i < 200000; i++)
            if (!(nv_rd32(NV_PDISP_OWNERSHIP) & 0x00000002)) break;
    }

    /* Capabilities (TU102 offsets) */
    nv_wr32(0x640008, 0x00000021);

    for (i = 0; i < 4; i++) {
        tmp = nv_rd32(0x61C000 + i * 0x800);
        if (tmp == 0 || (tmp & 0xFFFF0000) == 0xBADF0000) continue;
        nv_wr32(0x640000, nv_rd32(0x640000) | (0x100 << i));
        nv_wr32(0x640144 + i * 8, tmp);
    }

    for (i = 0; i < 2; i++) {
        tmp = nv_rd32(0x616300 + i * 0x800);
        nv_wr32(0x640048 + i * 0x20, tmp);
        for (j = 0; j < 5 * 4; j += 4) {
            tmp = nv_rd32(0x616140 + i * 0x800 + j);
            nv_wr32(0x640680 + i * 0x20 + j, tmp);
        }
    }

    for (i = 0; i < 8; i++) {
        nv_wr32(0x640004, nv_rd32(0x640004) | (1 << i));
        for (j = 0; j < 6 * 4; j += 4) {
            tmp = nv_rd32(0x630100 + i * 0x800 + j);
            nv_wr32(0x640780 + i * 0x20 + j, tmp);
        }
        nv_wr32(0x64000C, nv_rd32(0x64000C) | 0x100);
    }

    for (i = 0; i < 3; i++) {
        tmp = nv_rd32(0x62E000 + i * 4);
        nv_wr32(0x640010 + i * 4, tmp);
    }

    /* Master control */
    nv_wr32(NV_PDISP_MASTER_CTRL, nv_rd32(NV_PDISP_MASTER_CTRL) | 1);

    /* Instance memory with RAMHT and DMA context */
    volatile uint32_t *inst = (volatile uint32_t *)(fb_bar + DISP_INST_VRAM);
    memset((void *)inst, 0, DISP_INST_SIZE);

    /* DMA context object: VRAM, RDWR, full range */
    volatile uint32_t *dma = (volatile uint32_t *)(fb_bar + DISP_INST_VRAM + DMA_CTX_OFFSET);
    dma[0] = 0x000D003D;
    dma[1] = 0xFFFFFFFF;
    dma[2] = 0x00000000;
    dma[3] = 0x00000000;

    /* RAMHT entry: handle=1, chid=1, context=offset<<9 */
    uint32_t slot = ramht_hash(1, 1);
    volatile uint32_t *entry = (volatile uint32_t *)((uint8_t *)inst + slot * 8);
    entry[0] = 0x00000001;
    entry[1] = DMA_CTX_OFFSET << 9;

    nv_wr32(NV_PDISP_INST_TARGET, 0x00000009);
    nv_wr32(NV_PDISP_INST_ADDR, DISP_INST_VRAM >> 16);

    /* Interrupts */
    nv_wr32(0x611CF0, 0x00000187);
    nv_wr32(0x611DB0, 0x00000187);
    nv_wr32(0x611CEC, 0x00030001);
    nv_wr32(0x611DAC, 0x00000000);
    nv_wr32(0x611CE8, 0x000000FF);
    nv_wr32(0x611DA8, 0x00000000);
    nv_wr32(0x611CE4, 0x000000FF);
    nv_wr32(0x611DA4, 0x00000000);
    for (i = 0; i < 2; i++) {
        nv_wr32(0x611CC0 + i * 4, 0x00000004);
        nv_wr32(0x611D80 + i * 4, 0x00000000);
    }
    nv_wr32(0x611CF4, 0x00000000);
    nv_wr32(0x611DB4, 0x00000000);
}

/* ---- Channel init helpers ---- */

static int channel_init(int ctrl, uint32_t pb_vram, uint32_t put_reg, uint32_t stat_reg) {
    /* Disable channel first (clean slate) */
    nv_wr32(NV_PDISP_CHAN_CTRL(ctrl), 0x00000000);
    for (int i = 0; i < 100000; i++)
        if ((nv_rd32(stat_reg) & 0x000F0000) == 0) break;

    /* Push buffer setup */
    nv_wr32(NV_PDISP_PB_HI(ctrl), 0x00000000);
    nv_wr32(NV_PDISP_PB_LO(ctrl), PB_ENCODE(pb_vram));
    nv_wr32(NV_PDISP_PB_VALID(ctrl), 0x00000001);
    nv_wr32(NV_PDISP_PB_LIMIT(ctrl), 0x00000040);

    /* Idle request, reset PUT, enable */
    nv_wr32(NV_PDISP_CHAN_CTRL(ctrl), 0x00000010);
    for (int i = 0; i < 100000; i++)
        if ((nv_rd32(stat_reg) & 0x000F0000) == 0x00040000) break;

    nv_wr32(put_reg, 0x00000000);
    nv_wr32(NV_PDISP_CHAN_CTRL(ctrl), 0x00000013);

    for (int i = 0; i < 200000; i++) {
        uint32_t state = (nv_rd32(stat_reg) >> 16) & 0xF;
        if (state == 0x4 || state == 0xB) return 1;
    }
    return 0;
}

/* ---- Mode setting ---- */

static void mode_set(void) {
    /* 1280x1024@60Hz VESA timing */
    core_push(CORE_HEAD(0, HEAD_SET_RASTER_SIZE),        (1066 << 16) | 1688);
    core_push(CORE_HEAD(0, HEAD_SET_RASTER_SYNC_END),    (1028 << 16) | 1440);
    core_push(CORE_HEAD(0, HEAD_SET_RASTER_BLANK_END),   (39 << 16) | 48);
    core_push(CORE_HEAD(0, HEAD_SET_RASTER_BLANK_START), (1024 << 16) | 1280);
    core_push(CORE_HEAD(0, HEAD_SET_CONTROL), 0x00000001);
    core_push(CORE_UPDATE, 0x00000001);
    core_kick();
    core_wait();
}

/* ---- Window bounds (on core channel) ---- */

static void window_bounds(void) {
    core_push(CORE_WIN(0, WIN_SET_CONTROL), 0x00000000);
    core_push(CORE_WIN(0, WIN_SET_FORMAT_BOUNDS),
              (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3));
    core_push(CORE_WIN(0, WIN_SET_ROTATED_BOUNDS), 0x00000000);
    core_push(CORE_WIN(0, WIN_SET_USAGE_BOUNDS),
              0x00007FFF | (2 << 20));
    core_push(CORE_UPDATE, 0x00000001);
    core_kick();
    core_wait();
}

/* ---- Surface configuration — currently unused, EVO page flipping is dormant ---- */

static int surface_set(uint32_t vram_offset) {
    uint32_t w = gop_width;
    uint32_t h = gop_height;
    uint32_t pitch = gop_pitch;

    uint32_t geom[4] = {
        (h << 16) | w,       /* SET_SIZE */
        (1 << 4),            /* SET_STORAGE: PITCH */
        0x000000CF,          /* SET_PARAMS: A8R8G8B8 */
        pitch >> 6,          /* SET_PLANAR_STORAGE */
    };

    uint32_t dma[2] = { 0x00000001, 0x00000001 };

    win_push(WIN_SET_PRESENT_CONTROL, 0x00000001);
    win_push_multi(WIN_SET_SIZE, geom, 4);
    win_push_multi(WIN_SET_CTX_DMA_ISO, dma, 2);
    win_push(WIN_SET_OFFSET, vram_offset >> 8);
    win_push(WIN_SET_POINT_IN, 0x00000000);
    win_push(WIN_SET_SIZE_IN, (h << 16) | w);
    win_push(WIN_SET_SIZE_OUT, (h << 16) | w);
    win_push(WIN_UPDATE, 0x00000001);
    win_kick();
    win_wait();

    return ((nv_rd32(NV_PDISP_WIN_STAT(0)) >> 16) & 0xF) == 4;
}

/* ---- Page flip — currently unused, EVO page flipping is dormant ---- */

static void nvidia_flip(int page) {
    uint32_t offset = page ? PAGE1_VRAM : PAGE0_VRAM;
    win_push(WIN_SET_OFFSET, offset >> 8);
    win_push(WIN_UPDATE, 0x00000001);
    win_kick();
    win_wait();
    display_page = page;
}

/* ---- Detection ---- */

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
    terminal_printf(" NVIDIA: BAR1=0x%x (%dMB)\n",
                    (uint32_t)(uintptr_t)fb_bar,
                    fb_bar_size / (1024 * 1024));
    return 1;
}

/* ---- Init ---- */

void nvidia_init(uint32_t width, uint32_t height) {
    if (!detected) return;

    /* Capture GOP framebuffer info */
    uint8_t *addr;
    uint32_t w, h, p, b;
    terminal_get_fb(&addr, &w, &h, &p, &b);
    if (addr && w > 0) {
        gop_fb = addr;
        gop_width = w;
        gop_height = h;
        gop_pitch = p;
    }

    /* Display engine init */
    disp_engine_init();

    /* Core channel */
    core_pb = (volatile uint32_t *)(fb_bar + CORE_PB_VRAM);
    core_pb_put = 0;
    memset((void *)core_pb, 0, PB_SIZE);

    if (!channel_init(0, CORE_PB_VRAM, NV_PDISP_CORE_PUT, NV_PDISP_CORE_STAT)) {
        terminal_print(" NVIDIA: core channel failed\n");
        return;
    }

    mode_set();
    window_bounds();

    /* Window channel */
    win_pb = (volatile uint32_t *)(fb_bar + WIN_PB_VRAM);
    win_pb_put = 0;
    memset((void *)win_pb, 0, PB_SIZE);

    if (!channel_init(1, WIN_PB_VRAM, NV_PDISP_WIN_PUT(0), 0x610664)) {
        terminal_print(" NVIDIA: window channel failed\n");
        return;
    }

    /* Clear pages via GOP address (WC fast path) */
    uint32_t page_size = gop_pitch * gop_height;
    memset(gop_fb, 0, page_size);
    memset(gop_fb + PAGE1_VRAM, 0, page_size);

    if (surface_set(PAGE0_VRAM)) {
        flip_ready = 1;
        display_page = 0;
        terminal_print(" NVIDIA: page flip ready\n");
    } else {
        terminal_print(" NVIDIA: surface config failed\n");
    }
}

void nvidia_save_dump(void) { }

/* ---- gpu_driver interface ---- */

static uint8_t *nvidia_framebuffer(void) {
    /* GOP address maps to VRAM 0 with WC from UEFI */
    return gop_fb;
}

static uint32_t nvidia_width(void) { return gop_width; }
static uint32_t nvidia_height(void) { return gop_height; }
static uint32_t nvidia_pitch(void) { return gop_pitch; }
/* Page flipping disabled — dirty-region memcpy through gop_fb (WC) is faster
 * than full-frame memcpy through BAR1 (UC) + EVO flip. The MTRR default UC
 * on BAR1 prevents WC for the page-flip path. gop_fb has WC from UEFI. */
static int nvidia_can_flip(void) { return 0; }

static uint8_t *nvidia_page_addr(int page) {
    if (!flip_ready) return gop_fb;
    /* CPU writes through gop_fb (WC), EVO scans from VRAM offsets */
    return gop_fb + (page ? PAGE1_VRAM : 0);
}

static void nvidia_set_page(int page) {
    if (flip_ready)
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
