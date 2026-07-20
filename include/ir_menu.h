

#ifndef __KITEN_IR_MENU_H__
#define __KITEN_IR_MENU_H__

#include <Arduino.h>

void irMenuEntry();

void startTvBGone();
void otherIRcodes();
void irRead();
void startIrJammer();
void irConfigMenu();

extern uint8_t kitenIrTxPin;
extern uint8_t kitenIrRxPin;
extern uint8_t kitenIrTxRepeats;

#endif
