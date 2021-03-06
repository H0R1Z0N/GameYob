#include <dirent.h>

#ifdef WIN32
#include <io.h>
#define access _access
#else
#include <unistd.h>
#endif

#include <algorithm>
#include <chrono>
#include <fstream>
#include <sstream>
#include <iomanip>

#include "libs/inih/INIReader.h"
#include "libs/stb_image/stb_image.h"

#include "platform/common/menu/filechooser.h"
#include "platform/common/menu/mainmenu.h"
#include "platform/common/menu/menu.h"
#include "platform/common/config.h"
#include "platform/common/manager.h"
#include "platform/audio.h"
#include "platform/gfx.h"
#include "platform/input.h"
#include "platform/system.h"
#include "platform/ui.h"
#include "cartridge.h"
#include "gameboy.h"
#include "mmu.h"
#include "ppu.h"
#include "sgb.h"

#define RGBA32(r, g, b) ((u32) ((r) << 24 | (g) << 16 | (b) << 8 | 0xFF))

static const u32 p005[] = {
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0x52, 0xFF, 0x00), RGBA32(0xFF, 0x42, 0x00), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0x52, 0xFF, 0x00), RGBA32(0xFF, 0x42, 0x00), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0x52, 0xFF, 0x00), RGBA32(0xFF, 0x42, 0x00), RGBA32(0x00, 0x00, 0x00)
};

static const u32 p006[] = {
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xFF, 0x9C, 0x00), RGBA32(0xFF, 0x00, 0x00), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xFF, 0x9C, 0x00), RGBA32(0xFF, 0x00, 0x00), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xFF, 0x9C, 0x00), RGBA32(0xFF, 0x00, 0x00), RGBA32(0x00, 0x00, 0x00)
};

static const u32 p007[] = {
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xFF, 0xFF, 0x00), RGBA32(0xFF, 0x00, 0x00), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xFF, 0xFF, 0x00), RGBA32(0xFF, 0x00, 0x00), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xFF, 0xFF, 0x00), RGBA32(0xFF, 0x00, 0x00), RGBA32(0x00, 0x00, 0x00)
};

static const u32 p008[] = {
        RGBA32(0xA5, 0x9C, 0xFF), RGBA32(0xFF, 0xFF, 0x00), RGBA32(0x00, 0x63, 0x00), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xA5, 0x9C, 0xFF), RGBA32(0xFF, 0xFF, 0x00), RGBA32(0x00, 0x63, 0x00), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xA5, 0x9C, 0xFF), RGBA32(0xFF, 0xFF, 0x00), RGBA32(0x00, 0x63, 0x00), RGBA32(0x00, 0x00, 0x00)
};

static const u32 p012[] = {
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xFF, 0xAD, 0x63), RGBA32(0x84, 0x31, 0x00), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xFF, 0xAD, 0x63), RGBA32(0x84, 0x31, 0x00), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xFF, 0xAD, 0x63), RGBA32(0x84, 0x31, 0x00), RGBA32(0x00, 0x00, 0x00)
};

static const u32 p013[] = {
        RGBA32(0x00, 0x00, 0x00), RGBA32(0x00, 0x84, 0x84), RGBA32(0xFF, 0xDE, 0x00), RGBA32(0xFF, 0xFF, 0xFF),
        RGBA32(0x00, 0x00, 0x00), RGBA32(0x00, 0x84, 0x84), RGBA32(0xFF, 0xDE, 0x00), RGBA32(0xFF, 0xFF, 0xFF),
        RGBA32(0x00, 0x00, 0x00), RGBA32(0x00, 0x84, 0x84), RGBA32(0xFF, 0xDE, 0x00), RGBA32(0xFF, 0xFF, 0xFF)
};

static const u32 p016[] = {
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xA5, 0xA5, 0xA5), RGBA32(0x52, 0x52, 0x52), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xA5, 0xA5, 0xA5), RGBA32(0x52, 0x52, 0x52), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xA5, 0xA5, 0xA5), RGBA32(0x52, 0x52, 0x52), RGBA32(0x00, 0x00, 0x00)
};

static const u32 p017[] = {
        RGBA32(0xFF, 0xFF, 0xA5), RGBA32(0xFF, 0x94, 0x94), RGBA32(0x94, 0x94, 0xFF), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xA5), RGBA32(0xFF, 0x94, 0x94), RGBA32(0x94, 0x94, 0xFF), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xA5), RGBA32(0xFF, 0x94, 0x94), RGBA32(0x94, 0x94, 0xFF), RGBA32(0x00, 0x00, 0x00)
};

static const u32 p100[] = {
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xAD, 0xAD, 0x84), RGBA32(0x42, 0x73, 0x7B), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xFF, 0x73, 0x00), RGBA32(0x94, 0x42, 0x00), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xAD, 0xAD, 0x84), RGBA32(0x42, 0x73, 0x7B), RGBA32(0x00, 0x00, 0x00)
};

static const u32 p10B[] = {
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0x63, 0xA5, 0xFF), RGBA32(0x00, 0x00, 0xFF), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xFF, 0x84, 0x84), RGBA32(0x94, 0x3A, 0x3A), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0x63, 0xA5, 0xFF), RGBA32(0x00, 0x00, 0xFF), RGBA32(0x00, 0x00, 0x00)
};

static const u32 p10D[] = {
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0x8C, 0x8C, 0xDE), RGBA32(0x52, 0x52, 0x8C), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xFF, 0x84, 0x84), RGBA32(0x94, 0x3A, 0x3A), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0x8C, 0x8C, 0xDE), RGBA32(0x52, 0x52, 0x8C), RGBA32(0x00, 0x00, 0x00)
};

static const u32 p110[] = {
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xFF, 0x84, 0x84), RGBA32(0x94, 0x3A, 0x3A), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0x7B, 0xFF, 0x31), RGBA32(0x00, 0x84, 0x00), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xFF, 0x84, 0x84), RGBA32(0x94, 0x3A, 0x3A), RGBA32(0x00, 0x00, 0x00)
};

static const u32 p11C[] = {
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0x7B, 0xFF, 0x31), RGBA32(0x00, 0x63, 0xC5), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xFF, 0x84, 0x84), RGBA32(0x94, 0x3A, 0x3A), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0x7B, 0xFF, 0x31), RGBA32(0x00, 0x63, 0xC5), RGBA32(0x00, 0x00, 0x00)
};

static const u32 p20B[] = {
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0x63, 0xA5, 0xFF), RGBA32(0x00, 0x00, 0xFF), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0x63, 0xA5, 0xFF), RGBA32(0x00, 0x00, 0xFF), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xFF, 0x84, 0x84), RGBA32(0x94, 0x3A, 0x3A), RGBA32(0x00, 0x00, 0x00)
};

static const u32 p20C[] = {
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0x8C, 0x8C, 0xDE), RGBA32(0x52, 0x52, 0x8C), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0x8C, 0x8C, 0xDE), RGBA32(0x52, 0x52, 0x8C), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xC5, 0x42), RGBA32(0xFF, 0xD6, 0x00), RGBA32(0x94, 0x3A, 0x00), RGBA32(0x4A, 0x00, 0x00)
};

