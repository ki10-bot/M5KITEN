#include "clock_app.h"
#include "config.h"
#include "settings.h"
#include "keyboard.h"
#include "ui.h"
#include "menu.h"
#include <Arduino.h>
#include <M5Unified.h>
#include <time.h>
#include <WiFi.h>

static bool clockExitRequested = false;

static bool parseCompileTime(const char *str, struct tm &out)
{
    static const char *MONTHS[] = {
        "Jan","Feb","Mar","Apr","May","Jun",
        "Jul","Aug","Sep","Oct","Nov","Dec"
    };
    char mon[4] = {0};
    int day, year, hour, min, sec;
    
    
    if (sscanf(str, "%3s %d %d %d:%d:%d",
               mon, &day, &year, &hour, &min, &sec) != 6) {
        return false;
    }
    int monIdx = -1;
    for (int i = 0; i < 12; i++) {
        if (strcmp(mon, MONTHS[i]) == 0) { monIdx = i; break; }
    }
    if (monIdx < 0) return false;

    memset(&out, 0, sizeof(out));
    out.tm_year = year - 1900;
    out.tm_mon  = monIdx;
    out.tm_mday = day;
    out.tm_hour = hour;
    out.tm_min  = min;
    out.tm_sec  = sec;
    return true;
}

static void setFallbackTimeFromCompile()
{
    struct tm ti;
    const char *compileStr = __DATE__ " " __TIME__;
    if (!parseCompileTime(compileStr, ti)) return;

    
    
    time_t t = mktime(&ti);
    if (t == (time_t)-1) return;

    struct timeval tv = { t, 0 };
    settimeofday(&tv, nullptr);
    
    long offset = (long)(kitenConfig.tmz * 3600.0f);
    if (kitenConfig.dst) offset += 3600;
    configTime(offset, 0, NTP_SERVER);
}

void updateClockTimezone()
{
    long offset = (long)(kitenConfig.tmz * 3600.0f);
    if (kitenConfig.dst) offset += 3600;
    configTime(offset, 0, NTP_SERVER);

    
    
    
    
    time_t now = time(nullptr);
    if (now < 100) {
        setFallbackTimeFromCompile();
    }
    kitenConfig.setClockSet(true);
}

int8_t runClockLoop(bool showMenuHint)
{
    M5.Display.fillScreen(kitenConfig.bgColor);
    delay(300);

    
    
    
    
    if (time(nullptr) < 100) {
        setFallbackTimeFromCompile();
    }

    unsigned long lastDraw   = 0;
    unsigned long hintStart  = millis();
    bool          hintVisible = showMenuHint;
    bool          lastColon   = false;

    for (;;) {
        M5.update();
        pollKeyboard();

        if (check(SelPress)) {
            M5.Display.fillScreen(kitenConfig.bgColor);
            uiBeep(880, 50);
            waitAllKeysReleased();
            return 0;
        }
        if (check(EscPress)) {
            M5.Display.fillScreen(kitenConfig.bgColor);
            uiBeep(440, 30);
            waitAllKeysReleased();
            return -1;
        }
        if (check(AnyKeyPress)) wakeUpScreen();

        unsigned long now = millis();
        if (now - lastDraw < 200) {
            delay(5);
            continue;
        }
        lastDraw = now;

        
        
        
        
        struct tm timeInfo;
        if (!getLocalTime(&timeInfo, 0)) {
            
            setFallbackTimeFromCompile();
            if (!getLocalTime(&timeInfo, 0)) {
                snprintf(timeStr, sizeof(timeStr), "--:--:--");
            } else {
                updateTimeStr(timeInfo);
            }
        } else {
            updateTimeStr(timeInfo);
        }

        
        
        
        unsigned long pulse = (now % ANIM_CURSOR_PULSE_MS) / (ANIM_CURSOR_PULSE_MS / 2);
        uint16_t borderColor = (pulse == 0) ? kitenConfig.priColor
                                            : getColorVariation(kitenConfig.priColor);
        
        M5.Display.drawRect(BORDER_PAD_X - 1, BORDER_PAD_X - 1,
                            SCREEN_WIDTH - 2 * BORDER_PAD_X + 2,
                            SCREEN_HEIGHT - 2 * BORDER_PAD_X + 2,
                            kitenConfig.bgColor);
        M5.Display.drawRect(BORDER_PAD_X, BORDER_PAD_X,
                            SCREEN_WIDTH - 2 * BORDER_PAD_X,
                            SCREEN_HEIGHT - 2 * BORDER_PAD_X,
                            borderColor);

        
        
        uint8_t f_size = 1;
        for (uint8_t i = 4; i >= 1; i--) {
            int needed = i * LW * strlen(timeStr);
            if (needed < (SCREEN_WIDTH - BORDER_PAD_X * 2)) {
                f_size = i;
                break;
            }
        }

        
        bool showColon = (now % (2 * ANIM_CLOCK_COLON_MS)) < ANIM_CLOCK_COLON_MS;
        if (showColon != lastColon) {
            
            M5.Display.fillRect(BORDER_PAD_X + 2, BORDER_PAD_X + 2,
                                SCREEN_WIDTH - 2 * BORDER_PAD_X - 4,
                                SCREEN_HEIGHT - 2 * BORDER_PAD_X - 4,
                                kitenConfig.bgColor);
            lastColon = showColon;
        }

        
        String disp = String(timeStr);
        if (!showColon) {
            for (unsigned i = 0; i < disp.length(); i++) {
                if (disp[i] == ':') disp.setCharAt(i, ' ');
            }
        }

        M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
        M5.Display.setTextSize(f_size);
        M5.Display.setTextDatum(top_center);
        
        
        M5.Display.drawString(disp, SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 - 13);
        M5.Display.setTextDatum(top_left);

        
        if (hintVisible) {
            if (now - hintStart < 5000) {
                M5.Display.setTextSize(FP);
                M5.Display.setTextColor(kitenConfig.secColor, kitenConfig.bgColor);
                M5.Display.setTextDatum(top_center);
                M5.Display.drawString("OK to show menu", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 25);
                M5.Display.setTextDatum(top_left);
            } else {
                M5.Display.fillRect(BORDER_PAD_X + 1, SCREEN_HEIGHT / 2 + 20,
                                    SCREEN_WIDTH - 2 * BORDER_PAD_X - 2, 20,
                                    kitenConfig.bgColor);
                hintVisible = false;
            }
        }

        delay(5);
    }
}

