#pragma once

// Interrupt definitions (used with requestInterrupt())
#define VBLANK		1
#define LCD			2
#define TIMER		4
#define SERIAL      8
#define JOYPAD		0x10

#ifdef DS
#include <nds.h>
#else

typedef signed char s8;
typedef signed short s16;
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
#endif

class Gameboy;

extern Gameboy* gameboy;
