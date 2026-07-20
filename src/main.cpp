#include <Arduino.h>
#include <M5Unified.h>
#include <M5Cardputer.h>

#include "config.h"
#include "bootscreen.h"
#include "app.h"
#include "settings.h"
#include "ui.h"

void setup()
{
    Serial.begin(115200);

    auto cfg = M5.config();
    M5.begin(cfg);
    M5Cardputer.begin(cfg, true);

    M5.Display.setRotation(1);          
    M5.Display.setBrightness(200);
    M5.Display.setTextFont(0);
    M5.Display.setTextSize(1);

    
    
    
    
    kitenConfig.begin();

    
    M5.Speaker.setVolume(BOOT_SOUND_VOLUME);
    M5.Speaker.tone(880, 80);

    
    runBootscreen(M5.Display);

    
    initHardware();

    
    
    
    Serial.println("[BOOT] Boot complete. Entering KITEN main menu.");
}

void loop()
{
    M5.update();
    M5Cardputer.update();
    appLoop();
    delay(2);
}
