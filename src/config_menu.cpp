#include "config_menu.h"
#include "config.h"
#include "settings.h"
#include "keyboard.h"
#include "ui.h"
#include "menu.h"
#include "clock_app.h"
#include <Arduino.h>
#include <M5Unified.h>
#include <esp_system.h>

static void displayUIMenu();
static void audioMenu();
static void systemMenu();
static void powerMenu();
static void setBrightnessMenu();
static void setDimmerTimeMenu();
static void setRotationMenu();
static void setThemeMenu();
static void setCustomColorMenu();

void configMenuEntry()
{
    for (;;) {
        std::vector<Option> opts = {
            {"Display & UI",  [](){ displayUIMenu(); }},
            {"Audio Config",  [](){ audioMenu();     }},
            {"System Config", [](){ systemMenu();    }},
            {"Power",         [](){ powerMenu();     }},
            {"About",         [](){ showDeviceInfo();}},
            {"Main Menu",     [](){}},
        };

        int sel = loopOptions(opts, MENU_TYPE_SUBMENU, "Config");
        if (sel == -1 || sel == (int)opts.size() - 1) return;
    }
}

static void displayUIMenu()
{
    for (;;) {
        std::vector<Option> opts = {
            {"Brightness",  [](){ setBrightnessMenu();   }},
            {"Dim Time",    [](){ setDimmerTimeMenu();   }},
            {"Orientation", [](){ setRotationMenu();    }},
            {"Theme",       [](){ setThemeMenu();        }},
            {"Back",        [](){}},
        };
        int sel = loopOptions(opts, MENU_TYPE_SUBMENU, "Display & UI");
        if (sel == -1 || sel == (int)opts.size() - 1) return;
    }
}

static void setBrightnessMenu()
{
    struct Entry { const char *label; int value; };
    static const Entry entries[] = {
        {"100%", 100},
        {"75 %",  75},
        {"50 %",  50},
        {"25 %",  25},
        {" 1 %",   1},
    };
    static const int N = sizeof(entries) / sizeof(entries[0]);

    std::vector<Option> opts;
    for (int i = 0; i < N; i++) {
        bool isCur = (kitenConfig.bright == entries[i].value);
        opts.push_back({entries[i].label, [i](){
            kitenConfig.setBright(entries[i].value);
            applyBrightness(entries[i].value);
        }, isCur});
    }
    opts.push_back({"Back", [](){}});
    loopOptions(opts, MENU_TYPE_REGULAR, "Brightness");
}

static void setDimmerTimeMenu()
{
    struct Entry { const char *label; int value; };
    static const Entry entries[] = {
        {"10s",      10},
        {"20s",      20},
        {"30s",      30},
        {"60s",      60},
        {"Disabled",  0},
    };
    static const int N = sizeof(entries) / sizeof(entries[0]);

    std::vector<Option> opts;
    for (int i = 0; i < N; i++) {
        bool isCur = (kitenConfig.dimmerSet == entries[i].value);
        opts.push_back({entries[i].label, [i](){
            kitenConfig.setDimmerSet(entries[i].value);
        }, isCur});
    }
    opts.push_back({"Back", [](){}});
    loopOptions(opts, MENU_TYPE_REGULAR, "Dim Time");
}

static void setRotationMenu()
{
    struct Entry { const char *label; int value; };
    static const Entry entries[] = {
        {"Default",        1},   
        {"Landscape (180)", 3},  
    };
    static const int N = sizeof(entries) / sizeof(entries[0]);

    std::vector<Option> opts;
    for (int i = 0; i < N; i++) {
        bool isCur = (kitenConfig.rotation == entries[i].value);
        opts.push_back({entries[i].label, [i](){
            kitenConfig.setRotation(entries[i].value);
            M5.Display.setRotation(entries[i].value);
            delay(20);
            M5.Display.setRotation(entries[i].value);  
        }, isCur});
    }
    opts.push_back({"Back", [](){}});
    loopOptions(opts, MENU_TYPE_REGULAR, "Orientation");
}