static const u32 p300[] = {
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xAD, 0xAD, 0x84), RGBA32(0x42, 0x73, 0x7B), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xFF, 0x73, 0x00), RGBA32(0x94, 0x42, 0x00), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xFF, 0x73, 0x00), RGBA32(0x94, 0x42, 0x00), RGBA32(0x00, 0x00, 0x00)
};

static const u32 p304[] = {
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0x7B, 0xFF, 0x00), RGBA32(0xB5, 0x73, 0x00), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xFF, 0x84, 0x84), RGBA32(0x94, 0x3A, 0x3A), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xFF, 0x84, 0x84), RGBA32(0x94, 0x3A, 0x3A), RGBA32(0x00, 0x00, 0x00)
};

static const u32 p305[] = {
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0x52, 0xFF, 0x00), RGBA32(0xFF, 0x42, 0x00), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xFF, 0x84, 0x84), RGBA32(0x94, 0x3A, 0x3A), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xFF, 0x84, 0x84), RGBA32(0x94, 0x3A, 0x3A), RGBA32(0x00, 0x00, 0x00)
};

static const u32 p306[] = {
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xFF, 0x9C, 0x00), RGBA32(0xFF, 0x00, 0x00), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xFF, 0x84, 0x84), RGBA32(0x94, 0x3A, 0x3A), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xFF, 0x84, 0x84), RGBA32(0x94, 0x3A, 0x3A), RGBA32(0x00, 0x00, 0x00)
};

static const u32 p308[] = {
        RGBA32(0xA5, 0x9C, 0xFF), RGBA32(0xFF, 0xFF, 0x00), RGBA32(0x00, 0x63, 0x00), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0x63, 0x52), RGBA32(0xD6, 0x00, 0x00), RGBA32(0x63, 0x00, 0x00), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0x63, 0x52), RGBA32(0xD6, 0x00, 0x00), RGBA32(0x63, 0x00, 0x00), RGBA32(0x00, 0x00, 0x00)
};

static const u32 p30A[] = {
        RGBA32(0xB5, 0xB5, 0xFF), RGBA32(0xFF, 0xFF, 0x94), RGBA32(0xAD, 0x5A, 0x42), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0x00, 0x00, 0x00), RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xFF, 0x84, 0x84), RGBA32(0x94, 0x3A, 0x3A),
        RGBA32(0x00, 0x00, 0x00), RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xFF, 0x84, 0x84), RGBA32(0x94, 0x3A, 0x3A)
};

static const u32 p30C[] = {
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0x8C, 0x8C, 0xDE), RGBA32(0x52, 0x52, 0x8C), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xC5, 0x42), RGBA32(0xFF, 0xD6, 0x00), RGBA32(0x94, 0x3A, 0x00), RGBA32(0x4A, 0x00, 0x00),
        RGBA32(0xFF, 0xC5, 0x42), RGBA32(0xFF, 0xD6, 0x00), RGBA32(0x94, 0x3A, 0x00), RGBA32(0x4A, 0x00, 0x00)
};

static const u32 p30D[] = {
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0x8C, 0x8C, 0xDE), RGBA32(0x52, 0x52, 0x8C), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xFF, 0x84, 0x84), RGBA32(0x94, 0x3A, 0x3A), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xFF, 0x84, 0x84), RGBA32(0x94, 0x3A, 0x3A), RGBA32(0x00, 0x00, 0x00)
};

static const u32 p30E[] = {
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0x7B, 0xFF, 0x31), RGBA32(0x00, 0x84, 0x00), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xFF, 0x84, 0x84), RGBA32(0x94, 0x3A, 0x3A), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xFF, 0x84, 0x84), RGBA32(0x94, 0x3A, 0x3A), RGBA32(0x00, 0x00, 0x00)
};

static const u32 p30F[] = {
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xFF, 0xAD, 0x63), RGBA32(0x84, 0x31, 0x00), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0x63, 0xA5, 0xFF), RGBA32(0x00, 0x00, 0xFF), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0x63, 0xA5, 0xFF), RGBA32(0x00, 0x00, 0xFF), RGBA32(0x00, 0x00, 0x00)
};

static const u32 p312[] = {
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xFF, 0xAD, 0x63), RGBA32(0x84, 0x31, 0x00), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0x7B, 0xFF, 0x31), RGBA32(0x00, 0x84, 0x00), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0x7B, 0xFF, 0x31), RGBA32(0x00, 0x84, 0x00), RGBA32(0x00, 0x00, 0x00)
};

static const u32 p319[] = {
        RGBA32(0xFF, 0xE6, 0xC5), RGBA32(0xCE, 0x9C, 0x84), RGBA32(0x84, 0x6B, 0x29), RGBA32(0x5A, 0x31, 0x08),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xFF, 0xAD, 0x63), RGBA32(0x84, 0x31, 0x00), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xFF, 0xAD, 0x63), RGBA32(0x84, 0x31, 0x00), RGBA32(0x00, 0x00, 0x00)
};

static const u32 p31C[] = {
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0x7B, 0xFF, 0x31), RGBA32(0x00, 0x63, 0xC5), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xFF, 0x84, 0x84), RGBA32(0x94, 0x3A, 0x3A), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xFF, 0x84, 0x84), RGBA32(0x94, 0x3A, 0x3A), RGBA32(0x00, 0x00, 0x00)
};

static const u32 p405[] = {
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0x52, 0xFF, 0x00), RGBA32(0xFF, 0x42, 0x00), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0x52, 0xFF, 0x00), RGBA32(0xFF, 0x42, 0x00), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0x5A, 0xBD, 0xFF), RGBA32(0xFF, 0x00, 0x00), RGBA32(0x00, 0x00, 0xFF)
};

static const u32 p406[] = {
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xFF, 0x9C, 0x00), RGBA32(0xFF, 0x00, 0x00), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xFF, 0x9C, 0x00), RGBA32(0xFF, 0x00, 0x00), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0x5A, 0xBD, 0xFF), RGBA32(0xFF, 0x00, 0x00), RGBA32(0x00, 0x00, 0xFF)
};

static const u32 p407[] = {
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xFF, 0xFF, 0x00), RGBA32(0xFF, 0x00, 0x00), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xFF, 0xFF, 0x00), RGBA32(0xFF, 0x00, 0x00), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0x5A, 0xBD, 0xFF), RGBA32(0xFF, 0x00, 0x00), RGBA32(0x00, 0x00, 0xFF)
};

static const u32 p500[] = {
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xAD, 0xAD, 0x84), RGBA32(0x42, 0x73, 0x7B), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xFF, 0x73, 0x00), RGBA32(0x94, 0x42, 0x00), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0x5A, 0xBD, 0xFF), RGBA32(0xFF, 0x00, 0x00), RGBA32(0x00, 0x00, 0xFF)
};

