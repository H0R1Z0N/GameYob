#include <cstring>
#include <istream>
#include <ostream>
#include <gameboy.h>

#include "apu.h"
#include "cartridge.h"
#include "cpu.h"
#include "gameboy.h"
#include "mmu.h"
#include "ppu.h"

#include "bios_bin.h"
#include "dummy_bios_bin.h"

static const u8 initialHramGB[0x100] = {
        0xCF, 0x00, 0x7E, 0xFF, 0xD3, 0x00, 0x00, 0xF8,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xE1,
        0x80, 0xBF, 0xF3, 0xFF, 0xBF, 0xFF, 0x3F, 0x00,
        0xFF, 0xBF, 0x7F, 0xFF, 0x9F, 0xFF, 0xBF, 0xFF,
        0xFF, 0x00, 0x00, 0xBF, 0x77, 0xF3, 0xF1, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0x71, 0x72, 0xD5, 0x91, 0x58, 0xBB, 0x2A, 0xFA,
        0xCF, 0x3C, 0x54, 0x75, 0x48, 0xCF, 0x8F, 0xD9,
        0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFC,
        0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0x2B, 0x0B, 0x64, 0x2F, 0xAF, 0x15, 0x60, 0x6D,
        0x61, 0x4E, 0xAC, 0x45, 0x0F, 0xDA, 0x92, 0xF3,
        0x83, 0x38, 0xE4, 0x4E, 0xA7, 0x6C, 0x38, 0x58,
        0xBE, 0xEA, 0xE5, 0x81, 0xB4, 0xCB, 0xBF, 0x7B,
        0x59, 0xAD, 0x50, 0x13, 0x5E, 0xF6, 0xB3, 0xC1,
        0xDC, 0xDF, 0x9E, 0x68, 0xD7, 0x59, 0x26, 0xF3,
        0x62, 0x54, 0xF8, 0x36, 0xB7, 0x78, 0x6A, 0x22,
        0xA7, 0xDD, 0x88, 0x15, 0xCA, 0x96, 0x39, 0xD3,
        0xE6, 0x55, 0x6E, 0xEA, 0x90, 0x76, 0xB8, 0xFF,
        0x50, 0xCD, 0xB5, 0x1B, 0x1F, 0xA5, 0x4D, 0x2E,
        0xB4, 0x09, 0x47, 0x8A, 0xC4, 0x5A, 0x8C, 0x4E,
        0xE7, 0x29, 0x50, 0x88, 0xA8, 0x66, 0x85, 0x4B,
        0xAA, 0x38, 0xE7, 0x6B, 0x45, 0x3E, 0x30, 0x37,
        0xBA, 0xC5, 0x31, 0xF2, 0x71, 0xB4, 0xCF, 0x29,
        0xBC, 0x7F, 0x7E, 0xD0, 0xC7, 0xC3, 0xBD, 0xCF,
        0x59, 0xEA, 0x39, 0x01, 0x2E, 0x00, 0x69, 0x00
};

static const u8 initialHramCGB[0x100] = {
        0xCF, 0x00, 0x7C, 0xFF, 0x44, 0x00, 0x00, 0xF8,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xE1,
        0x80, 0xBF, 0xF3, 0xFF, 0xBF, 0xFF, 0x3F, 0x00,
        0xFF, 0xBF, 0x7F, 0xFF, 0x9F, 0xFF, 0xBF, 0xFF,
        0xFF, 0x00, 0x00, 0xBF, 0x77, 0xF3, 0xF1, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF,
        0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF,
        0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFC,
        0x00, 0x00, 0x00, 0x00, 0xFF, 0x7E, 0xFF, 0xFE,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x3E, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xC0, 0xFF, 0xC1, 0x00, 0xFE, 0xFF, 0xFF, 0xFF,
        0xF8, 0xFF, 0x00, 0x00, 0x00, 0x8F, 0x00, 0x00,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B,
        0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D,
        0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E,
        0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99,
        0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC, 0xCC,
        0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E,
        0x45, 0xEC, 0x42, 0xFA, 0x08, 0xB7, 0x07, 0x5D,
        0x01, 0xF5, 0xC0, 0xFF, 0x08, 0xFC, 0x00, 0xE5,
        0x0B, 0xF8, 0xC2, 0xCA, 0xF4, 0xF9, 0x0D, 0x7F,
        0x44, 0x6D, 0x19, 0xFE, 0x46, 0x97, 0x33, 0x5E,
        0x08, 0xFF, 0xD1, 0xFF, 0xC6, 0x8B, 0x24, 0x74,
        0x12, 0xFC, 0x00, 0x9F, 0x94, 0xB7, 0x06, 0xD5,
        0x40, 0x7A, 0x20, 0x9E, 0x04, 0x5F, 0x41, 0x2F,
        0x3D, 0x77, 0x36, 0x75, 0x81, 0x8A, 0x70, 0x3A,
        0x98, 0xD1, 0x71, 0x02, 0x4D, 0x01, 0xC1, 0xFF,
        0x0D, 0x00, 0xD3, 0x05, 0xF9, 0x00, 0x0B, 0x00
};

