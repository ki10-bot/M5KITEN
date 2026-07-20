

#ifndef __KITEN_RFID_MENU_H__
#define __KITEN_RFID_MENU_H__

#include <Arduino.h>

void rfidMenuEntry();

void TagOMatic(int mode = 0);
void EMVReader();
void RFID125();
void Amiibo();
void Chameleon();
void Pn532ble();
void PN532KillerTools();
void PN532_SRIX();
void rfidConfigMenu();
void addMifareKeyMenu();

enum TagOMaticMode {
    TAGOMATIC_READ_MODE       = 0,
    TAGOMATIC_SCAN_MODE       = 1,
    TAGOMATIC_LOAD_MODE       = 2,
    TAGOMATIC_ERASE_MODE      = 3,
    TAGOMATIC_WRITE_NDEF_MODE = 4,
};

extern uint8_t kitenRfidModule;

#endif
