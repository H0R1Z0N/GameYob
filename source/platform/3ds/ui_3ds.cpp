#ifdef BACKEND_3DS

#include <cstring>
#include <cstdarg>
#include <queue>

#include <3ds.h>

#include "platform/common/menu/menu.h"
#include "platform/common/config.h"
#include "platform/ui.h"

static gfxScreen_t currConsole;

static PrintConsole* topConsole;
static PrintConsole* bottomConsole;

static TextColor textColor;
static bool highlighted;

static std::queue<UIKey> keyQueue;

void uiInit() {
    topConsole = (PrintConsole*) calloc(1, sizeof(PrintConsole));
    consoleInit(GFX_TOP, topConsole);

    bottomConsole = (PrintConsole*) calloc(1, sizeof(PrintConsole));
    consoleInit(GFX_BOTTOM, bottomConsole);

    consoleDebugInit(debugDevice_SVC);

    gfxSetScreenFormat(GFX_TOP, GSP_BGR8_OES);
    gfxSetDoubleBuffering(GFX_TOP, true);

    consoleSelect(bottomConsole);
    currConsole = GFX_BOTTOM;
}

void uiCleanup() {
    if(topConsole != nullptr) {
        free(topConsole);
        topConsole = nullptr;
    }

    if(bottomConsole != nullptr) {
        free(bottomConsole);
        bottomConsole = nullptr;
    }
}

void uiGetSize(u32* width, u32* height) {
    PrintConsole* console = configGetMultiChoice(GROUP_GAMEYOB, GAMEYOB_GAME_SCREEN) == GAME_SCREEN_BOTTOM ? topConsole : bottomConsole;

    if(width != nullptr) {
        *width = (u32) console->consoleWidth;
    }

    if(height != nullptr) {
        *height = (u32) console->consoleHeight;
    }
}

void uiClear() {
    gfxScreen_t screen = configGetMultiChoice(GROUP_GAMEYOB, GAMEYOB_GAME_SCREEN) == GAME_SCREEN_BOTTOM ? GFX_TOP : GFX_BOTTOM;
    if(currConsole != screen) {
        gfxSetScreenFormat(currConsole, GSP_BGR8_OES);
        gfxSetDoubleBuffering(currConsole, true);

        for(u32 i = 0; i < 2; i++) {
            u16 width = 0;
            u16 height = 0;

            memset(gfxGetFramebuffer(GFX_TOP, GFX_LEFT, &width, &height), 0, (size_t) (width * height * 3));
            memset(gfxGetFramebuffer(GFX_TOP, GFX_RIGHT, &width, &height), 0, (size_t) (width * height * 3));
            memset(gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, &width, &height), 0, (size_t) (width * height * 3));

            gfxSwapBuffers();
        }

        currConsole = screen;

        gfxSetScreenFormat(screen, GSP_RGB565_OES);
        gfxSetDoubleBuffering(screen, false);

        gfxSwapBuffers();
        gspWaitForVBlank();

        PrintConsole* console = screen == GFX_TOP ? topConsole : bottomConsole;
        console->frameBuffer = (u16*) gfxGetFramebuffer(screen, GFX_LEFT, nullptr, nullptr);
        consoleSelect(console);
    }

    consoleClear();
}

static void uiUpdateAttr() {
    if(highlighted) {
        printf("\x1b[0m\x1b[47m");
        switch(textColor) {
            case TEXT_COLOR_NONE:
                printf("\x1b[30m");
                break;
            case TEXT_COLOR_YELLOW:
                printf("\x1b[33m");
                break;
            case TEXT_COLOR_RED:
                printf("\x1b[31m");
                break;
            case TEXT_COLOR_GREEN:
                printf("\x1b[32m");
                break;
            case TEXT_COLOR_PURPLE:
                printf("\x1b[35m");
                break;
            case TEXT_COLOR_GRAY:
                printf("\x1b[2m\x1b[37m");
                break;
            case TEXT_COLOR_WHITE:
                printf("\x1b[30m");
                break;
        }
    } else {
        printf("\x1b[0m\x1b[40m");
        switch(textColor) {
            case TEXT_COLOR_NONE:
                printf("\x1b[37m");
                break;
            case TEXT_COLOR_YELLOW:
                printf("\x1b[1m\x1b[33m");
                break;
            case TEXT_COLOR_RED:
                printf("\x1b[1m\x1b[31m");
                break;
            case TEXT_COLOR_GREEN:
                printf("\x1b[1m\x1b[32m");
                break;
            case TEXT_COLOR_PURPLE:
                printf("\x1b[1m\x1b[35m");
                break;
            case TEXT_COLOR_GRAY:
                printf("\x1b[2m\x1b[37m");
                break;
            case TEXT_COLOR_WHITE:
                printf("\x1b[37m");
                break;
        }
    }
}

void uiSetLineHighlighted(bool highlight) {
    highlighted = highlight;
    uiUpdateAttr();
}

void uiSetTextColor(TextColor color) {
    textColor = color;
    uiUpdateAttr();
}

void uiAdvanceCursor(u32 n) {
    printf("\x1b[%" PRIu32 "C", n);
}

void uiSetLine(u32 n) {
    printf("\x1b[%" PRIu32 ";1H", n);
}

void uiPrint(const char* str, ...) {
    va_list list;
    va_start(list, str);
    uiPrintv(str, list);
    va_end(list);
}

void uiPrintv(const char* str, va_list list) {
    vprintf(str, list);
}

void uiFlush() {
    gfxFlushBuffers();
}

void uiPushInput(UIKey key) {
    keyQueue.push(key);
}

UIKey uiReadKey() {
    if(!keyQueue.empty()) {
        UIKey key = keyQueue.front();
        keyQueue.pop();
        return key;
    }

    return UI_KEY_NONE;
}

bool uiIsStringInputSupported() {
    return true;
}

bool uiInputString(std::string& out, size_t maxLength, const std::string& hint) {
    char* buf = new char[maxLength];

    SwkbdState swkbd;
    swkbdInit(&swkbd, SWKBD_TYPE_NORMAL, 2, maxLength < 65000 ? maxLength : 65000);
    swkbdSetHintText(&swkbd, hint.c_str());
    swkbdSetValidation(&swkbd, SWKBD_NOTEMPTY_NOTBLANK, 0, 0);
    SwkbdButton button = swkbdInputText(&swkbd, buf, maxLength);

    out = std::string(buf);
    delete[] buf;

    return button == SWKBD_BUTTON_CONFIRM;
}

#endif