MMU::MMU(Gameboy* gameboy) {
    this->gameboy = gameboy;
}

void MMU::reset() {
    memset(this->pages, 0, sizeof(this->pages));
    memset(this->pageRead, 0, sizeof(this->pageRead));
    memset(this->pageWrite, 0, sizeof(this->pageWrite));

    for(int i = 0; i < 8; i++) {
        memset(this->wram[i], 0, sizeof(this->wram[i]));
    }

    memcpy(this->hram, this->gameboy->gbMode == MODE_CGB ? (const u8*) initialHramCGB : initialHramGB, sizeof(this->hram));

    this->biosMapped = true;
    this->useRealBios = this->gameboy->settings.getOption(GB_OPT_BIOS_ENABLED);

    this->mapBanks();
}

u8 MMU::read(u16 addr) {
    u8 area = (u8) (addr >> 12);
    if(this->pageRead[area]) {
        return this->pages[area][addr & 0xFFF];
    }

    switch(area) {
        case 0x0:
            if(this->biosMapped) {
                if((addr >= 0x000 && addr < 0x100) || (addr >= 0x200 && addr < 0x900)) {
                    return (this->useRealBios ? bios_bin : dummy_bios_bin)[addr & 0xFFF];
                } else if(this->gameboy->cartridge != nullptr) {
                    return this->gameboy->cartridge->getRomBank(0)[addr & 0xFFF];
                }
            }

            return 0xFF;
        case 0xA:
        case 0xB:
            if(this->gameboy->cartridge != nullptr) {
                mbcRead read = this->gameboy->cartridge->getReadFunc();
                if(read != nullptr) {
                    return (this->gameboy->cartridge->*read)(addr);
                }
            }

            return 0xFF;
        case 0xF:
            if(addr >= 0xFF00) {
                if(addr == DIV || addr == TIMA || addr == TMA || addr == TAC) {
                    return this->gameboy->timer.read(addr);
                } else if((addr >= NR10 && addr <= WAVEF) || addr == PCM12 || addr == PCM34) {
                    return this->gameboy->apu.read(addr);
                } else {
                    return this->hram[addr & 0xFF];
                }
            } else if(addr < 0xFE00) {
                u8 wramBank = (u8) (this->readIO(SVBK) & 0x7);
                return this->wram[wramBank != 0 ? wramBank : 1][addr & 0xFFF];
            } else if(addr < 0xFEA0) {
                return this->gameboy->ppu.readOam(addr);
            }

            return 0xFF;
        default: {
            if(this->gameboy->settings.printDebug != nullptr) {
                this->gameboy->settings.printDebug("Attempted to read from unmapped memory page: 0x%x\n", area);
            }

            return 0xFF;
        }
    }
}

