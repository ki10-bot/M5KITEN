

#ifndef __KITEN_OTHERS_MENU_H__
#define __KITEN_OTHERS_MENU_H__

#include <Arduino.h>

void othersMenuEntry();

void micMenu();          
void badUsbHidMenu();    
void qrcodeCustomMenu(); 

void qrcode_display(String qrcodeUrl);
void pix_qrcode();
void qrcode_menu();
void display_custom_qrcode();
void save_and_display_qrcode();
void remove_custom_qrcode();
void mic_test();         
void mic_record_app();   
void ducky_setup();      
void ducky_keyboard();   
void clicker_setup();    
void u2f_setup();        
void setup_ibutton();    

#endif
