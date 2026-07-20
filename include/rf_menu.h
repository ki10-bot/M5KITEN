

#ifndef __KITEN_RF_MENU_H__
#define __KITEN_RF_MENU_H__

#include <Arduino.h>

void rfMenuEntry();

void RFScan();
void rf_raw_record();
void sendCustomRF();
void rf_spectrum();
void rf_CC1101_rssi();
void rf_SquareWave();
void rf_waterfall();
void rf_listen();
void rf_bruteforce();
void RFJammer(bool jam = true);
void rfConfigMenu();

extern uint8_t kitenRfTxPin;
extern uint8_t kitenRfRxPin;
extern uint8_t kitenRfModule;   
extern uint32_t kitenRfFreq;    

#endif
