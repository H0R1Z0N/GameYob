#include <nds.h>
#include <dirent.h>
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
//#include <iostream>
//#include <string>
#include "gbgfx.h"
#include "inputhelper.h"
#include "filechooser.h"
#include "timer.h"
#include "soundengine.h"
#include "gameboy.h"
#include "main.h"
#include "console.h"
#include "nifi.h"
#include "cheats.h"
#include "gbs.h"
#include "common.h"
#include "romfile.h"
#include "menu.h"

void updateVBlank();

time_t rawTime;
time_t lastRawTime;

volatile SharedData* sharedData;
volatile SharedData* dummySharedData;

Gameboy* gameboy;

Gameboy* gb1;
Gameboy* gb2;

RomFile* romFile = NULL;

// This is used to signal sleep mode starting or ending.
void fifoValue32Handler(u32 value, void* user_data) {
    static u8 scalingWasOn;
    static bool soundWasDisabled;
    static bool wasPaused;
    switch(value) {
        case FIFOMSG_LID_CLOSED:
            // Entering sleep mode
            scalingWasOn = sharedData->scalingOn;
            soundWasDisabled = soundDisabled;
            wasPaused = gameboy->isGameboyPaused();

            sharedData->scalingOn = 0;
            soundDisabled = true;
            gameboy->pause();
            break;
        case FIFOMSG_LID_OPENED:
            // Exiting sleep mode
            sharedData->scalingOn = scalingWasOn;
            soundDisabled = soundWasDisabled;
            if (!wasPaused)
                gameboy->unpause();
            // Time isn't incremented properly in sleep mode, compensate here.
            time(&rawTime);
            lastRawTime = rawTime;
            break;
    }
}

void loadRom(const char* filename) {
    if (romFile != NULL)
        delete romFile;

    romFile = new RomFile(filename);
    gb1->setRomFile(romFile);
    gb1->loadSave(1);
    gb2->setRomFile(romFile);
    gb2->loadSave(2);
}

void unloadRom() {
    gb1->unloadRom();
    gb2->unloadRom();

    if (romFile != NULL) {
        delete romFile;
        romFile = NULL;
    }
}

void setMainGb(int id) {
    Gameboy* otherGameboy;

    if (id == 1) {
        gameboy = gb1;
        otherGameboy = gb2;
    }
    else {
        gameboy = gb2;
        otherGameboy = gb1;
    }

    if (gameboy && gameboy->isRomLoaded()) {
        refreshGFX();
    }
    gameboy->getSoundEngine()->unmute();
    gameboy->getSoundEngine()->refresh();
    otherGameboy->getSoundEngine()->mute();
}

void selectRom() {
    unloadRom();

    loadFileChooserState(&romChooserState);
    const char* extraExtensions[] = {"gbs"};
    char* filename = startFileChooser(extraExtensions, true);
    saveFileChooserState(&romChooserState);

    loadRom(filename);

    if (!biosExists) {
        gameboy->getRomFile()->loadBios("gbc_bios.bin");
    }

    free(filename);

    updateScreens();

    initializeGameboyFirstTime();
}

void initializeGameboyFirstTime() {
    if (sgbBordersEnabled)
        probingForBorder = true; // This will be ignored if starting in sgb mode, or if there is no sgb mode.
    sgbBorderLoaded = false; // Effectively unloads any sgb border

    gb1->init();
    gb2->init();

    if (gbsMode) {
        disableMenuOption("State Slot");
        disableMenuOption("Save State");
        disableMenuOption("Load State");
        disableMenuOption("Delete State");
        disableMenuOption("Suspend");
        disableMenuOption("Exit without saving");
    }
    else {
        enableMenuOption("State Slot");
        enableMenuOption("Save State");
        enableMenuOption("Suspend");
        if (gameboy->checkStateExists(stateNum)) {
            enableMenuOption("Load State");
            enableMenuOption("Delete State");
        }
        else {
            disableMenuOption("Load State");
            disableMenuOption("Delete State");
        }

        if (gameboy->getNumRamBanks() && !autoSavingEnabled)
            enableMenuOption("Exit without saving");
        else
            disableMenuOption("Exit without saving");
    }

    if (biosExists)
        enableMenuOption("GBC Bios");
    else
        disableMenuOption("GBC Bios");
}

