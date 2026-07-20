#include "app.h"
#include "config.h"
#include "settings.h"
#include "keyboard.h"
#include "ui.h"
#include "menu.h"
#include "clock_app.h"
#include "config_menu.h"
#include "usb_tools_menu.h"

#include "others_menu.h"
#include <Arduino.h>
#include "wifi_menu.h"
#include "ble_menu.h"

#include "ir_menu.h"
#include "rf_menu.h"
#include "rfid_menu.h"
#include "nrf24_menu.h"

#include "fm_menu.h"
#include "scripts_menu.h"
#include "python_menu.h"

#include <Arduino.h>
#include <M5Unified.h>

void initHardware()
{
    Serial.println("[HW] Initialising settings…");
    kitenConfig.begin();

    Serial.println("[HW] Applying UI settings…");
    initUI();

    
    if (kitenConfig.wifiAtStartup) {
        Serial.println("[HW] WiFi Startup enabled — attempting connection…");
        WiFi.mode(WIFI_STA);
        WiFi.begin();
        
        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start < 5000) {
            delay(100);
        }
        if (WiFi.status() == WL_CONNECTED) {
            wifiConnected = true;
            wifiIP = WiFi.localIP().toString();
            updateClockTimezone();
            Serial.println("[HW] WiFi connected: " + wifiIP);
        } else {
            Serial.println("[HW] WiFi startup failed — no stored credentials or unavailable");
        }
    }

    Serial.println("[HW] Hardware init complete.");
}

void runMainMenu()
{
    for (;;) {
        std::vector<Option> opts = {
            {"Clock",     [](){ clockMenuEntry();    }},
            {"WiFi",      [](){ wifiMenuEntry();     }},
            {"BLE",       [](){ bleMenuEntry();      }},
            {"IR",        [](){ irMenuEntry();       }},
            {"RF",        [](){ rfMenuEntry();       }},
            {"RFID",      [](){ rfidMenuEntry();     }},
            {"NRF24",     [](){ nrf24MenuEntry();    }},

            {"FM",        [](){ fmMenuEntry();       }},
            {"Scripts",   [](){ scriptsMenuEntry();  }},
            {"Python",    [](){ pythonMenuEntry();   }},
            {"USB Tools", [](){ usbToolsMenuEntry(); }},
            {"Others",    [](){ othersMenuEntry();   }},

            
            {"Config",    [](){ configMenuEntry();   }},
        };
        
        
        loopOptions(opts, MENU_TYPE_MAIN, "KITEN");
    }
}

void appLoop()
{
    pollKeyboard();
    runMainMenu();
}
