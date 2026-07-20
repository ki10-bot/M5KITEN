

#include "rf_menu.h"
#include "config.h"
#include "settings.h"
#include "keyboard.h"
#include "ui.h"
#include "menu.h"
#include "sd_helper.h"
#include <Arduino.h>
#include <M5Unified.h>

uint8_t  kitenRfTxPin  = 2;
uint8_t  kitenRfRxPin  = 36;
uint8_t  kitenRfModule = 0;       
uint32_t kitenRfFreq   = 433920000;  

static void rfNeedsCC1101(const String &feature)
{
    displayInfo(
        feature + " needs a\n"
        "CC1101 Sub-GHz radio\n"
        "module wired to the\n"
        "SPI bus:\n"
        "  SCK  = GPIO 40\n"
        "  MISO = GPIO 39\n"
        "  MOSI = GPIO 14\n"
        "  CS   = GPIO 12\n\n"
        "Add ELECHOUSE CC1101\n"
        "lib to platformio.ini\n"
        "to enable.",
        true);
}

void RFScan()                 { rfNeedsCC1101("Scan/copy"); }
void rf_raw_record()          { rfNeedsCC1101("Record RAW"); }
void sendCustomRF()           { rfNeedsCC1101("Custom SubGhz"); }
void rf_spectrum()            { rfNeedsCC1101("Spectrum"); }
void rf_CC1101_rssi()         { rfNeedsCC1101("RSSI Spectrum"); }
void rf_SquareWave()          { rfNeedsCC1101("SquareWave Spec"); }
void rf_waterfall()           { rfNeedsCC1101("Spectogram"); }
void rf_listen()              { rfNeedsCC1101("Listen"); }
void rf_bruteforce()          { rfNeedsCC1101("Bruteforce"); }
void RFJammer(bool jam)       { (void)jam; rfNeedsCC1101("Jammer"); }

void rfConfigMenu()
{
    std::vector<Option> opts = {
        {"RF TX Pin (GPIO " + String(kitenRfTxPin) + ")", []() {
            String s = keyboard(String(kitenRfTxPin), 3, "RF TX GPIO");
            if (s == "\x1B" || s.length() == 0) return;
            int pin = s.toInt();
            if (pin >= 0 && pin <= 48) kitenRfTxPin = (uint8_t)pin;
        }},
        {"RF RX Pin (GPIO " + String(kitenRfRxPin) + ")", []() {
            String s = keyboard(String(kitenRfRxPin), 3, "RF RX GPIO");
            if (s == "\x1B" || s.length() == 0) return;
            int pin = s.toInt();
            if (pin >= 0 && pin <= 48) kitenRfRxPin = (uint8_t)pin;
        }},
        {"RF Module: " + String(kitenRfModule == 1 ? "CC1101" : "None"), []() {
            std::vector<Option> mopts = {
                {"None",   []() { kitenRfModule = 0; }},
                {"CC1101", []() { kitenRfModule = 1; }},
                {"Back",   [](){}},
            };
            loopOptions(mopts, MENU_TYPE_REGULAR, "RF Module");
        }},
        {"RF Freq: " + String(kitenRfFreq / 1000) + " kHz", []() {
            String s = keyboard(String(kitenRfFreq / 1000), 8, "Freq (kHz)");
            if (s == "\x1B" || s.length() == 0) return;
            long f = s.toInt();
            if (f >= 300000 && f <= 928000) kitenRfFreq = (uint32_t)f * 1000;
        }},
        {"Back", [](){}},
    };
    loopOptions(opts, MENU_TYPE_REGULAR, "RF Config");
}

void rfMenuEntry()
{
    std::vector<Option> opts = {
        {"Scan/copy",       []() { RFScan();          }},
        {"Record RAW",      []() { rf_raw_record();   }},
        {"Custom SubGhz",   []() { sendCustomRF();    }},
        {"Spectrum",        []() { rf_spectrum();     }},
        {"RSSI Spectrum",   []() { rf_CC1101_rssi();  }},
        {"SquareWave Spec", []() { rf_SquareWave();   }},
        {"Spectogram",      []() { rf_waterfall();    }},
        {"Listen",          []() { rf_listen();       }},
        {"Bruteforce",      []() { rf_bruteforce();   }},
        {"Jammer",          []() { RFJammer(true);    }},
        {"Config",          []() { rfConfigMenu();    }},
    };

    String txt = "Radio Frequency";
    if (kitenRfModule == 1) txt += " (CC1101)";
    else txt += " Tx:" + String(kitenRfTxPin) + " Rx:" + String(kitenRfRxPin);
    loopOptions(opts, MENU_TYPE_SUBMENU, txt.c_str());
}
