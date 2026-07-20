

#include "others_menu.h"
#include "ipgen_menu.h"
#include "email_osint.h"
#include "config.h"
#include "settings.h"
#include "keyboard.h"
#include "ui.h"
#include "menu.h"
#include "sd_helper.h"
#include <Arduino.h>
#include <M5Unified.h>

static uint16_t crc_ccitt_update(uint16_t crc, uint8_t data)
{
    crc = (uint8_t)(crc >> 8) | (crc << 8);
    crc ^= data;
    crc ^= (uint8_t)(crc & 0xff) >> 4;
    crc ^= crc << 12;
    crc ^= (crc & 0x00ff) << 5;
    return crc;
}

static String calculate_crc(String input)
{
    size_t len = input.length();
    const uint8_t *data = (const uint8_t *)input.c_str();
    uint16_t crc = 0xffff;
    for (size_t i = 0; i < len; i++) crc = crc_ccitt_update(crc, data[i]);
    String crc_str = String(crc, HEX);
    crc_str.toUpperCase();
    while (crc_str.length() < 4) crc_str = "0" + crc_str;
    return crc_str;
}

void qrcode_display(String qrcodeUrl)
{
    M5.Display.fillScreen(kitenConfig.bgColor);

    
    
    int qrSize = SCREEN_HEIGHT - 30;   
    int qrX = (SCREEN_WIDTH - qrSize) / 2;
    int qrY = 28;

    
    M5.Display.setTextSize(FP);
    M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
    M5.Display.setTextDatum(top_center);
    M5.Display.drawString("QR Code", SCREEN_WIDTH / 2, 4);
    M5.Display.setTextDatum(top_left);

    
    M5.Display.setTextSize(FP);
    M5.Display.setTextColor(TFT_WHITE, kitenConfig.bgColor);
    String shortUrl = qrcodeUrl;
    if (shortUrl.length() > 38) shortUrl = shortUrl.substring(0, 37) + "$";
    M5.Display.setCursor(2, qrY + qrSize + 2);
    M5.Display.print(shortUrl);

    
    M5.Display.qrcode(qrcodeUrl.c_str(), qrX, qrY, qrSize, 1);

    delay(300);
    waitAllKeysReleased();
    while (!check(EscPress) && !check(SelPress)) {
        delay(50);
    }
    M5.Display.fillScreen(kitenConfig.bgColor);
}

void display_custom_qrcode()
{
    String message = keyboard("", 100, "QRCode text:");
    if (message == "\x1B" || message.length() == 0) return;
    qrcode_display(message);
}

static String num_keyboard(const String &initial, int maxSize, const String &msg)
{
    
    
    
    return keyboard(initial, maxSize, msg);
}

void pix_qrcode()
{
    String key = keyboard("", 25, "PIX Key:");
    if (key == "\x1B") return;
    String key_length = key.length() >= 10 ? String(key.length()) : "0" + String(key.length());
    String amount = num_keyboard("1000.00", 10, "Int amount:");
    if (amount == "\x1B") return;
    amount = String(amount.toFloat());
    String amount_length = amount.length() >= 10 ? String(amount.length()) : "0" + String(amount.length());

    String data0 = "0014BR.GOV.BCB.PIX01" + key_length + key;
    String pix_code = "00020126" + String(data0.length()) + data0 +
                      "52040000530398654" + amount_length + amount +
                      "5802BR5909KITEN PIX6014Rio de Janeiro62070503***6304";
    String crc = calculate_crc(pix_code);

    qrcode_display(pix_code + crc);
}

struct KitenQrEntry {
    String menuName;
    String content;
};
static std::vector<KitenQrEntry> kitenQrCodes;

void qrcode_menu()
{
    std::vector<Option> opts;

    
    for (const auto &entry : kitenQrCodes) {
        opts.push_back({entry.menuName, [entry]() { qrcode_display(entry.content); }});
    }

    opts.push_back({"PIX",        []() { pix_qrcode();              }});
    opts.push_back({"Custom",     []() { qrcodeCustomMenu();        }});
    opts.push_back({"Main Menu",  [](){}});
    loopOptions(opts, MENU_TYPE_SUBMENU, "QRCodes");
}

