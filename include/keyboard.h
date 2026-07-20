#pragma once
#include <M5Unified.h>
#include <M5Cardputer.h>

extern volatile bool PrevPress;       
extern volatile bool NextPress;       
extern volatile bool UpPress;         
extern volatile bool DownPress;       
extern volatile bool SelPress;        
extern volatile bool EscPress;        
extern volatile bool AnyKeyPress;     

void pollKeyboard();

bool check(volatile bool &flag);

void waitAllKeysReleased();

String keyboard(String mytext, int maxSize = 76, String msg = "Type your message:", bool mask_input = false);
