

#include "nrf24_menu.h"
#include "config.h"
#include "settings.h"
#include "keyboard.h"
#include "ui.h"
#include "menu.h"
#include <Arduino.h>
#include <M5Unified.h>

uint8_t kitenNrf24CePin   = 33;
uint8_t kitenNrf24CsPin   = 32;
uint8_t kitenNrf24SckPin  = 40;
uint8_t kitenNrf24MisoPin = 39;
uint8_t kitenNrf24MosiPin = 14;

void nrf24Info()
{
    M5.Display.fillScreen(kitenConfig.bgColor);
    M5.Display.setTextSize(FM);
    M5.Display.setTextColor(TFT_RED, kitenConfig.bgColor);
    M5.Display.setTextDatum(top_center);
    M5.Display.drawString("_Disclaimer_", SCREEN_WIDTH / 2, 10);
    M5.Display.setTextDatum(top_left);
    M5.Display.setTextColor(TFT_WHITE, kitenConfig.bgColor);
    M5.Display.setTextSize(FP);
    M5.Display.setCursor(15, 33);

    
    const char *lines[] = {
        "These functions were made",
        "to be used in a controlled",
        "environment for STUDY only.",
        "",
        "DO NOT use these functions",
        "to harm people or companies,",
        "you can go to jail!",
        "",
        NULL
    };
    int y = 33;
    for (int i = 0; lines[i] != NULL; i++) {
        M5.Display.setCursor(15, y);
        M5.Display.println(lines[i]);
        y += LH;
    }
    M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
    M5.Display.setCursor(15, y + LH);
    M5.Display.println("This device is VERY sensible");
    M5.Display.setCursor(15, y + 2 * LH);
    M5.Display.println("to noise, so long wires or");
    M5.Display.setCursor(15, y + 3 * LH);
    M5.Display.println("passing near VCC line can");
    M5.Display.setCursor(15, y + 4 * LH);
    M5.Display.println("make things go wrong.");
    delay(1000);
    
    while (!check(AnyKeyPress) && !check(EscPress) && !check(SelPress)) {
        delay(50);
    }
}

static void nrfNeedsModule(const String &feature)
{
    displayInfo(
        feature + " needs an\n"
        "NRF24L01+ 2.4 GHz radio\n"
        "module wired to the\n"
        "SPI bus:\n"
        "  SCK  = GPIO " + String(kitenNrf24SckPin) + "\n"
        "  MISO = GPIO " + String(kitenNrf24MisoPin) + "\n"
        "  MOSI = GPIO " + String(kitenNrf24MosiPin) + "\n"
        "  CE   = GPIO " + String(kitenNrf24CePin) + "\n"
        "  CS   = GPIO " + String(kitenNrf24CsPin) + "\n\n"
        "Add tmrh20/RF24\n"
        "lib to platformio.ini\n"
        "to enable.",
        true);
}

void nrf24Spectrum()  { nrfNeedsModule("Spectrum");   }
void nrf24MouseJack() { nrfNeedsModule("MouseJack");  }
void nrf24Jammer()    { nrfNeedsModule("NRF Jammer"); }

void nrf24ConfigMenu()
{
    auto pinEntry = [](const String &label, uint8_t &pinVar) {
        String s = keyboard(String(pinVar), 3, label);
        if (s == "\x1B" || s.length() == 0) return;
        int p = s.toInt();
        if (p >= 0 && p <= 48) pinVar = (uint8_t)p;
    };

    std::vector<Option> opts = {
        {"CE  = GPIO "   + String(kitenNrf24CePin),   [&]() { pinEntry("CE GPIO",  kitenNrf24CePin);   }},
        {"CS  = GPIO "   + String(kitenNrf24CsPin),   [&]() { pinEntry("CS GPIO",  kitenNrf24CsPin);   }},
        {"SCK = GPIO "   + String(kitenNrf24SckPin),  [&]() { pinEntry("SCK GPIO", kitenNrf24SckPin);  }},
        {"MISO= GPIO "   + String(kitenNrf24MisoPin), [&]() { pinEntry("MISO GPIO",kitenNrf24MisoPin); }},
        {"MOSI= GPIO "   + String(kitenNrf24MosiPin), [&]() { pinEntry("MOSI GPIO",kitenNrf24MosiPin); }},
        {"Back",         [](){}},
    };
    loopOptions(opts, MENU_TYPE_REGULAR, "NRF24 Config");
}

void nrf24MenuEntry()
{
    std::vector<Option> opts = {
        {"Information", []() { nrf24Info();       }},
        {"Spectrum",    []() { nrf24Spectrum();   }},
        {"MouseJack",   []() { nrf24MouseJack();  }},
        {"NRF Jammer",  []() { nrf24Jammer();     }},
        {"Config pins", []() { nrf24ConfigMenu(); }},
        {"Back",        [](){}},
    };
    loopOptions(opts, MENU_TYPE_SUBMENU, "NRF24");
}