static const u32 p501[] = {
        RGBA32(0xFF, 0xFF, 0x9C), RGBA32(0x94, 0xB5, 0xFF), RGBA32(0x63, 0x94, 0x73), RGBA32(0x00, 0x3A, 0x3A),
        RGBA32(0xFF, 0xC5, 0x42), RGBA32(0xFF, 0xD6, 0x00), RGBA32(0x94, 0x3A, 0x00), RGBA32(0x4A, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xFF, 0x84, 0x84), RGBA32(0x94, 0x3A, 0x3A), RGBA32(0x00, 0x00, 0x00)
};

static const u32 p502[] = {
        RGBA32(0x6B, 0xFF, 0x00), RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xFF, 0x52, 0x4A), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0x63, 0xA5, 0xFF), RGBA32(0x00, 0x00, 0xFF),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xFF, 0xAD, 0x63), RGBA32(0x84, 0x31, 0x00), RGBA32(0x00, 0x00, 0x00)
};

static const u32 p503[] = {
        RGBA32(0x52, 0xDE, 0x00), RGBA32(0xFF, 0x84, 0x00), RGBA32(0xFF, 0xFF, 0x00), RGBA32(0xFF, 0xFF, 0xFF),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0x63, 0xA5, 0xFF), RGBA32(0x00, 0x00, 0xFF),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xFF, 0x84, 0x84), RGBA32(0x94, 0x3A, 0x3A), RGBA32(0x00, 0x00, 0x00)
};

static const u32 p508[] = {
        RGBA32(0xA5, 0x9C, 0xFF), RGBA32(0xFF, 0xFF, 0x00), RGBA32(0x00, 0x63, 0x00), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0x63, 0x52), RGBA32(0xD6, 0x00, 0x00), RGBA32(0x63, 0x00, 0x00), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0x00, 0x00, 0xFF), RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xFF, 0xFF, 0x7B), RGBA32(0x00, 0x84, 0xFF)
};

static const u32 p509[] = {
        RGBA32(0xFF, 0xFF, 0xCE), RGBA32(0x63, 0xEF, 0xEF), RGBA32(0x9C, 0x84, 0x31), RGBA32(0x5A, 0x5A, 0x5A),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xFF, 0x73, 0x00), RGBA32(0x94, 0x42, 0x00), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0x63, 0xA5, 0xFF), RGBA32(0x00, 0x00, 0xFF), RGBA32(0x00, 0x00, 0x00)
};

static const u32 p50B[] = {
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0x63, 0xA5, 0xFF), RGBA32(0x00, 0x00, 0xFF), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xFF, 0x84, 0x84), RGBA32(0x94, 0x3A, 0x3A), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xFF, 0xFF, 0x7B), RGBA32(0x00, 0x84, 0xFF), RGBA32(0xFF, 0x00, 0x00)
};

static const u32 p50C[] = {
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0x8C, 0x8C, 0xDE), RGBA32(0x52, 0x52, 0x8C), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xC5, 0x42), RGBA32(0xFF, 0xD6, 0x00), RGBA32(0x94, 0x3A, 0x00), RGBA32(0x4A, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0x5A, 0xBD, 0xFF), RGBA32(0xFF, 0x00, 0x00), RGBA32(0x00, 0x00, 0xFF)
};

static const u32 p50D[] = {
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0x8C, 0x8C, 0xDE), RGBA32(0x52, 0x52, 0x8C), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xFF, 0x84, 0x84), RGBA32(0x94, 0x3A, 0x3A), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xFF, 0xAD, 0x63), RGBA32(0x84, 0x31, 0x00), RGBA32(0x00, 0x00, 0x00)
};

static const u32 p50E[] = {
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0x7B, 0xFF, 0x31), RGBA32(0x00, 0x84, 0x00), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xFF, 0x84, 0x84), RGBA32(0x94, 0x3A, 0x3A), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0x63, 0xA5, 0xFF), RGBA32(0x00, 0x00, 0xFF), RGBA32(0x00, 0x00, 0x00)
};

static const u32 p50F[] = {
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xFF, 0xAD, 0x63), RGBA32(0x84, 0x31, 0x00), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0x63, 0xA5, 0xFF), RGBA32(0x00, 0x00, 0xFF), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0x7B, 0xFF, 0x31), RGBA32(0x00, 0x84, 0x00), RGBA32(0x00, 0x00, 0x00)
};

static const u32 p510[] = {
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xFF, 0x84, 0x84), RGBA32(0x94, 0x3A, 0x3A), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0x7B, 0xFF, 0x31), RGBA32(0x00, 0x84, 0x00), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0x63, 0xA5, 0xFF), RGBA32(0x00, 0x00, 0xFF), RGBA32(0x00, 0x00, 0x00)
};

static const u32 p511[] = {
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xFF, 0x84, 0x84), RGBA32(0x94, 0x3A, 0x3A), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0x00, 0xFF, 0x00), RGBA32(0x31, 0x84, 0x00), RGBA32(0x00, 0x4A, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0x63, 0xA5, 0xFF), RGBA32(0x00, 0x00, 0xFF), RGBA32(0x00, 0x00, 0x00)
};

static const u32 p512[] = {
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xFF, 0xAD, 0x63), RGBA32(0x84, 0x31, 0x00), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0x7B, 0xFF, 0x31), RGBA32(0x00, 0x84, 0x00), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0x63, 0xA5, 0xFF), RGBA32(0x00, 0x00, 0xFF), RGBA32(0x00, 0x00, 0x00)
};

static const u32 p514[] = {
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0x63, 0xA5, 0xFF), RGBA32(0x00, 0x00, 0xFF), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0x00), RGBA32(0xFF, 0x00, 0x00), RGBA32(0x63, 0x00, 0x00), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0x7B, 0xFF, 0x31), RGBA32(0x00, 0x84, 0x00), RGBA32(0x00, 0x00, 0x00)
};

static const u32 p515[] = {
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xAD, 0xAD, 0x84), RGBA32(0x42, 0x73, 0x7B), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xFF, 0xAD, 0x63), RGBA32(0x84, 0x31, 0x00), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0x63, 0xA5, 0xFF), RGBA32(0x00, 0x00, 0xFF), RGBA32(0x00, 0x00, 0x00)
};

static const u32 p518[] = {
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0x63, 0xA5, 0xFF), RGBA32(0x00, 0x00, 0xFF), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xFF, 0x84, 0x84), RGBA32(0x94, 0x3A, 0x3A), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0x7B, 0xFF, 0x31), RGBA32(0x00, 0x84, 0x00), RGBA32(0x00, 0x00, 0x00)
};

static const u32 p51A[] = {
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xFF, 0xFF, 0x00), RGBA32(0x7B, 0x4A, 0x00), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0x63, 0xA5, 0xFF), RGBA32(0x00, 0x00, 0xFF), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0x7B, 0xFF, 0x31), RGBA32(0x00, 0x84, 0x00), RGBA32(0x00, 0x00, 0x00)
};