void showClockSubMenu()
{
    std::vector<Option> opts = {
        {"Timer",         []() { displayInfo("Timer coming soon", true); }},
        {"Back to Clock", []() {}},
        {"Exit",          []() { clockExitRequested = true; }},
    };
    delay(200);
    int sel = loopOptions(opts, MENU_TYPE_SUBMENU, "Clock");
    (void)sel;
}

void clockMenuEntry()
{
    for (;;) {
        clockExitRequested = false;
        int8_t r = runClockLoop(true);
        if (r == -1) return;

        showClockSubMenu();
        if (clockExitRequested) return;
    }
}

struct TZEntry { const char *label; float offset; };
static const TZEntry TZ_LIST[TZ_ENTRIES_TOTAL] = {
    {"UTC-12 (Baker, Howland)",                   -12   },
    {"UTC-11 (Niue, Pago Pago)",                  -11   },
    {"UTC-10 (Honolulu, Papeete)",                -10   },
    {"UTC-9 (Anchorage, Gambell)",                 -9   },
    {"UTC-9.5 (Marquesas Islands)",                -9.5 },
    {"UTC-8 (LA, Vancouver, Tijuana)",             -8   },
    {"UTC-7 (Denver, Phoenix, Edmonton)",          -7   },
    {"UTC-6 (Mexico City, Chicago)",               -6   },
    {"UTC-5 (NY, Toronto, Lima)",                  -5   },
    {"UTC-4 (Caracas, Santiago, La Paz)",          -4   },
    {"UTC-3 (Brasilia, Sao Paulo)",                -3   },
    {"UTC-2 (South Georgia, Mid-Atl)",             -2   },
    {"UTC-1 (Azores, Cape Verde)",                 -1   },
    {"UTC+0 (London, Lisbon, Casablanca)",          0   },
    {"UTC+0.5 (Tehran)",                           0.5 },
    {"UTC+1 (Berlin, Paris, Rome)",                 1   },
    {"UTC+2 (Cairo, Athens, Joburg)",               2   },
    {"UTC+3 (Moscow, Riyadh, Nairobi)",             3   },
    {"UTC+3.5 (Tehran)",                            3.5 },
    {"UTC+4 (Dubai, Baku, Muscat)",                 4   },
    {"UTC+4.5 (Kabul)",                             4.5 },
    {"UTC+5 (Islamabad, Karachi)",                  5   },
    {"UTC+5.5 (New Delhi, Mumbai)",                 5.5 },
    {"UTC+5.75 (Kathmandu)",                        5.75},
    {"UTC+6 (Dhaka, Almaty, Omsk)",                 6   },
    {"UTC+6.5 (Yangon, Cocos Islands)",             6.5 },
    {"UTC+7 (Bangkok, Jakarta, Hanoi)",             7   },
    {"UTC+8 (Beijing, Singapore, Perth)",           8   },
    {"UTC+8.75 (Eucla)",                            8.75},
    {"UTC+9 (Tokyo, Seoul, Pyongyang)",             9   },
    {"UTC+9.5 (Adelaide, Darwin)",                  9.5 },
    {"UTC+10 (Sydney, Melbourne, Vladi)",          10   },
    {"UTC+10.5 (Lord Howe Island)",                10.5 },
    {"UTC+11 (Solomon Is., Noumea)",               11   },
    {"UTC+12 (Auckland, Fiji, Kamchatka)",         12   },
    {"UTC+12.75 (Chatham Islands)",                12.75},
    {"UTC+13 (Tonga, Phoenix Islands)",            13   },
    {"UTC+14 (Kiritimati)",                        14   },
};