static void setThemeMenu()
{
    for (;;) {
        std::vector<Option> opts;
        for (int i = 0; i < THEME_COUNT; i++) {
            String name = THEME_PRESETS[i].name;
            if (i == THEME_CUSTOM_ID && kitenConfig.themeId == i) {
                name += " (edit)";
            }
            bool isCur = (kitenConfig.themeId == i);
            opts.push_back({name, [i](){
                if (i == THEME_CUSTOM_ID) {
                    
                    setCustomColorMenu();
                } else {
                    kitenConfig.setTheme(i);
                    
                    
                    M5.Display.invertDisplay(kitenConfig.colorInverted);
                }
            }, isCur});
        }
        
        opts.push_back({String("Invert Color: ") + (kitenConfig.colorInverted ? "ON" : "OFF"), [](){
            kitenConfig.setColorInverted(!kitenConfig.colorInverted);
            M5.Display.invertDisplay(kitenConfig.colorInverted);
        }});
        opts.push_back({"Back", [](){}});
        int sel = loopOptions(opts, MENU_TYPE_REGULAR, "Theme");
        if (sel == -1 || sel == (int)opts.size() - 1) return;
    }
}

static uint16_t hsv565(float h, float s, float v)
{
    
    if (h < 0) h = 0;
    if (h >= 360) h = 359.9f;
    if (s < 0) s = 0;
    if (s > 1) s = 1;
    if (v < 0) v = 0;
    if (v > 1) v = 1;

    float c = v * s;
    float x = c * (1 - fabs(fmod(h / 60.0f, 2) - 1));
    float m = v - c;
    float r, g, b;
    if (h < 60)       { r = c; g = x; b = 0; }
    else if (h < 120) { r = x; g = c; b = 0; }
    else if (h < 180) { r = 0; g = c; b = x; }
    else if (h < 240) { r = 0; g = x; b = c; }
    else if (h < 300) { r = x; g = 0; b = c; }
    else              { r = c; g = 0; b = x; }

    uint8_t R = (uint8_t)((r + m) * 255);
    uint8_t G = (uint8_t)((g + m) * 255);
    uint8_t B = (uint8_t)((b + m) * 255);
    return rgb565(R, G, B);
}