static const u32 p51C[] = {
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0x7B, 0xFF, 0x31), RGBA32(0x00, 0x63, 0xC5), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0xFF, 0x84, 0x84), RGBA32(0x94, 0x3A, 0x3A), RGBA32(0x00, 0x00, 0x00),
        RGBA32(0xFF, 0xFF, 0xFF), RGBA32(0x63, 0xA5, 0xFF), RGBA32(0x00, 0x00, 0xFF), RGBA32(0x00, 0x00, 0x00)
};

static const u32 pCls[] = {
        RGBA32(0x9B, 0xBC, 0x0F), RGBA32(0x8B, 0xAC, 0x0F), RGBA32(0x30, 0x62, 0x30), RGBA32(0x0F, 0x38, 0x0F),
        RGBA32(0x9B, 0xBC, 0x0F), RGBA32(0x8B, 0xAC, 0x0F), RGBA32(0x30, 0x62, 0x30), RGBA32(0x0F, 0x38, 0x0F),
        RGBA32(0x9B, 0xBC, 0x0F), RGBA32(0x8B, 0xAC, 0x0F), RGBA32(0x30, 0x62, 0x30), RGBA32(0x0F, 0x38, 0x0F)
};

struct GbcPaletteEntry {
    const std::string title;
    const u32* p;
};

static const GbcPaletteEntry gbcPalettes[] = {
        {"GB - Classic",     pCls},
        {"GBC - Blue",       p518},
        {"GBC - Brown",      p012},
        {"GBC - Dark Blue",  p50D},
        {"GBC - Dark Brown", p319},
        {"GBC - Dark Green", p31C},
        {"GBC - Grayscale",  p016},
        {"GBC - Green",      p005},
        {"GBC - Inverted",   p013},
        {"GBC - Orange",     p007},
        {"GBC - Pastel Mix", p017},
        {"GBC - Red",        p510},
        {"GBC - Yellow",     p51A},
        {"ALLEY WAY",        p008},
        {"ASTEROIDS/MISCMD", p30E},
        {"ATOMIC PUNK",      p30F}, // unofficial ("DYNABLASTER" alt.)
        {"BA.TOSHINDEN",     p50F},
        {"BALLOON KID",      p006},
        {"BASEBALL",         p503},
        {"BOMBERMAN GB",     p31C}, // unofficial ("WARIO BLAST" alt.)
        {"BOY AND BLOB GB1", p512},
        {"BOY AND BLOB GB2", p512},
        {"BT2RAGNAROKWORLD", p312},
        {"DEFENDER/JOUST",   p50F},
        {"DMG FOOTBALL",     p30E},
        {"DONKEY KONG",      p306},
        {"DONKEYKONGLAND",   p50C},
        {"DONKEYKONGLAND 2", p50C},
        {"DONKEYKONGLAND 3", p50C},
        {"DONKEYKONGLAND95", p501},
        {"DR.MARIO",         p20B},
        {"DYNABLASTER",      p30F},
        {"F1RACE",           p012},
        {"FOOTBALL INT'L",   p502}, // unofficial ("SOCCER" alt.)
        {"G&W GALLERY",      p304},
        {"GALAGA&GALAXIAN",  p013},
        {"GAME&WATCH",       p012},
        {"GAMEBOY GALLERY",  p304},
        {"GAMEBOY GALLERY2", p304},
        {"GBWARS",           p500},
        {"GBWARST",          p500}, // unofficial ("GBWARS" alt.)
        {"GOLF",             p30E},
        {"Game and Watch 2", p304},
        {"HOSHINOKA-BI",     p508},
        {"JAMES  BOND  007", p11C},
        {"KAERUNOTAMENI",    p10D},
        {"KEN GRIFFEY JR",   p31C},
        {"KID ICARUS",       p30D},
        {"KILLERINSTINCT95", p50D},
        {"KINGOFTHEZOO",     p30F},
        {"KIRAKIRA KIDS",    p012},
        {"KIRBY BLOCKBALL",  p508},
        {"KIRBY DREAM LAND", p508},
        {"KIRBY'S PINBALL",  p308},
        {"KIRBY2",           p508},
        {"LOLO2",            p50F},
        {"MAGNETIC SOCCER",  p50E},
        {"MANSELL",          p012},
        {"MARIO & YOSHI",    p305},
        {"MARIO'S PICROSS",  p012},
        {"MARIOLAND2",       p509},
        {"MEGA MAN 2",       p50F},
        {"MEGAMAN",          p50F},
        {"MEGAMAN3",         p50F},
        {"METROID2",         p514},
        {"MILLI/CENTI/PEDE", p31C},
        {"MOGURANYA",        p300},
        {"MYSTIC QUEST",     p50E},
        {"NETTOU KOF 95",    p50F},
        {"NEW CHESSMASTER",  p30F},
        {"OTHELLO",          p50E},
        {"PAC-IN-TIME",      p51C},
        {"PENGUIN WARS",     p30F}, // unofficial ("KINGOFTHEZOO" alt.)
        {"PENGUINKUNWARSVS", p30F}, // unofficial ("KINGOFTHEZOO" alt.)
        {"PICROSS 2",        p012},
        {"PINOCCHIO",        p20C},
        {"POKEBOM",          p30C},
        {"POKEMON BLUE",     p10B},
        {"POKEMON GREEN",    p11C},
        {"POKEMON RED",      p110},
        {"POKEMON YELLOW",   p007},
        {"QIX",              p407},
        {"RADARMISSION",     p100},
        {"ROCKMAN WORLD",    p50F},
        {"ROCKMAN WORLD2",   p50F},
        {"ROCKMANWORLD3",    p50F},
        {"SEIKEN DENSETSU",  p50E},
        {"SOCCER",           p502},
        {"SOLARSTRIKER",     p013},
        {"SPACE INVADERS",   p013},
        {"STAR STACKER",     p012},
        {"STAR WARS",        p512},
        {"STAR WARS-NOA",    p512},
        {"STREET FIGHTER 2", p50F},
        {"SUPER BOMBLISS  ", p006}, // unofficial ("TETRIS BLAST" alt.)
        {"SUPER MARIOLAND",  p30A},
        {"SUPER RC PRO-AM",  p50F},
        {"SUPERDONKEYKONG",  p501},
        {"SUPERMARIOLAND3",  p500},
        {"TENNIS",           p502},
        {"TETRIS",           p007},
        {"TETRIS ATTACK",    p405},
        {"TETRIS BLAST",     p006},
        {"TETRIS FLASH",     p407},
        {"TETRIS PLUS",      p31C},
        {"TETRIS2",          p407},
        {"THE CHESSMASTER",  p30F},
        {"TOPRANKINGTENNIS", p502},
        {"TOPRANKTENNIS",    p502},
        {"TOY STORY",        p30E},
        {"TRIP WORLD",       p500}, // unofficial
        {"VEGAS STAKES",     p50E},
        {"WARIO BLAST",      p31C},
        {"WARIOLAND2",       p515},
        {"WAVERACE",         p50B},
        {"WORLD CUP",        p30E},
        {"X",                p016},
        {"YAKUMAN",          p012},
        {"YOSHI'S COOKIE",   p406},
        {"YOSSY NO COOKIE",  p406},
        {"YOSSY NO PANEPON", p405},
        {"YOSSY NO TAMAGO",  p305},
        {"ZELDA",            p511},
};

