#include <istream>
#include <ostream>

#include "cpu.h"
#include "gameboy.h"
#include "mmu.h"
#include "timer.h"

static const u8 timerShifts[] = {
        10, 4, 6, 8
};

Timer::Timer(Gameboy* gameboy) {
    this->gameboy = gameboy;
}

void Timer::reset() {
    this->lastDividerCycle = 0;
    this->lastTimerCycle = 0;
}

void Timer::update() {
    u8 tac = this->gameboy->mmu.readIO(TAC);
    if((tac & 0x4) != 0) {
        u8 shift = timerShifts[tac & 0x3];

        u32 timaAdd = (u32) (this->gameboy->cpu.getCycle() - this->lastTimerCycle) >> shift;
        if(timaAdd > 0) {
            u32 counter = this->gameboy->mmu.readIO(TIMA) + timaAdd;
            if(counter >= 0x100) {
                u8 tma = this->gameboy->mmu.readIO(TMA);
                while(counter >= 0x100) {
                    counter = tma + (counter - 0x100);
                }

                this->gameboy->mmu.writeIO(IF, (u8) (this->gameboy->mmu.readIO(IF) | INT_TIMER));
            }

            this->gameboy->mmu.writeIO(TIMA, (u8) counter);
            this->lastTimerCycle += timaAdd << shift;
        }

        this->gameboy->cpu.setEventCycle(this->lastTimerCycle + ((0x100 - this->gameboy->mmu.readIO(TIMA)) << shift));
    }
}

u8 Timer::read(u16 addr) {
    switch(addr) {
        case DIV: {
            u8 div = (this->gameboy->mmu.readIO(DIV) + ((this->gameboy->cpu.getCycle() - this->lastDividerCycle) >> 8)) & 0xFF;

            this->gameboy->mmu.writeIO(DIV, div);
            this->lastDividerCycle = this->gameboy->cpu.getCycle() & ~0xFF;

            return div;
        }
        case TIMA:
        case TMA:
        case TAC:
            this->update();
            return this->gameboy->mmu.readIO(addr);
        default:
            return 0xFF;
    }
}

void Timer::write(u16 addr, u8 val) {
    switch(addr) {
        case DIV:
            this->gameboy->mmu.writeIO(DIV, 0);
            this->lastDividerCycle = this->gameboy->cpu.getCycle() & ~0xFF;

            break;
        case TIMA:
        case TMA:
            this->update();
            this->gameboy->mmu.writeIO(addr, val);

            break;
        case TAC: {
            this->update();
            this->gameboy->mmu.writeIO(TAC, (u8) (val | 0xF8));

            u8 shift = timerShifts[val & 0x3];
            this->lastTimerCycle = this->gameboy->cpu.getCycle() >> shift << shift;
            this->gameboy->cpu.setEventCycle(this->lastTimerCycle + ((0x100 - this->gameboy->mmu.readIO(TIMA)) << shift));

            break;
        }
        default:
            break;
    }
}

std::istream& operator>>(std::istream& is, Timer& timer) {
    is.read((char*) &timer.lastDividerCycle, sizeof(timer.lastDividerCycle));
    is.read((char*) &timer.lastTimerCycle, sizeof(timer.lastTimerCycle));

    return is;
}

std::ostream& operator<<(std::ostream& os, const Timer& timer) {
    os.write((char*) &timer.lastDividerCycle, sizeof(timer.lastDividerCycle));
    os.write((char*) &timer.lastTimerCycle, sizeof(timer.lastTimerCycle));

    return os;
}