static void setManualHour();
static void setManualMinute();

static bool tryNtpSync()
{
    
    if (WiFi.status() == WL_CONNECTED) {
        configTime((long)(kitenConfig.tmz * 3600),
                   kitenConfig.dst ? 3600 : 0,
                   NTP_SERVER);
        for (int j = 0; j < 20; j++) {
            struct tm ti;
            if (getLocalTime(&ti, 500)) return true;
            delay(100);
        }
        return false;
    }

    
    displayInfo("Connecting WiFi...", false);
    WiFi.mode(WIFI_STA);
    WiFi.begin();

    
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 8000) {
        delay(100);
    }

    if (WiFi.status() != WL_CONNECTED) {
        displayWarning("No WiFi - set time manually", true);
        return false;
    }

    displayInfo("Syncing NTP...", false);
    configTime((long)(kitenConfig.tmz * 3600),
               kitenConfig.dst ? 3600 : 0,
               NTP_SERVER);
    for (int j = 0; j < 20; j++) {
        struct tm ti;
        if (getLocalTime(&ti, 500)) {
            displaySuccess("Time synced via NTP", true);
            return true;
        }
        delay(100);
    }
    displayWarning("NTP failed - set manually", true);
    return false;
}

void setClock()
{
    for (;;) {
        std::vector<Option> opts;
        opts.push_back({"Via NTP Set Timezone", [](){}});
        opts.push_back({"Set Time Manually",    [](){}});

        {
            String lbl = "Daylight Savings ";
            lbl += kitenConfig.dst ? "On" : "Off";
            opts.push_back({lbl, [](){
                kitenConfig.setDST(!kitenConfig.dst);
                updateClockTimezone();
            }});
        }
        opts.push_back({kitenConfig.clock24hr ? "24-Hour Format" : "12-Hour Format", [](){
            kitenConfig.setClock24Hr(!kitenConfig.clock24hr);
        }});
        opts.push_back({"Back", [](){}});

        int sel = loopOptions(opts, MENU_TYPE_SUBMENU, "Clock");
        if (sel == -1 || sel == (int)opts.size() - 1) return;

        if (sel == 0) {
            
            std::vector<Option> tzOpts;
            int currentIdx = 0;
            for (int i = 0; i < TZ_ENTRIES_TOTAL; i++) {
                if (TZ_LIST[i].offset == kitenConfig.tmz) currentIdx = i;
                tzOpts.push_back({TZ_LIST[i].label, [i](){
                    kitenConfig.setTmz(TZ_LIST[i].offset);
                    
                    
                    
                    
                    bool synced = tryNtpSync();
                    updateClockTimezone();
                    if (!synced) {
                        
                        displayInfo("Time set to fallback. Set manually to adjust.", true);
                    }
                }, (i == currentIdx)});
            }
            tzOpts.push_back({"Back", [](){}});
            loopOptions(tzOpts, MENU_TYPE_REGULAR, "Timezone");
        } else if (sel == 1) {
            setManualHour();
            setManualMinute();
            kitenConfig.setClockSet(true);
            updateClockTimezone();
            displaySuccess("Time set", true);
        }
        
    }
}

static void setManualHour()
{
    std::vector<Option> opts;
    for (int h = 0; h < 24; h++) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%02d", h);
        opts.push_back({String(buf), [h](){
            struct tm ti;
            if (!getLocalTime(&ti, 0)) {
                memset(&ti, 0, sizeof(ti));
                ti.tm_year = 2025 - 1900;
                ti.tm_mon  = 0;
                ti.tm_mday = 1;
            }
            ti.tm_hour = h;
            ti.tm_sec  = 0;
            time_t t = mktime(&ti);
            struct timeval tv = { t, 0 };
            settimeofday(&tv, nullptr);
        }});
    }
    opts.push_back({"Back", [](){}});
    loopOptions(opts, MENU_TYPE_REGULAR, "Set Hour");
}

static void setManualMinute()
{
    std::vector<Option> opts;
    for (int m = 0; m < 60; m++) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%02d", m);
        opts.push_back({String(buf), [m](){
            struct tm ti;
            if (!getLocalTime(&ti, 0)) {
                memset(&ti, 0, sizeof(ti));
                ti.tm_year = 2025 - 1900;
                ti.tm_mon  = 0;
                ti.tm_mday = 1;
            }
            ti.tm_min = m;
            ti.tm_sec = 0;
            time_t t = mktime(&ti);
            struct timeval tv = { t, 0 };
            settimeofday(&tv, nullptr);
        }});
    }
    opts.push_back({"Back", [](){}});
    loopOptions(opts, MENU_TYPE_REGULAR, "Set Minute");
}