int main(int argc, char* argv[])
{
    srand(time(NULL));

    REG_POWERCNT = POWER_ALL & ~(POWER_MATRIX | POWER_3D_CORE); // don't need 3D
    consoleDebugInit(DebugDevice_CONSOLE);
    lcdMainOnTop();

    defaultExceptionHandler();

    time(&rawTime);
    lastRawTime = rawTime;
    timerStart(0, ClockDivider_1024, TIMER_FREQ_1024(1), clockUpdater);

    /* Reset the EZ3in1 if present */
    if (!__dsimode) {
        sysSetCartOwner(BUS_OWNER_ARM9);

        GBA_BUS[0x0000] = 0xF0;
        GBA_BUS[0x1000] = 0xF0;
    }

    fifoSetValue32Handler(FIFO_USER_02, fifoValue32Handler, NULL);

    sharedData = (SharedData*)memUncached(malloc(sizeof(SharedData)));
    dummySharedData = (SharedData*)memUncached(malloc(sizeof(SharedData)));
    sharedData->scalingOn = false;
    sharedData->enableSleepMode = true;
    // It might make more sense to use "fifoSendAddress" here.
    // However there may have been something wrong with it in dsi mode.
    fifoSendValue32(FIFO_USER_03, ((u32)sharedData)&0x00ffffff);

    gb1 = new Gameboy();
    gb2 = new Gameboy();

    setMainGb(1);

    initInput();
    setMenuDefaults();
    readConfigFile();
        lcdMainOnBottom();
    swiWaitForVBlank();
    swiWaitForVBlank();
    // initGFX is called in gameboy->init, but I also call it from here to
    // set up the vblank handler asap.
    initGFX();

    consoleInitialized = false;

    if (argc >= 2) {
        char* filename = argv[1];
        loadRom(filename);
        updateScreens();
        initializeGameboyFirstTime();
    }
    else {
        selectRom();
    }

    for (;;) {
        if (!gb1->isGameboyPaused())
            gb1->runEmul();
        /*
        if (!gb2->isGameboyPaused())
            gb2->runEmul();
            */
        updateVBlank();
    }

    return 0;
}

void updateVBlank() {
    readKeys();
    if (isMenuOn())
        updateMenu();
    else {
        gameboy->gameboyCheckInput();
        if (gbsMode)
            gbsCheckInput();
    }

    nifiCheckInput();

    /*
       if (gameboy->isGameboyPaused()) {
       muteSND();
       while (gameboyPaused) {
       swiWaitForVBlank();
       readKeys();
       gameboyCheckInput();
       updateMenu();
       nifiCheckInput();
       }
       unmuteSND();
       }
       */

    int oldIME = REG_IME;
    REG_IME = 0;
    nifiUpdateInput();
    REG_IME = oldIME;

    if (gbsMode) {
        drawScreen(); // Just because it syncs with vblank...
    }
    else {
        drawScreen();
    }

    if (isConsoleOn() && !isMenuOn() && !consoleDebugOutput && (rawTime > lastRawTime))
    {
        setPrintConsole(menuConsole);
        consoleClear();
        int line=0;
        if (fpsOutput) {
            consoleClear();
            iprintf("FPS: %d\n", gameboy->fps);
            line++;
        }
        gameboy->fps = 0;
        if (timeOutput) {
            for (; line<23-1; line++)
                iprintf("\n");
            char *timeString = ctime(&rawTime);
            for (int i=0;; i++) {
                if (timeString[i] == ':') {
                    timeString += i-2;
                    break;
                }
            }
            char s[50];
            strncpy(s, timeString, 50);
            s[5] = '\0';
            int spaces = 31-strlen(s);
            for (int i=0; i<spaces; i++)
                iprintf(" ");
            iprintf("%s\n", s);
        }
        lastRawTime = rawTime;
    }
}
