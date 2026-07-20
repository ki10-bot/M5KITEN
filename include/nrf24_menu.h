

#ifndef __KITEN_NRF24_MENU_H__
#define __KITEN_NRF24_MENU_H__

#include <Arduino.h>

void nrf24MenuEntry();

void nrf24Info();
void nrf24Spectrum();
void nrf24MouseJack();
void nrf24Jammer();
void nrf24ConfigMenu();

extern uint8_t kitenNrf24CePin;     
extern uint8_t kitenNrf24CsPin;     
extern uint8_t kitenNrf24SckPin;    
extern uint8_t kitenNrf24MisoPin;   
extern uint8_t kitenNrf24MosiPin;   

#endif