static const u32* findPalette(const std::string& title) {
    for(u32 i = 0; i < (sizeof gbcPalettes) / (sizeof gbcPalettes[0]); i++) {
        if(gbcPalettes[i].title == title) {
            return gbcPalettes[i].p;
        }
    }

    return nullptr;
}

#define NS_PER_FRAME ((s64) (1000000000.0 / ((double) CYCLES_PER_SECOND / (double) CYCLES_PER_FRAME)))

static Gameboy* gameboy = nullptr;

static int fastForwardCounter;

static u32 audioBuffer[2048];

static std::chrono::time_point<std::chrono::high_resolution_clock> lastFrameTime;
static std::chrono::time_point<std::chrono::system_clock> lastPrintTime;
static int fps;
static bool fastForward;

static std::string romDir;
static std::string romName;

static u32 numPrinted;

static int autoFireCounterA;
static int autoFireCounterB;

static bool emulationPaused;

static u8 optToConfigGroup[NUM_GB_OPT] = {
        GROUP_GAMEBOY,
        GROUP_GAMEBOY,
        GROUP_GAMEBOY,
        GROUP_GAMEBOY,
        GROUP_GAMEBOY,
        GROUP_DISPLAY,
        GROUP_DISPLAY,
        0,
        GROUP_SOUND,
        GROUP_SOUND,
        GROUP_SOUND,
        GROUP_SOUND,
        GROUP_SOUND
};

static u8 optToConfigOption[NUM_GB_OPT] = {
        GAMEBOY_SGB_MODE,
        GAMEBOY_GBC_MODE,
        GAMEBOY_GBA_MODE,
        GAMEBOY_BIOS,
        GAMEBOY_GB_PRINTER,
        0,
        DISPLAY_RENDERING_MODE,
        DISPLAY_EMULATE_BLUR,
        SOUND_MASTER,
        SOUND_CHANNEL_1,
        SOUND_CHANNEL_2,
        SOUND_CHANNEL_3,
        SOUND_CHANNEL_4
};

static u8 mgrGetOption(GameboyOption opt) {
    if(opt == GB_OPT_DRAW_ENABLED) {
        return (u8) (!mgrGetFastForward() || fastForwardCounter >= configGetMultiChoice(GROUP_DISPLAY, DISPLAY_FF_FRAME_SKIP));
    } else {
        return configGetMultiChoice(optToConfigGroup[opt], optToConfigOption[opt]);
    }
}

static std::string mgrGetBasePath(u8 option) {
    std::string basePath = configGetPath(GROUP_GAMEYOB, option);
    if(basePath.empty()) {
        basePath = romDir;
    }

    std::stringstream stream;

    stream << basePath;
    if(basePath.empty() || basePath[basePath.length() - 1] != '/') {
        stream << "/";
    }

    stream << romName;
    return stream.str();
}

static std::string mgrGetStatePath(int stateNum) {
    std::stringstream stream;

    stream << mgrGetBasePath(GAMEYOB_SAVE_STATE_PATH);
    if(stateNum == -1) {
        stream << ".yss";
    } else {
        stream << ".ys" << stateNum;
    }

    return stream.str();
}

static std::string mgrGetPrintPath(bool& appending) {
    // Find the first available "print number".
    const std::string baseFile = mgrGetBasePath(GAMEYOB_PRINT_PATH);
    while(true) {
        std::stringstream finalStream;
        finalStream << baseFile << "-" << numPrinted << ".bmp";

        const std::string finalFile = finalStream.str();

        // If appending, the last file written to is already selected.
        // Else, if the file doesn't exist, we're done searching.
        bool exists = access(finalFile.c_str(), R_OK) == 0;
        if(appending || !exists) {
            if(appending && !exists) {
                // This is a failsafe, this shouldn't happen.
                appending = false;
                mgrPrintDebug("The image to be appended to doesn't exist: %s\n", finalFile.c_str());
            }

            return finalFile;
        }

        numPrinted++;
    }
}

static void mgrPrintImage(bool appending, u8* buf, int size, u8 palette) {
    int width = PRINTER_WIDTH;

    // In case of error, size must be rounded off to the nearest 16 vertical pixels.
    if(size % (width / 4 * 16) != 0) {
        size += (width / 4 * 16) - (size % (width / 4 * 16));
    }

    int height = size / width * 4;
    int pixelArraySize = (width * height + 1) / 2;

    u8 bmpHeader[] = { // Contains header data & palettes
            0x42, 0x4d, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46, 0x00, 0x00, 0x00, 0x28, 0x00,
            0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x04, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x12, 0x0b, 0x00, 0x00, 0x12, 0x0b, 0x00, 0x00, 0x04, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x55, 0x55, 0x55, 0x55, 0xaa, 0xaa,
            0xaa, 0xaa, 0xff, 0xff, 0xff, 0xff
    };

    // Set up the palette
    for(int i = 0; i < 4; i++) {
        u8 rgb = 0;
        switch((palette >> (i * 2)) & 3) {
            case 0:
                rgb = 0xff;
                break;
            case 1:
                rgb = 0xaa;
                break;
            case 2:
                rgb = 0x55;
                break;
            case 3:
                rgb = 0x00;
                break;
            default:
                break;
        }

        for(int j = 0; j < 4; j++) {
            bmpHeader[0x36 + i * 4 + j] = rgb;
        }
    }

    u16* pixelData = (u16*) malloc((size_t) pixelArraySize);

    // Convert the gameboy's tile-based 2bpp into a linear 4bpp format.
    for(int i = 0; i < size; i += 2) {
        u8 b1 = buf[i];
        u8 b2 = buf[i + 1];

        int pixel = i * 4;
        int tile = pixel / 64;

        int index = tile / 20 * width * 8;
        index += (tile % 20) * 8;
        index += ((pixel % 64) / 8) * width;
        index += (pixel % 8);
        index /= 4;

        pixelData[index] = 0;
        pixelData[index + 1] = 0;
        for(int j = 0; j < 2; j++) {
            pixelData[index] |= (((b1 >> j >> 4) & 1) | (((b2 >> j >> 4) & 1) << 1)) << (j * 4 + 8);
            pixelData[index] |= (((b1 >> j >> 6) & 1) | (((b2 >> j >> 6) & 1) << 1)) << (j * 4);
            pixelData[index + 1] |= (((b1 >> j) & 1) | (((b2 >> j) & 1) << 1)) << (j * 4 + 8);
            pixelData[index + 1] |= (((b1 >> j >> 2) & 1) | (((b2 >> j >> 2) & 1) << 1)) << (j * 4);
        }
    }

    const std::string path = mgrGetPrintPath(appending);

    if(appending) {
        std::fstream stream(path, std::fstream::in | std::fstream::out | std::fstream::binary | std::fstream::ate);
        s64 end = stream.tellg();

        int temp = 0;

        // Update height
        stream.seekg(0x16);
        stream.read((char*) &temp, sizeof(temp));
        temp = -(height + (-temp));
        stream.seekg(0x16);
        stream.write((char*) &temp, sizeof(temp));

        // Update pixelArraySize
        stream.seekg(0x22);
        stream.read((char*) &temp, sizeof(temp));
        temp += pixelArraySize;
        stream.seekg(0x22);
        stream.write((char*) &temp, sizeof(temp));

        // Update file size
        temp += sizeof(bmpHeader);
        stream.seekg(0x2);
        stream.write((char*) &temp, sizeof(temp));

        // Append pixel data
        stream.seekg(end);
        stream.write((char*) pixelData, pixelArraySize);

        stream.close();
    } else { // Not appending; making a file from scratch
        std::fstream stream(path, std::fstream::out | std::fstream::binary);

        *(u32*) (bmpHeader + 0x02) = sizeof(bmpHeader) + pixelArraySize;
        *(u32*) (bmpHeader + 0x22) = (u32) pixelArraySize;
        *(u32*) (bmpHeader + 0x12) = (u32) width;
        *(u32*) (bmpHeader + 0x16) = (u32) -height;

        stream.write((char*) bmpHeader, sizeof(bmpHeader));
        stream.write((char*) pixelData, pixelArraySize);
        stream.close();
    }

    free(pixelData);
}

