#include "rfid_cloner.h"
#include "config.h"
#include "settings.h"
#include "keyboard.h"
#include "ui.h"
#include "menu.h"
#include <Arduino.h>
#include <M5Unified.h>
#include <SD.h>
#include <vector>
#include <Adafruit_PN532.h>

#define PN532_IRQ   2
#define PN532_RESET 5

#ifndef PN532_COMMAND_TGINITASTARGET
#define PN532_COMMAND_TGINITASTARGET 0x8C
#endif

class PN532Emu : public Adafruit_PN532 {
public:
    PN532Emu(uint8_t irq, uint8_t reset) : Adafruit_PN532(irq, reset) {}

    
    
    bool startCardEmulation(const uint8_t* uid, uint8_t uidLen,
                            const uint8_t* atqa, uint8_t sak) {
        
        uint8_t cmd[30];
        memset(cmd, 0, sizeof(cmd));
        cmd[0] = PN532_COMMAND_TGINITASTARGET;
        cmd[1] = 0x00;                     

        uint8_t pos = 2;
        cmd[pos++] = uidLen;               
        memcpy(cmd + pos, uid, uidLen);    
        pos += uidLen;
        cmd[pos++] = atqa[0];              
        cmd[pos++] = atqa[1];              
        cmd[pos++] = sak;                  

        
        
        return sendCommandCheckAck(cmd, pos);
    }
};

static PN532Emu nfc(PN532_IRQ, PN532_RESET);

struct StoredCard {
    char     name[16];
    uint8_t  uid[10];
    uint8_t  uidLen;
    uint8_t  atqa[2];
    uint8_t  sak;
    uint8_t  cardType;   
};
static StoredCard currentCard;
static bool cardLoaded = false;

static String uid2str(const uint8_t* uid, uint8_t len) {
    String s = "";
    for (uint8_t i = 0; i < len; i++) {
        if (i > 0) s += ":";
        char buf[3];
        snprintf(buf, sizeof(buf), "%02X", uid[i]);
        s += buf;
    }
    return s;
}

static const char* cardTypeName(uint8_t type) {
    switch (type) {
        case 0: return "Mifare Classic";
        case 1: return "Mifare Ultralight";
        case 2: return "NTAG";
        default: return "Unknown";
    }
}

static bool initPN532() {
    if (!nfc.begin()) {
        displayError("PN532 nicht gefunden!", true);
        return false;
    }
    nfc.SAMConfig();
    return true;
}

static bool readCard() {
    if (!initPN532()) return false;

    uint8_t uid[10];
    uint8_t uidLen;

    if (!nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLen, 2000)) {
        displayWarning("Keine Karte erkannt", true);
        return false;
    }

    uint8_t atqa[2] = {0x00, 0x04};
    uint8_t sak = 0x08;

    memcpy(currentCard.uid, uid, uidLen);
    currentCard.uidLen = uidLen;
    currentCard.atqa[0] = atqa[0];
    currentCard.atqa[1] = atqa[1];
    currentCard.sak = sak;

    if (uidLen == 4)      currentCard.cardType = 0;
    else if (uidLen == 7) currentCard.cardType = 1;
    else                  currentCard.cardType = 3;

    cardLoaded = true;
    strcpy(currentCard.name, "ReadCard");

    String msg = "UID: " + uid2str(currentCard.uid, currentCard.uidLen) + "\n";
    msg += "Typ: " + String(cardTypeName(currentCard.cardType)) + "\n";
    msg += "ATQA: " + String(currentCard.atqa[0], HEX) + " " + String(currentCard.atqa[1], HEX) + "\n";
    msg += "SAK: " + String(currentCard.sak, HEX);
    displayInfo(msg, true);
    return true;
}

static bool saveCardToSD(const char* name) {
    if (!cardLoaded) {
        displayError("Keine Karte geladen", true);
        return false;
    }
    if (!SD.begin(4, SPI, 4000000)) {
        displayError("SD-Karte nicht bereit", true);
        return false;
    }
    if (!SD.exists("/rfid_cloner")) SD.mkdir("/rfid_cloner");

    String path = "/rfid_cloner/";
    path += name;
    path += ".nfc";
    File f = SD.open(path, FILE_WRITE);
    if (!f) {
        SD.end();
        displayError("Datei kann nicht erstellt werden", true);
        return false;
    }
    f.write((uint8_t*)&currentCard, sizeof(StoredCard));
    f.close();
    SD.end();
    strcpy(currentCard.name, name);
    displaySuccess("Karte gespeichert.");
    return true;
}

static bool loadCardFromSD(const char* filename) {
    if (!SD.begin(4, SPI, 4000000)) {
        displayError("SD-Karte nicht bereit", true);
        return false;
    }
    String path = "/rfid_cloner/";
    path += filename;
    File f = SD.open(path, FILE_READ);
    if (!f) {
        SD.end();
        displayError("Datei nicht gefunden", true);
        return false;
    }
    f.read((uint8_t*)&currentCard, sizeof(StoredCard));
    f.close();
    SD.end();
    cardLoaded = true;
    displayInfo("Geladen: " + String(currentCard.name) + "\nUID: " + uid2str(currentCard.uid, currentCard.uidLen), true);
    return true;
}

