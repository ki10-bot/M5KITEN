

#include "rfid_menu.h"
#include "rfid_cloner.h"
#include "config.h"
#include "settings.h"
#include "keyboard.h"
#include "ui.h"
#include "menu.h"
#include "sd_helper.h"
#include <Arduino.h>
#include <M5Unified.h>

uint8_t kitenRfidModule = 0;  

static const char *RFID_MODULE_NAMES[] = {
    "None", "PN532-I2C", "PN532-SPI", "RC522-SPI", "M5-RFID2", "PN532-G33"
};

static void rfidNeedsHardware(const String &feature, const String &modules)
{
    displayInfo(
        feature + " needs a\n"
        "wired RFID reader:\n"
        + modules + "\n\n"
        "Wire to the Cardputer\n"
        "Groove port (I2C)\n"
        "or SPI bus. KITEN's\n"
        "PN532 driver (~1k\n"
        "lines) is too large\n"
        "to bundle here.",
        true);
}

void TagOMatic(int mode)
{
    String modeName;
    switch (mode) {
        case TAGOMATIC_READ_MODE:       modeName = "Read tag"; break;
        case TAGOMATIC_SCAN_MODE:       modeName = "Scan tags"; break;
        case TAGOMATIC_LOAD_MODE:       modeName = "Load file"; break;
        case TAGOMATIC_ERASE_MODE:      modeName = "Erase data"; break;
        case TAGOMATIC_WRITE_NDEF_MODE: modeName = "Write NDEF"; break;
        default:                        modeName = "TagOMatic"; break;
    }
    rfidNeedsHardware(modeName, "PN532 (I2C/SPI/UART)\nor RC522 (SPI)");
}

void EMVReader()
{
    rfidNeedsHardware("Read EMV", "PN532 (I2C/SPI/UART)\n");
}

void RFID125()
{
    rfidNeedsHardware("Read 125kHz", "125 kHz RFID module\n(e.g. RDM6300)\n");
}

void Amiibo()
{
    
    displayInfo(
        "Amiibolink uses BLE\n"
        "to talk to an\n"
        "Amiibolink device.\n"
        "KITEN's amiibo.cpp\n"
        "is ~200 lines.\n"
        "Coming in a future\n"
        "KITEN build.",
        true);
}

void Chameleon()
{
    
    displayInfo(
        "Chameleon Ultra/Tiny\n"
        "communicates over\n"
        "BLE. KITEN's\n"
        "chameleon.cpp is\n"
        "~1,000 lines.\n"
        "Coming in a future\n"
        "KITEN build.",
        true);
}

void Pn532ble()
{
    
    displayInfo(
        "PN532 BLE connects\n"
        "to a PN532 v3\n"
        "module over BLE.\n"
        "KITEN's pn532ble.cpp\n"
        "is ~1,500 lines.\n"
        "Coming in a future\n"
        "KITEN build.",
        true);
}

void PN532KillerTools()
{
    rfidNeedsHardware("PN532 UART", "PN532 (UART/I2C/SPI)\n");
}

void PN532_SRIX()
{
    rfidNeedsHardware("SRIX Tool", "PN532 (I2C only)\n");
}

void addMifareKeyMenu()
{
    
    
    displayInfo(
        "MIFARE key manager\n"
        "lets you save known\n"
        "MIFARE Classic keys\n"
        "for tag reads.\n"
        "Coming in a future\n"
        "KITEN build (needs\n"
        "the PN532 driver).",
        true);
}

void rfidConfigMenu()
{
    std::vector<Option> opts;
    
    {
        String label = "RFID Module: ";
        if (kitenRfidModule < sizeof(RFID_MODULE_NAMES)/sizeof(RFID_MODULE_NAMES[0])) {
            label += RFID_MODULE_NAMES[kitenRfidModule];
        } else {
            label += "None";
        }
        opts.push_back({label, []() {
            std::vector<Option> mopts;
            for (int i = 0; i < (int)(sizeof(RFID_MODULE_NAMES)/sizeof(RFID_MODULE_NAMES[0])); i++) {
                String name = RFID_MODULE_NAMES[i];
                mopts.push_back({name, [i]() { kitenRfidModule = (uint8_t)i; }});
            }
            mopts.push_back({"Back", [](){}});
            loopOptions(mopts, MENU_TYPE_REGULAR, "RFID Module");
        }});
    }
    opts.push_back({"Add MIF Key", []() { addMifareKeyMenu(); }});
    opts.push_back({"Back",        [](){}});
    loopOptions(opts, MENU_TYPE_REGULAR, "RFID Config");
}

void rfidMenuEntry()
{
    std::vector<Option> opts = {
        {"Read tag",     []() { TagOMatic(TAGOMATIC_READ_MODE);       }},
        {"Read EMV",     []() { EMVReader();                          }},
        {"Read 125kHz",  []() { RFID125();                            }},
        {"Scan tags",    []() { TagOMatic(TAGOMATIC_SCAN_MODE);       }},
        {"Load file",    []() { TagOMatic(TAGOMATIC_LOAD_MODE);       }},
        {"Erase data",   []() { TagOMatic(TAGOMATIC_ERASE_MODE);      }},
        {"Write NDEF",   []() { TagOMatic(TAGOMATIC_WRITE_NDEF_MODE); }},
        {"Amiibolink",   []() { Amiibo();                             }},
        {"Chameleon",    []() { Chameleon();                          }},
        {"PN532 BLE",    []() { Pn532ble();                           }},
        {"PN532 UART",   []() { PN532KillerTools();                   }},
        {"SRIX Tool",    []() { PN532_SRIX();                         }},
        {"RFID Clone",   []() { rfidClonerMenuEntry();               }},
        {"Config",       []() { rfidConfigMenu();                     }},
    };

    String txt = "RFID";
    if (kitenRfidModule < sizeof(RFID_MODULE_NAMES)/sizeof(RFID_MODULE_NAMES[0])) {
        String name = RFID_MODULE_NAMES[kitenRfidModule];
        if (name != "None") txt += " (" + name + ")";
    }
    loopOptions(opts, MENU_TYPE_SUBMENU, txt.c_str());
}