void mgrInit() {
    gameboy = new Gameboy();

    gameboy->settings.printDebug = mgrPrintDebug;

    gameboy->settings.readTilt = inputGetMotionSensor;
    gameboy->settings.setRumble = inputSetRumble;

    gameboy->settings.getCameraImage = systemGetCameraImage;

    gameboy->settings.printImage = mgrPrintImage;

    gameboy->settings.getOption = mgrGetOption;

    gameboy->settings.frameBuffer = gfxGetScreenBuffer();
    gameboy->settings.framePitch = gfxGetScreenPitch();

    gameboy->settings.audioBuffer = audioBuffer;
    gameboy->settings.audioSamples = sizeof(audioBuffer) / sizeof(u32);
    gameboy->settings.audioSampleRate = audioGetSampleRate();

    fastForwardCounter = 0;

    memset(audioBuffer, 0, sizeof(audioBuffer));

    lastFrameTime = std::chrono::high_resolution_clock::now();
    lastPrintTime = std::chrono::system_clock::now();
    fastForward = false;
    fps = 0;

    romDir = "";
    romName = "";

    autoFireCounterA = 0;
    autoFireCounterB = 0;

    emulationPaused = false;

    configLoad();
}

void mgrExit() {
    mgrUnloadRom(true, true);

    if(gameboy != nullptr) {
        delete gameboy;
        gameboy = nullptr;
    }
}

void mgrPrintDebug(const char* str, ...) {
#ifndef BACKEND_SWITCH
    if(configGetMultiChoice(GROUP_GAMEYOB, GAMEYOB_CONSOLE_OUTPUT) == CONSOLE_OUTPUT_DEBUG && !menuIsVisible()) {
        va_list list;
        va_start(list, str);
        uiPrintv(str, list);
        va_end(list);

        uiFlush();
    }
#endif
}

Gameboy* mgrGetGameboy() {
    return gameboy;
}

bool mgrGetFastForward() {
    return fastForward || (!menuIsVisible() && inputKeyHeld(FUNC_KEY_FAST_FORWARD));
}

static bool mgrIsPaused() {
    return emulationPaused || (menuIsVisible() && configGetMultiChoice(GROUP_GAMEYOB, GAMEYOB_PAUSE_IN_MENU) == PAUSE_IN_MENU_ON);
}

void mgrRefreshPalette() {
    if(gameboy == nullptr || gameboy->gbMode != MODE_GB) {
        return;
    }

    const u32* palette = nullptr;
    switch(configGetMultiChoice(GROUP_DISPLAY, DISPLAY_COLORIZE_GB)) {
        case COLORIZE_GB_OFF:
            palette = findPalette("GBC - Grayscale");
            break;
        case COLORIZE_GB_AUTO:
            if(gameboy->cartridge != nullptr) {
                palette = findPalette(gameboy->cartridge->getRomTitle());
            }

            if(palette == nullptr) {
                palette = findPalette("GBC - Grayscale");
            }

            break;
        case COLORIZE_GB_INVERTED:
            palette = findPalette("GBC - Inverted");
            break;
        case COLORIZE_GB_PASTEL_MIX:
            palette = findPalette("GBC - Pastel Mix");
            break;
        case COLORIZE_GB_RED:
            palette = findPalette("GBC - Red");
            break;
        case COLORIZE_GB_ORANGE:
            palette = findPalette("GBC - Orange");
            break;
        case COLORIZE_GB_YELLOW:
            palette = findPalette("GBC - Yellow");
            break;
        case COLORIZE_GB_GREEN:
            palette = findPalette("GBC - Green");
            break;
        case COLORIZE_GB_BLUE:
            palette = findPalette("GBC - Blue");
            break;
        case COLORIZE_GB_BROWN:
            palette = findPalette("GBC - Brown");
            break;
        case COLORIZE_GB_DARK_GREEN:
            palette = findPalette("GBC - Dark Green");
            break;
        case COLORIZE_GB_DARK_BLUE:
            palette = findPalette("GBC - Dark Blue");
            break;
        case COLORIZE_GB_DARK_BROWN:
            palette = findPalette("GBC - Dark Brown");
            break;
        case COLORIZE_GB_CLASSIC_GREEN:
            palette = findPalette("GB - Classic");
            break;
        default:
            palette = findPalette("GBC - Grayscale");
            break;
    }

    memcpy(gameboy->ppu.getBgPalette(), palette, 4 * sizeof(u32));
    memcpy(gameboy->ppu.getSprPalette(), palette + 4, 4 * sizeof(u32));
    memcpy(gameboy->ppu.getSprPalette() + 4 * 4, palette + 8, 4 * sizeof(u32));
}

static bool mgrTryRawBorderFile(const std::string& border) {
    std::ifstream stream(border, std::ios::binary);
    if(stream.is_open()) {
        stream.close();

        int imgWidth;
        int imgHeight;
        int imgDepth;
        u8* image = stbi_load(border.c_str(), &imgWidth, &imgHeight, &imgDepth, STBI_rgb_alpha);
        if(image == nullptr) {
            mgrPrintDebug("Failed to decode image file.\n");
            return false;
        }

        u8* imgData = new u8[imgWidth * imgHeight * sizeof(u32)];
        for(int x = 0; x < imgWidth; x++) {
            for(int y = 0; y < imgHeight; y++) {
                int offset = (y * imgWidth + x) * 4;
                imgData[offset + 0] = image[offset + 3];
                imgData[offset + 1] = image[offset + 2];
                imgData[offset + 2] = image[offset + 1];
                imgData[offset + 3] = image[offset + 0];
            }
        }

        stbi_image_free(image);

        gfxLoadBorder(imgData, imgWidth, imgHeight);
        delete[] imgData;

        return true;
    }

    return false;
}