static uint16_t pickColorHSV(const char *title, uint16_t initial)
{
    
    
    uint8_t r, g, b;
    rgb565_to_rgb888(initial, r, g, b);
    float rf = r / 255.0f, gf = g / 255.0f, bf = b / 255.0f;
    float maxC = rf > gf ? (rf > bf ? rf : bf) : (gf > bf ? gf : bf);
    float minC = rf < gf ? (rf < bf ? rf : bf) : (gf < bf ? gf : bf);
    float delta = maxC - minC;
    float h = 0;
    if (delta > 0.0001f) {
        if (maxC == rf)      h = 60.0f * fmod(((gf - bf) / delta), 6.0f);
        else if (maxC == gf) h = 60.0f * (((bf - rf) / delta) + 2);
        else                 h = 60.0f * (((rf - gf) / delta) + 4);
        if (h < 0) h += 360;
    }
    float s = (maxC < 0.0001f) ? 0 : (delta / maxC);
    float v = maxC;

    
    int activeSlider = 0;
    bool firstRender = true;
    unsigned long lastDraw = 0;

    drawMainBorder(true);

    for (;;) {
        M5.update();
        pollKeyboard();
        checkPowerSaveTime();

        unsigned long now = millis();
        if (!firstRender && now - lastDraw < 50) {
            delay(5);
            continue;
        }
        lastDraw = now;
        firstRender = false;

        
        
        M5.Display.fillRect(BORDER_PAD_X, 30, SCREEN_WIDTH - 2*BORDER_PAD_X, LH * FP,
                             kitenConfig.bgColor);
        M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
        M5.Display.setTextSize(FP);
        M5.Display.setTextDatum(top_left);
        M5.Display.drawString(String(title), 12, 30);

        
        uint16_t curColor = hsv565(h, s, v);
        int swatchX = 12, swatchY = 45, swatchW = 80, swatchH = 60;
        M5.Display.fillRect(swatchX, swatchY, swatchW, swatchH, curColor);
        M5.Display.drawRect(swatchX - 1, swatchY - 1, swatchW + 2, swatchH + 2,
                             kitenConfig.priColor);

        
        M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
        M5.Display.setTextSize(FP);
        M5.Display.setTextDatum(top_left);
        M5.Display.setCursor(swatchX + swatchW + 8, swatchY + 4);
        M5.Display.printf("R: %3d\n", r);
        M5.Display.setCursor(swatchX + swatchW + 8, swatchY + 4 + LH);
        M5.Display.printf("G: %3d\n", g);
        M5.Display.setCursor(swatchX + swatchW + 8, swatchY + 4 + 2*LH);
        M5.Display.printf("B: %3d\n", b);

        
        const int sliderX = 12, sliderW = SCREEN_WIDTH - 2*12 - 40;
        const int sliderH = 12;
        int sliderY = 115;

        struct SliderSpec { const char *name; float value; float maxVal; uint16_t color; };
        SliderSpec sliders[3] = {
            {"Hue", h, 360, hsv565(h, 1.0f, 1.0f)},
            {"Sat", s * 100, 100, kitenConfig.priColor},
            {"Val", v * 100, 100, kitenConfig.accentColor},
        };

        for (int i = 0; i < 3; i++) {
            int y = sliderY + i * 20;
            
            M5.Display.setTextColor(kitenConfig.secColor, kitenConfig.bgColor);
            M5.Display.setTextSize(FP);
            M5.Display.setTextDatum(top_left);
            M5.Display.drawString(sliders[i].name, sliderX, y);
            
            int trackX = sliderX + 24;
            int trackW = sliderW - 24;
            M5.Display.fillRect(trackX, y + 2, trackW, sliderH, kitenConfig.bgColor);
            M5.Display.drawRect(trackX, y + 2, trackW, sliderH,
                                 kitenConfig.secColor);
            
            int fillW = (int)(trackW * sliders[i].value / sliders[i].maxVal);
            M5.Display.fillRect(trackX, y + 2, fillW, sliderH, sliders[i].color);
            
            if (i == activeSlider) {
                M5.Display.drawRect(trackX - 2, y, trackW + 4, sliderH + 4,
                                     kitenConfig.priColor);
            }
            
            M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
            M5.Display.setTextDatum(top_right);
            char valBuf[8];
            if (i == 0) snprintf(valBuf, sizeof(valBuf), "%d", (int)sliders[i].value);
            else        snprintf(valBuf, sizeof(valBuf), "%d%%", (int)sliders[i].value);
            M5.Display.drawString(valBuf, SCREEN_WIDTH - 12, y);
        }
        M5.Display.setTextDatum(top_left);

        
        M5.Display.setTextColor(kitenConfig.secColor, kitenConfig.bgColor);
        M5.Display.setTextSize(FP);
        M5.Display.setTextDatum(top_center);
        M5.Display.drawString(",:up .:down ;:'slider Enter:OK", SCREEN_WIDTH/2, SCREEN_HEIGHT - 12);
        M5.Display.setTextDatum(top_left);

        
        if (check(UpPress)) {
            activeSlider = (activeSlider - 1 + 3) % 3;
            uiBeep(660, 25);
            markActivity();
        }
        if (check(DownPress)) {
            activeSlider = (activeSlider + 1) % 3;
            uiBeep(660, 25);
            markActivity();
        }
        if (check(PrevPress)) {
            
            switch (activeSlider) {
                case 0: h -= 10; if (h < 0) h += 360; break;
                case 1: s -= 0.05f; if (s < 0) s = 0; break;
                case 2: v -= 0.05f; if (v < 0) v = 0; break;
            }
            uiBeep(440, 25);
            markActivity();
        }
        if (check(NextPress)) {
            
            switch (activeSlider) {
                case 0: h += 10; if (h >= 360) h -= 360; break;
                case 1: s += 0.05f; if (s > 1) s = 1; break;
                case 2: v += 0.05f; if (v > 1) v = 1; break;
            }
            uiBeep(880, 25);
            markActivity();
        }
        if (check(SelPress)) {
            uiBeep(880, 50);
            waitAllKeysReleased();
            
            
            uint16_t finalColor = hsv565(h, s, v);
            return finalColor;
        }
        if (check(EscPress)) {
            uiBeep(440, 30);
            waitAllKeysReleased();
            return initial;  
        }
        if (check(AnyKeyPress)) {
            markActivity();
            wakeUpScreen();
        }

        delay(5);
    }
}

