

#include "fm_menu.h"
#include "config.h"
#include "settings.h"
#include "keyboard.h"
#include "ui.h"
#include "menu.h"
#include <Arduino.h>
#include <M5Unified.h>

static void fmNeedsSi4713(const String &feature)
{
    displayInfo(
        feature + " needs an\n"
        "Adafruit Si4713 FM\n"
        "transmitter breakout\n"
        "(part #1958) wired to\n"
        "the I2C bus:\n"
        "  SDA = GPIO 32\n"
        "  SCL = GPIO 33\n"
        "  RST = GPIO 0\n"
        "  (Cardputer Groove)\n\n"
        "Add Adafruit Si4713\n"
        "lib to platformio.ini\n"
        "to enable.",
        true);
}

void fmBrdcastStd()  { fmNeedsSi4713("Brdcast std");  }
void fmBrdcastRsvd() { fmNeedsSi4713("Brdcast rsvd"); }
void fmBrdcastStop() { fmNeedsSi4713("Brdcast stop"); }
void fmSpectrum()    { fmNeedsSi4713("FM Spectrum");  }
void fmHijackTA()    { fmNeedsSi4713("Hijack TA");    }

void fmMenuEntry()
{
    std::vector<Option> opts = {
        {"Brdcast std",  []() { fmBrdcastStd();  }},
        {"Brdcast rsvd", []() { fmBrdcastRsvd(); }},
        {"Brdcast stop", []() { fmBrdcastStop(); }},
        {"FM Spectrum",  []() { fmSpectrum();    }},
        {"Hijack TA",    []() { fmHijackTA();    }},
        {"Back",         [](){}},
    };
    loopOptions(opts, MENU_TYPE_SUBMENU, "FM");
}