static bool mgrTryBorderFile(const std::string& border) {
    if(configGetMultiChoice(GROUP_DISPLAY, DISPLAY_CUSTOM_BORDERS_SCALING) == CUSTOM_BORDERS_SCALING_PRE_SCALED) {
        std::string name;
        std::string extension;

        std::string::size_type dotPos = border.rfind('.');
        if(dotPos != std::string::npos) {
            name = border.substr(0, dotPos);
            extension = border.substr(dotPos);
        } else {
            name = border;
        }

        std::string scaleName = configGetMultiChoiceValueName(GROUP_DISPLAY, DISPLAY_SCALING_MODE, configGetMultiChoice(GROUP_DISPLAY, DISPLAY_SCALING_MODE));
        std::transform(scaleName.begin(), scaleName.end(), scaleName.begin(), ::tolower);

        if(mgrTryRawBorderFile(name + "_" + scaleName + extension)) {
            return true;
        }
    }

    return mgrTryRawBorderFile(border);
}

void mgrRefreshBorder() {
    gfxLoadBorder(nullptr, 0, 0);

    if(gameboy != nullptr && gameboy->cartridge != nullptr && configGetMultiChoice(GROUP_DISPLAY, DISPLAY_CUSTOM_BORDERS) == CUSTOM_BORDERS_ON) {
        std::vector<std::string> supportedExtensions = configGetPathExtensions(GROUP_DISPLAY, DISPLAY_CUSTOM_BORDER_PATH);

        const std::string basePath = mgrGetBasePath(GAMEYOB_BORDER_PATH);

        bool foundRomSpecific = false;
        for(const std::string& extension : supportedExtensions) {
            if(mgrTryBorderFile(basePath + "." + extension)) {
                foundRomSpecific = true;
                break;
            }
        }

        if(!foundRomSpecific) {
            mgrTryBorderFile(configGetPath(GROUP_DISPLAY, DISPLAY_CUSTOM_BORDER_PATH));
        }
    }
}

bool mgrStateExists(int stateNum) {
    if(gameboy == nullptr || gameboy->cartridge == nullptr) {
        return false;
    }

    std::ifstream stream(mgrGetStatePath(stateNum), std::ios::binary);
    if(stream.is_open()) {
        stream.close();
        return true;
    }

    return false;
}

bool mgrLoadState(int stateNum) {
    if(gameboy == nullptr || gameboy->cartridge == nullptr) {
        return false;
    }

    std::ifstream stream(mgrGetStatePath(stateNum), std::ios::binary);
    if(!stream.is_open()) {
        mgrPrintDebug("Failed to open state file: %s\n", strerror(errno));
        return false;
    }

    mgrReset();

    bool ret = gameboy->loadState(stream);
    stream.close();
    return ret;
}

bool mgrSaveState(int stateNum) {
    if(gameboy == nullptr || gameboy->cartridge == nullptr) {
        return false;
    }

    std::ofstream stream(mgrGetStatePath(stateNum), std::ios::binary);
    if(!stream.is_open()) {
        mgrPrintDebug("Failed to open state file: %s\n", strerror(errno));
        return false;
    }

    bool ret = gameboy->saveState(stream);
    stream.close();
    return ret;
}

void mgrDeleteState(int stateNum) {
    if(gameboy == nullptr || gameboy->cartridge == nullptr) {
        return;
    }

    remove(mgrGetStatePath(stateNum).c_str());
}

static int mgrParseCheats(void* user, const char* section, const char* name, const char* value) {
    CheatEngine* engine = (CheatEngine*) user;

    if(strcmp(name, "value") == 0) {
        engine->addCheat(std::string(section), std::string(value));
    } else if(strcmp(name, "enabled") == 0) {
        u32 numCheats = gameboy->cheatEngine.getNumCheats();
        for(u32 i = 0; i < numCheats; i++) {
            if(gameboy->cheatEngine.getCheatName(i) == section) {
                engine->toggleCheat(i, strcmp(value, "1") == 0);
                break;
            }
        }
    }

    return 1;
}

static void mgrSaveCheats() {
    if(gameboy == nullptr || gameboy->cartridge == nullptr) {
        return;
    }

    const std::string path = mgrGetBasePath(GAMEYOB_CHEATS_PATH) + ".cht";

    u32 numCheats = gameboy->cheatEngine.getNumCheats();
    if(numCheats == 0) {
        remove(path.c_str());
        return;
    }

    std::ofstream stream(path);
    if(stream.is_open()) {
        for(u32 i = 0; i < numCheats; i++) {
            stream << "[" << gameboy->cheatEngine.getCheatName(i) << "]\n";
            stream << "value=" << gameboy->cheatEngine.getCheatValue(i) << "\n";
            stream << "enabled=" << (gameboy->cheatEngine.isCheatEnabled(i) ? 1 : 0) << "\n";
            stream << "\n";
        }

        stream.close();
    }
}

static void mgrWriteSave() {
    if(gameboy == nullptr || gameboy->cartridge == nullptr) {
        return;
    }

    std::ofstream stream(mgrGetBasePath(GAMEYOB_SAVE_PATH) + ".sav", std::ios::binary);
    if(!stream.is_open()) {
        mgrPrintDebug("Failed to open save file: %s\n", strerror(errno));
        return;
    }

    gameboy->cartridge->save(stream);
    stream.close();
}

static void mgrRefreshState() {
    mgrRefreshBorder();
    mgrRefreshPalette();

    memset(gfxGetScreenBuffer(), 0, gfxGetScreenPitch() * GB_FRAME_HEIGHT * sizeof(u32));
    gfxDrawScreen();
}

static void mgrLoadRom(const std::string& romFile) {
    if(gameboy == nullptr) {
        return;
    }

    gameboy->powerOff();

    if(!romFile.empty()) {
        std::ifstream romStream(romFile, std::ios::binary | std::ios::ate);
        if(!romStream.is_open()) {
            mgrPrintDebug("Failed to open ROM file: %s\n", strerror(errno));
            return;
        }

        u32 romSize = (u32) romStream.tellg();
        romStream.seekg(0);

        std::string::size_type dot = romFile.find_last_of('.');
        if(dot != std::string::npos) {
            romName = romFile.substr(0, dot);
        } else {
            romName = romFile;
        }

        std::string::size_type slash = romName.find_last_of('/');
        if(slash != std::string::npos) {
            romDir = romName.substr(0, slash + 1);
            romName = romName.substr(slash + 1);
        } else {
            romDir = "/";
        }

        gameboy->insert(new Cartridge(romStream, romSize));

        romStream.close();

        std::ifstream saveStream(mgrGetBasePath(GAMEYOB_SAVE_PATH) + ".sav", std::ios::binary);
        if(saveStream.is_open()) {
            gameboy->cartridge->load(saveStream);
            saveStream.close();
        } else {
            mgrPrintDebug("Failed to open save file: %s\n", strerror(errno));
        }

        const std::string cheatsPath = mgrGetBasePath(GAMEYOB_CHEATS_PATH) + ".cht";
        ini_parse(cheatsPath.c_str(), mgrParseCheats, &gameboy->cheatEngine);
    }

    gameboy->powerOn();

    mgrRefreshState();

    if(mgrStateExists(-1)) {
        mgrLoadState(-1);
        mgrDeleteState(-1);
    }
}