static void setCustomColorMenu()
{
    
    if (kitenConfig.themeId != THEME_CUSTOM_ID) {
        kitenConfig.setTheme(THEME_CUSTOM_ID);
    }

    for (;;) {
        std::vector<Option> opts = {
            {"Primary Color",   [](){
                uint16_t c = pickColorHSV("Primary Color", kitenConfig.priColor);
                kitenConfig.setCustomPriColor(c);
            }},
            {"Background Color", [](){
                uint16_t c = pickColorHSV("Background Color", kitenConfig.bgColor);
                kitenConfig.setCustomBgColor(c);
            }},
            {"Accent Color",    [](){
                uint16_t c = pickColorHSV("Accent Color", kitenConfig.accentColor);
                kitenConfig.setCustomAccent(c);
            }},
            {"Gradient Start",  [](){
                uint16_t c = pickColorHSV("Gradient Start (Boot)", kitenConfig.gradStart);
                kitenConfig.setCustomGradStart(c);
            }},
            {"Gradient End",    [](){
                uint16_t c = pickColorHSV("Gradient End (Boot)", kitenConfig.gradEnd);
                kitenConfig.setCustomGradEnd(c);
            }},
            {"Secondary Color", [](){
                uint16_t c = pickColorHSV("Secondary Color", kitenConfig.secColor);
                kitenConfig.setCustomSecColor(c);
            }},
            {"Back",            [](){}},
        };
        int sel = loopOptions(opts, MENU_TYPE_SUBMENU, "Custom Theme");
        if (sel == -1 || sel == (int)opts.size() - 1) return;
    }
}

static void audioMenu()
{
    for (;;) {
        std::vector<Option> opts = {
            {String("Sound: ") + (kitenConfig.soundEnabled ? "ON" : "OFF"), [](){
                kitenConfig.setSoundEnabled(!kitenConfig.soundEnabled);
                if (kitenConfig.soundEnabled) uiBeep(880, 80);
            }},
            {"Sound Volume", [](){
                std::vector<Option> vOpts;
                for (int v = 10; v <= 100; v += 10) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "%d%%", v);
                    bool isCur = (kitenConfig.soundVolume == v);
                    vOpts.push_back({String(buf), [v](){
                        kitenConfig.setSoundVolume(v);
                        uiBeep(880, 80);   
                    }, isCur});
                }
                vOpts.push_back({"Back", [](){}});
                loopOptions(vOpts, MENU_TYPE_REGULAR, "Sound Volume");
            }},
            {"Back",         [](){}},
        };
        int sel = loopOptions(opts, MENU_TYPE_SUBMENU, "Audio Config");
        if (sel == -1 || sel == (int)opts.size() - 1) return;
    }
}

static void systemMenu()
{
    for (;;) {
        std::vector<Option> opts = {
            {String("InstaBoot: ") + (kitenConfig.instantBoot ? "ON" : "OFF"), [](){
                kitenConfig.setInstantBoot(!kitenConfig.instantBoot);
            }},
            {String("WiFi Startup: ") + (kitenConfig.wifiAtStartup ? "ON" : "OFF"), [](){
                kitenConfig.setWifiAtStartup(!kitenConfig.wifiAtStartup);
            }},
            {String("Sleep Mode: ") + (kitenConfig.sleepDisabled ? "OFF" : "ON"), [](){
                
                
                kitenConfig.setSleepDisabled(!kitenConfig.sleepDisabled);
                if (!kitenConfig.sleepDisabled) {
                    
                    applyBrightness(kitenConfig.bright);
                }
            }},
            {"Clock",                                       [](){ setClock(); }},
            {String("Keyboard Lang: ") + kitenConfig.keyboardLang, [](){
                std::vector<Option> kOpts = {
                    {"QWERTY (English)",  [](){ kitenConfig.setKeyboardLang("QWERTY"); }},
                    {"AZERTY (Francais)", [](){ kitenConfig.setKeyboardLang("AZERTY"); }},
                    {"QWERTZ (Deutsch)",  [](){ kitenConfig.setKeyboardLang("QWERTZ"); }},
                    {"Back",              [](){}},
                };
                loopOptions(kOpts, MENU_TYPE_REGULAR, "Keyboard Lang");
            }},
            {"Back",                                        [](){}},
        };
        int sel = loopOptions(opts, MENU_TYPE_SUBMENU, "System Config");
        if (sel == -1 || sel == (int)opts.size() - 1) return;
    }
}