static void listSavedCards() {
    if (!SD.begin(4, SPI, 4000000)) {
        displayError("SD-Karte nicht bereit", true);
        return;
    }
    if (!SD.exists("/rfid_cloner")) {
        SD.end();
        displayWarning("Keine gespeicherten Karten", true);
        return;
    }
    File dir = SD.open("/rfid_cloner");
    if (!dir || !dir.isDirectory()) {
        SD.end();
        displayWarning("Ordner nicht lesbar", true);
        return;
    }

    std::vector<String> files;
    File f = dir.openNextFile();
    while (f) {
        if (!f.isDirectory()) files.push_back(String(f.name()));
        f = dir.openNextFile();
    }
    dir.close();
    SD.end();

    if (files.empty()) {
        displayWarning("Keine Karten gefunden", true);
        return;
    }

    std::vector<Option> opts;
    for (auto &name : files) {
        opts.push_back({name, [name]() {
            loadCardFromSD(name.c_str());
        }});
    }
    addBackItem(opts);
    loopOptions(opts, MENU_TYPE_REGULAR, "Karte laden");
}

static void emulateCard() {
    if (!cardLoaded) {
        displayError("Keine Karte geladen", true);
        return;
    }
    if (!initPN532()) return;

    
    if (!nfc.startCardEmulation(currentCard.uid, currentCard.uidLen,
                                currentCard.atqa, currentCard.sak)) {
        displayError("Emulation fehlgeschlagen", true);
        return;
    }

    M5.Display.fillScreen(kitenConfig.bgColor);
    drawMainBorder(false);
    M5.Display.setTextSize(FM);
    M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
    M5.Display.setCursor(BORDER_PAD_X, BORDER_PAD_Y);
    M5.Display.print("EMULIERE KARTE");
    M5.Display.drawFastHLine(BORDER_PAD_X, BORDER_PAD_Y + LH*FM + 2,
                             SCREEN_WIDTH - 2*BORDER_PAD_X, kitenConfig.secColor);
    int y = BORDER_PAD_Y + LH*FM + 6;
    M5.Display.setTextSize(FP);
    M5.Display.setTextColor(TFT_WHITE, kitenConfig.bgColor);
    M5.Display.setCursor(BORDER_PAD_X, y); y+=LH*FP+2;
    M5.Display.print("UID: " + uid2str(currentCard.uid, currentCard.uidLen));
    M5.Display.setCursor(BORDER_PAD_X, y); y+=LH*FP+2;
    M5.Display.print("Typ: " + String(cardTypeName(currentCard.cardType)));
    M5.Display.setTextColor(TFT_RED, kitenConfig.bgColor);
    M5.Display.setCursor(BORDER_PAD_X, SCREEN_HEIGHT - LH*FP - 4);
    M5.Display.print("[ESC] Beenden");

    
    while (true) {
        M5.update();
        pollKeyboard();
        if (check(EscPress)) {
            uiBeep(440, 30);
            waitAllKeysReleased();
            break;
        }
        delay(10);
    }

    
    nfc.begin();
    nfc.SAMConfig();
    displayInfo("Emulation beendet.", true);
}

void rfidClonerMenuEntry() {
    if (!initPN532()) return;

    int sel = 0;
    std::vector<Option> opts;
    for (;;) {
        opts.clear();
        opts.push_back({"Read Card", []() { readCard(); }});
        opts.push_back({"Save to SD", []() {
            if (!cardLoaded) {
                displayWarning("Zuerst Karte lesen oder laden", true);
                return;
            }
            String name = keyboard(currentCard.name, 15, "Name:");
            if (name == "\x1B" || name.length() == 0) return;
            saveCardToSD(name.c_str());
        }});
        opts.push_back({"Load from SD", []() { listSavedCards(); }});
        opts.push_back({"Emulate", []() { emulateCard(); }});
        opts.push_back({"Delete Card", []() {
            if (!SD.begin(4, SPI, 4000000)) {
                displayError("SD nicht bereit", true);
                return;
            }
            if (!SD.exists("/rfid_cloner")) {
                SD.end();
                displayWarning("Kein Ordner", true);
                return;
            }
            std::vector<String> files;
            File dir = SD.open("/rfid_cloner");
            File f = dir.openNextFile();
            while (f) {
                if (!f.isDirectory()) files.push_back(f.name());
                f = dir.openNextFile();
            }
            dir.close();
            SD.end();
            if (files.empty()) {
                displayWarning("Keine Karten", true);
                return;
            }
            std::vector<Option> delOpts;
            for (auto &n : files) {
                delOpts.push_back({n, [n]() {
                    SD.begin(4, SPI, 4000000);
                    SD.remove(("/rfid_cloner/" + n).c_str());
                    SD.end();
                    displaySuccess(n + " geloescht");
                }});
            }
            addBackItem(delOpts);
            loopOptions(delOpts, MENU_TYPE_REGULAR, "Karte loeschen");
        }});
        if (cardLoaded) {
            String info = "Aktive Karte: " + String(currentCard.name) + " (" + uid2str(currentCard.uid, currentCard.uidLen) + ")";
            opts.push_back({info, [](){}});
        }
        addBackItem(opts);
        sel = loopOptions(opts, MENU_TYPE_REGULAR, "RFID Cloner");
        if (sel == -1 || sel == (int)opts.size()-1) return;
    }
}