void MMU::write(u16 addr, u8 val) {
    u8 area = (u8) (addr >> 12);
    if(this->pageWrite[area]) {
        this->pages[area][addr & 0xFFF] = val;
        return;
    }

    switch(area) {
        case 0x0:
        case 0x1:
        case 0x2:
        case 0x3:
        case 0x4:
        case 0x5:
        case 0x6:
        case 0x7:
            if(this->gameboy->cartridge != nullptr) {
                mbcWrite write = this->gameboy->cartridge->getWriteFunc();
                if(write != nullptr) {
                    (this->gameboy->cartridge->*write)(addr, val);
                }
            }

            break;
        case 0xA:
        case 0xB:
            if(this->gameboy->cartridge != nullptr) {
                mbcWrite writeSram = this->gameboy->cartridge->getWriteSramFunc();
                if(writeSram != nullptr) {
                    (this->gameboy->cartridge->*writeSram)(addr, val);
                }
            }

            break;
        case 0xF:
            if(addr >= 0xFF00) {
                if((addr >= NR10 && addr <= WAVEF) || addr == PCM12 || addr == PCM34) {
                    this->gameboy->apu.write(addr, val);
                } else {
                    switch(addr) {
                        case BIOS:
                            if(this->biosMapped) {
                                this->biosMapped = false;

                                // Reset cartridge to map ROM bank 0.
                                this->gameboy->cartridge->reset(this->gameboy);
                                this->mapBanks();
                            }

                            break;
                        case RP:
                            if(this->gameboy->gbMode == MODE_CGB) {
                                // TODO: IR communication.
                                this->writeIO(RP, (u8) ((val & ~0x2) | 0x3C | 0x2));
                            }

                            break;
                        case SVBK:
                            if(this->gameboy->gbMode == MODE_CGB) {
                                this->writeIO(SVBK, (u8) (val | 0xF8));
                                this->mapBanks();
                            }

                            break;
                        case IF:
                        case KEY1:
                            this->gameboy->cpu.write(addr, val);
                            break;
                        case LCDC:
                        case STAT:
                        case LY:
                        case LYC:
                        case DMA:
                        case BGP:
                        case OBP0:
                        case OBP1:
                        case WX:
                        case VBK:
                        case BCPS:
                        case BCPD:
                        case OCPS:
                        case OCPD:
                        case HDMA5:
                            this->gameboy->ppu.write(addr, val);
                            break;
                        case SC:
                            this->gameboy->serial.write(addr, val);
                            break;
                        case JOYP:
                            this->gameboy->sgb.write(addr, val);
                            break;
                        case DIV:
                        case TIMA:
                        case TMA:
                        case TAC:
                            this->gameboy->timer.write(addr, val);
                            break;
                        default:
                            this->hram[addr & 0xFF] = val;
                            break;
                    }
                }
            } else if(addr < 0xFE00) {
                u8 wramBank = (u8) (this->readIO(SVBK) & 0x7);
                this->wram[wramBank != 0 ? wramBank : 1][addr & 0xFFF] = val;
            } else if(addr < 0xFEA0) {
                this->gameboy->ppu.writeOam(addr, val);
            }

            break;
        default: {
            if(this->gameboy->settings.printDebug != nullptr) {
                this->gameboy->settings.printDebug("Attempted to write to unmapped memory page: 0x%x\n", area);
            }

            break;
        }
    }
}

std::istream& operator>>(std::istream& is, MMU& mmu) {
    is.read((char*) mmu.wram, sizeof(mmu.wram));
    is.read((char*) mmu.hram, sizeof(mmu.hram));
    is.read((char*) &mmu.biosMapped, sizeof(mmu.biosMapped));
    is.read((char*) &mmu.useRealBios, sizeof(mmu.useRealBios));

    mmu.mapBanks();

    return is;
}

std::ostream& operator<<(std::ostream& os, const MMU& mmu) {
    os.write((char*) mmu.wram, sizeof(mmu.wram));
    os.write((char*) mmu.hram, sizeof(mmu.hram));
    os.write((char*) &mmu.biosMapped, sizeof(mmu.biosMapped));
    os.write((char*) &mmu.useRealBios, sizeof(mmu.useRealBios));

    return os;
}

void MMU::mapBanks() {
    u8 wramBank = (u8) (this->readIO(SVBK) & 0x7);

    u8* wram0 = this->wram[0];
    u8* wram1 = this->wram[wramBank != 0 ? wramBank : 1];

    this->mapPage(0xC, wram0, true, true);
    this->mapPage(0xD, wram1, true, true);
    this->mapPage(0xE, wram0, true, true);
}