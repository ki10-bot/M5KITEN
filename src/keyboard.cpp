#include "keyboard.h"
#include "ui.h"
#include "config.h"
#include <Arduino.h>

volatile bool PrevPress       = false;
volatile bool NextPress       = false;
volatile bool UpPress         = false;
volatile bool DownPress       = false;
volatile bool SelPress        = false;
volatile bool EscPress        = false;
volatile bool AnyKeyPress     = false;

void pollKeyboard()
{
    M5Cardputer.update();

    auto &kb = M5Cardputer.Keyboard;

    
    if (!kb.isChange()) {
        if (kb.isPressed()) AnyKeyPress = true;
        return;
    }

    
    if (!kb.isPressed()) return;

    AnyKeyPress = true;

    
    auto st = kb.keysState();

    
    if (st.enter) SelPress = true;
    if (st.del)   EscPress = true;   

    
    for (char c : st.word) {
        switch (c) {
            case ';':  PrevPress  = true;  break;   
            case '\'': NextPress  = true;  break;   
            case ',':  UpPress    = true;  break;   
            case '.':  DownPress  = true;  break;   
            default:   break;
        }
    }
}

bool check(volatile bool &flag)
{
    bool v = flag;
    flag = false;
    return v;
}

void waitAllKeysReleased()
{
    delay(30);
    while (M5Cardputer.Keyboard.isPressed()) {
        M5Cardputer.update();
        delay(10);
    }
    
    
    PrevPress   = false;
    NextPress   = false;
    UpPress     = false;
    DownPress   = false;
    SelPress    = false;
    EscPress    = false;
    AnyKeyPress = false;
}

String keyboard(String mytext, int maxSize, String msg, bool mask_input)
{
    if (mytext.length() > maxSize) mytext = mytext.substring(0, maxSize);

    delay(200);  

    for (;;) {
        
        M5.Display.fillScreen(kitenConfig.bgColor);
        
        M5.Display.drawFastHLine(0, 25, SCREEN_WIDTH, kitenConfig.priColor);

        
        M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
        M5.Display.setTextSize(FP);
        M5.Display.setTextDatum(top_left);
        M5.Display.setCursor(4, 4);
        M5.Display.print(msg);

        
        int boxX = 4, boxY = 32, boxW = SCREEN_WIDTH - 8, boxH = 28;
        M5.Display.fillRect(boxX, boxY, boxW, boxH, kitenConfig.bgColor);
        M5.Display.drawRect(boxX, boxY, boxW, boxH, kitenConfig.priColor);

        
        String displayText = mask_input ? String("") : mytext;
        if (mask_input) {
            for (unsigned i = 0; i < mytext.length(); i++) displayText += "*";
        }
        M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
        M5.Display.setTextSize(FM);
        M5.Display.setTextDatum(top_left);
        
        int maxChars = (boxW - 8) / (FM * LW);
        String shown = displayText;
        if ((int)shown.length() > maxChars) {
            shown = shown.substring(shown.length() - maxChars);
        }
        M5.Display.drawString(shown, boxX + 4, boxY + 6);

        
        unsigned long now = millis();
        if ((now / 500) % 2 == 0) {
            int cx = boxX + 4 + shown.length() * FM * LW;
            int cy = boxY + 6;
            if (cx < boxX + boxW - 4) {
                M5.Display.fillRect(cx, cy, FM * LW, FM * LH, kitenConfig.priColor);
            }
        }

        
        M5.Display.setTextColor(kitenConfig.secColor, kitenConfig.bgColor);
        M5.Display.setTextSize(FP);
        M5.Display.setTextDatum(top_left);
        M5.Display.drawString("Enter: OK    Backspace: Del/Bksp", 4, SCREEN_HEIGHT - 12);
        M5.Display.drawString(String(mytext.length()) + "/" + String(maxSize),
                              SCREEN_WIDTH - 40, SCREEN_HEIGHT - 12);

        M5.Display.setTextDatum(top_left);

        
        M5.update();
        M5Cardputer.update();

        if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
            auto st = M5Cardputer.Keyboard.keysState();

            
            if (st.enter) {
                delay(100);
                waitAllKeysReleased();
                return mytext;
            }

            
            if (st.del) {
                if (mytext.length() > 0) {
                    mytext.remove(mytext.length() - 1);
                } else {
                    
                    delay(100);
                    waitAllKeysReleased();
                    return "\x1B";
                }
            }

            
            for (char c : st.word) {
                if ((int)mytext.length() < maxSize) {
                    
                    if (c >= 0x20 && c < 0x7F) {
                        mytext += c;
                    }
                }
            }

            
            if (st.space) {
                if ((int)mytext.length() < maxSize) {
                    mytext += ' ';
                }
            }

            
            if (st.tab) {
                if ((int)mytext.length() < maxSize) {
                    mytext += '\t';
                }
            }
        }

        delay(20);
    }
}