void mgrUnloadRom(bool save, bool exiting) {
    if(gameboy == nullptr) {
        return;
    }

    gameboy->powerOff();

    if(gameboy->cartridge != nullptr) {
        if(save) {
            mgrWriteSave();
        }

        mgrSaveCheats();

        Cartridge* cart = gameboy->cartridge;
        gameboy->insert(nullptr);
        delete cart;
    }

    romDir = "";
    romName = "";

    if(!exiting) {
        mgrRefreshState();
    }
}

void mgrReset() {
    if(gameboy == nullptr) {
        return;
    }

    gameboy->powerOff();
    gameboy->powerOn();

    mgrRefreshState();
}

void mgrRun() {
    inputUpdate();

    if(!gameboy->isPoweredOn() && !menuIsVisible()) {
        std::string romPath = configGetPath(GROUP_GAMEYOB, GAMEYOB_ROM_PATH);
        DIR* dir = opendir(romPath.c_str());
        if(dir == nullptr) {
            romPath = "/";
        } else {
            closedir(dir);
        }

        // TODO: Preserve path changes.
        menuPush(new FileChooser([](bool chosen, const std::string& path) -> void {
            if(chosen)  {
                mgrLoadRom(path);
            } else {
                systemRequestExit();
            }
        }, romPath, {"sgb", "gbc", "cgb", "gb"}, false));
    }

    menuUpdate();
    emulationPaused = emulationPaused && menuIsVisible();

    if(gameboy->isPoweredOn()) {
        if(inputKeyPressed(FUNC_KEY_SAVE)) {
            mgrWriteSave();
            mgrSaveCheats();
        }

        if(inputKeyPressed(FUNC_KEY_FAST_FORWARD_TOGGLE)) {
            fastForward = !fastForward;
        }

        if(inputKeyPressed(FUNC_KEY_SCALE)) {
            configToggleMultiChoice(GROUP_DISPLAY, DISPLAY_SCALING_MODE, 1);
        }

        if(inputKeyPressed(FUNC_KEY_RESET)) {
            mgrReset();
        }

        if(inputKeyPressed(FUNC_KEY_MENU) || inputKeyPressed(FUNC_KEY_MENU_PAUSE)) {
            if(inputKeyPressed(FUNC_KEY_MENU_PAUSE)) {
                emulationPaused = true;
            }

            menuOpenMain();
        }

        auto frameTime = std::chrono::high_resolution_clock::now();
        s64 nsPassed = std::chrono::duration_cast<std::chrono::nanoseconds>(frameTime - lastFrameTime).count();
        if(!mgrIsPaused() && (mgrGetFastForward() || nsPassed >= NS_PER_FRAME)) {
            lastFrameTime = frameTime;

            u8 buttonsPressed = 0xFF;

            if(!menuIsVisible()) {
                if(inputKeyHeld(FUNC_KEY_UP)) {
                    buttonsPressed &= ~GB_UP;
                }

                if(inputKeyHeld(FUNC_KEY_DOWN)) {
                    buttonsPressed &= ~GB_DOWN;
                }

                if(inputKeyHeld(FUNC_KEY_LEFT)) {
                    buttonsPressed &= ~GB_LEFT;
                }

                if(inputKeyHeld(FUNC_KEY_RIGHT)) {
                    buttonsPressed &= ~GB_RIGHT;
                }

                if(inputKeyHeld(FUNC_KEY_A)) {
                    buttonsPressed &= ~GB_A;
                }

                if(inputKeyHeld(FUNC_KEY_B)) {
                    buttonsPressed &= ~GB_B;
                }

                if(inputKeyHeld(FUNC_KEY_START)) {
                    buttonsPressed &= ~GB_START;
                }

                if(inputKeyHeld(FUNC_KEY_SELECT)) {
                    buttonsPressed &= ~GB_SELECT;
                }

                if(inputKeyHeld(FUNC_KEY_AUTO_A)) {
                    if(autoFireCounterA <= 0) {
                        buttonsPressed &= ~GB_A;
                        autoFireCounterA = 2;
                    }

                    autoFireCounterA--;
                }

                if(inputKeyHeld(FUNC_KEY_AUTO_B)) {
                    if(autoFireCounterB <= 0) {
                        buttonsPressed &= ~GB_B;
                        autoFireCounterB = 2;
                    }

                    autoFireCounterB--;
                }
            }

            gameboy->sgb.setController(0, buttonsPressed);
            gameboy->runFrame();

            if(configGetMultiChoice(GROUP_SOUND, SOUND_MASTER) == SOUND_ON) {
                audioPlay(audioBuffer, gameboy->audioSamplesWritten);
            }

            if(!mgrGetFastForward() || fastForwardCounter++ >= configGetMultiChoice(GROUP_DISPLAY, DISPLAY_FF_FRAME_SKIP)) {
                fastForwardCounter = 0;
                gfxDrawScreen();
            }

#ifndef BACKEND_SWITCH
            fps++;

            auto printTime = std::chrono::system_clock::now();
            s64 secPassed = std::chrono::duration_cast<std::chrono::seconds>(printTime - lastPrintTime).count();
            if(secPassed > 0) {
                lastPrintTime = printTime;

                u8 mode = configGetMultiChoice(GROUP_GAMEYOB, GAMEYOB_CONSOLE_OUTPUT);
                bool showFPS = mode == CONSOLE_OUTPUT_FPS || mode == CONSOLE_OUTPUT_FPS_TIME;
                bool showTime = mode == CONSOLE_OUTPUT_TIME || mode == CONSOLE_OUTPUT_FPS_TIME;

                if(!menuIsVisible() && (showFPS || showTime)) {
                    uiClear();

                    u32 width = 0;
                    uiGetSize(&width, nullptr);

                    if(showFPS) {
                        std::stringstream fpsStream;
                        fpsStream << "FPS: " << fps;
                        std::string fpsString = fpsStream.str();

                        uiPrint("%s", fpsString.c_str());

                        width -= fpsString.length();
                    }

                    if(showTime) {
                        std::time_t timet = std::chrono::system_clock::to_time_t(printTime);
                        std::tm* local = std::localtime(&timet);

                        std::stringstream timeStream;
                        timeStream << std::put_time(local, "%H:%M");
                        std::string timeString = timeStream.str();

                        uiAdvanceCursor(width - timeString.length());
                        uiPrint("%s", timeString.c_str());
                    }

                    uiFlush();
                }

                fps = 0;
            }
#endif
        }
    }
}