static void powerMenu()
{
    for (;;) {
        std::vector<Option> opts = {
            {"Sleep",     [](){
                sleepModeOn();
                
                while (!check(AnyKeyPress)) {
                    M5.update();
                    pollKeyboard();
                    delay(20);
                }
                sleepModeOff();
            }},
            {"Restart",   [](){ ESP.restart(); }},
            {"Power Off", [](){
                int8_t c = displayMessage("Power Off Device?", "No", nullptr, "Yes", TFT_RED);
                if (c == 1) {
                    M5.Power.powerOff();
                }
            }},
            {"Back",      [](){}},
        };
        int sel = loopOptions(opts, MENU_TYPE_SUBMENU, "Power Menu");
        if (sel == -1 || sel == (int)opts.size() - 1) return;
    }
}

void showDeviceInfo()
{
    drawMainBorder(true);

    M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
    M5.Display.setTextSize(FP);
    M5.Display.setTextWrap(true);
    M5.Display.setCursor(8, 30);

    M5.Display.println();
    M5.Display.println(String("Firmware: ") + FW_NAME + " v" + FW_VERSION);
    M5.Display.println(String("Author:    ") + FW_AUTHOR);
    M5.Display.println(String("Style:     KITEN-faithful"));
    M5.Display.println();
    M5.Display.println(String("Heap:    ") + ESP.getFreeHeap() / 1024 + " KB free");
    M5.Display.println(String("PSRAM:   ") + ESP.getPsramSize() / 1024 + " KB total");
    M5.Display.println(String("SDK:     ") + ESP.getSdkVersion());
    M5.Display.println();
    M5.Display.println(String("Theme:     ") + THEME_PRESETS[kitenConfig.themeId].name);
    M5.Display.println(String("Rotation:  ") + kitenConfig.rotation);
    M5.Display.println(String("Bright:    ") + kitenConfig.bright + "%");
    M5.Display.println(String("Dim Time:  ") +
                        (kitenConfig.dimmerSet == 0 ? String("Disabled") :
                                                       String(kitenConfig.dimmerSet) + "s"));
    M5.Display.println(String("Sleep:     ") +
                        (kitenConfig.sleepDisabled ? String("OFF (never sleep)")
                                                   : String("ON")));
    M5.Display.println(String("Sound:     ") +
                        (kitenConfig.soundEnabled ? String("ON (") + kitenConfig.soundVolume + "%)"
                                                  : String("OFF")));
    M5.Display.println(String("Timezone:  UTC") +
                        (kitenConfig.tmz >= 0 ? "+" : "") + String(kitenConfig.tmz, 2) +
                        (kitenConfig.dst ? " DST" : ""));
    M5.Display.println(String("Clock:     ") +
                        (kitenConfig.clock24hr ? String("24h") : String("12h")) +
                        (kitenConfig.clockSet ? " (set)" : " (unset)"));
    M5.Display.println(String("Keyboard:  ") + kitenConfig.keyboardLang);
    M5.Display.println();
    M5.Display.println("Press any key to exit");

    M5.Display.setTextWrap(false);

    waitAllKeysReleased();
    while (!check(AnyKeyPress)) {
        M5.update();
        pollKeyboard();
        delay(10);
    }
    waitAllKeysReleased();
}