void qrcodeCustomMenu()
{
    std::vector<Option> opts = {
        {"Display",      []() { display_custom_qrcode(); }},
        {"Save&Display", []() { save_and_display_qrcode(); }},
        {"Remove",       []() { remove_custom_qrcode();  }},
        {"Back",         [](){}},
    };
    loopOptions(opts, MENU_TYPE_REGULAR, "Custom QR");
}

void save_and_display_qrcode()
{
    String name = keyboard("", 100, "QRCode name:");
    if (name == "\x1B" || name.isEmpty()) {
        if (name.isEmpty()) displayError("Name cannot be empty!", true);
        return;
    }

    
    for (const auto &e : kitenQrCodes) {
        if (e.menuName == name) {
            displayError("Name already exists!", true);
            return;
        }
    }

    String text = keyboard("", 100, "QRCode text:");
    if (text == "\x1B") return;

    kitenQrCodes.push_back({name, text});
    qrcode_display(text);
}

void remove_custom_qrcode()
{
    if (kitenQrCodes.empty()) {
        displayInfo("There is nothing to remove!", true);
        return;
    }
    std::vector<Option> opts;
    for (const auto &entry : kitenQrCodes) {
        opts.push_back({entry.menuName, [entry]() {
            
            for (auto it = kitenQrCodes.begin(); it != kitenQrCodes.end(); ++it) {
                if (it->menuName == entry.menuName) {
                    kitenQrCodes.erase(it);
                    break;
                }
            }
            displaySuccess("Removed: " + entry.menuName, true);
        }});
    }
    opts.push_back({"Back", [](){}});
    loopOptions(opts, MENU_TYPE_REGULAR, "Remove QR");
}

extern void mic_test_impl();        
extern void mic_record_app_impl();  
extern void ducky_setup_impl();     
extern void ducky_keyboard_impl();  
extern void clicker_setup_impl();   
extern void u2f_setup_impl();       
extern void setup_ibutton_impl();   

void mic_test()       { mic_test_impl(); }
void mic_record_app() { mic_record_app_impl(); }
void ducky_setup()    { ducky_setup_impl(); }
void ducky_keyboard() { ducky_keyboard_impl(); }
void clicker_setup()  { clicker_setup_impl(); }
void u2f_setup()      { u2f_setup_impl(); }
void setup_ibutton()  { setup_ibutton_impl(); }

void micMenu()
{
    std::vector<Option> opts = {
        {"Spectrum", []() { mic_test();       }},
        {"Record",   []() { mic_record_app(); }},
        {"Back",     [](){}},
    };
    loopOptions(opts, MENU_TYPE_SUBMENU, "Microphone");
}

void badUsbHidMenu()
{
    std::vector<Option> opts = {
        {"BadUSB",       []() { ducky_setup();    }},
        {"USB Keyboard", []() { ducky_keyboard(); }},
        {"USB Clicker",  []() { clicker_setup();  }},
        {"USB U2F",      []() { u2f_setup();      }},
        {"Back",         [](){}},
    };
    loopOptions(opts, MENU_TYPE_SUBMENU, "BadUSB & HID");
}

void othersMenuEntry()
{
    std::vector<Option> opts = {
        {"QRCodes",      []() { qrcode_menu();       }},
{"Microphone",   []() { micMenu();           }},  
        {"BadUSB & HID", []() { badUsbHidMenu();     }},
        {"iButton",      []() { setup_ibutton();     }},
        {"IP Gen",       []() { ipgenMenuEntry();    }},
        {"EmailOSINT",   []() { emailOsintMenuEntry(); }},
        {"Back",         [](){}},
    };
    loopOptions(opts, MENU_TYPE_SUBMENU, "Others");
}
