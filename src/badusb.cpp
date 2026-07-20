

#include "others_menu.h"
#include "config.h"
#include "settings.h"
#include "keyboard.h"
#include "ui.h"
#include "menu.h"
#include "sd_helper.h"
#include <Arduino.h>
#include <M5Unified.h>
#include <M5Cardputer.h>
#include <USB.h>
#include <USBHID.h>
#include <USBHIDKeyboard.h>
#include <SD.h>

static USBHIDKeyboard *g_hidKb = nullptr;

static USBHIDKeyboard *getHidKb()
{
    if (!g_hidKb) {
        g_hidKb = new USBHIDKeyboard();
        g_hidKb->begin();
        USB.begin();
        delay(150);   
    }
    return g_hidKb;
}

static String trimStr(const String &s)
{
    int start = 0;
    while (start < (int)s.length() && (s[start] == ' ' || s[start] == '\t' || s[start] == '\r')) start++;
    int end = s.length();
    while (end > start && (s[end-1] == ' ' || s[end-1] == '\t' || s[end-1] == '\r')) end--;
    return s.substring(start, end);
}

static uint8_t duckyKeyNameToCode(const String &name)
{
    String n = name;
    n.toUpperCase();
    n.trim();

    if (n == "ENTER"      || n == "RETURN")    return KEY_RETURN;
    if (n == "ESC"        || n == "ESCAPE")    return KEY_ESC;
    if (n == "BACKSPACE"  || n == "BKSP")      return KEY_BACKSPACE;
    if (n == "TAB")                            return KEY_TAB;
    if (n == "SPACE"       || n == "SPACEBAR")  return 0x2C;  
    if (n == "DELETE"      || n == "DEL")       return KEY_DELETE;
    if (n == "INSERT"      || n == "INS")       return KEY_INSERT;
    if (n == "HOME")                            return KEY_HOME;
    if (n == "END")                             return KEY_END;
    if (n == "PAGEUP")                          return KEY_PAGE_UP;
    if (n == "PAGEDOWN")                        return KEY_PAGE_DOWN;
    if (n == "UP")                              return KEY_UP_ARROW;
    if (n == "DOWN")                            return KEY_DOWN_ARROW;
    if (n == "LEFT")                            return KEY_LEFT_ARROW;
    if (n == "RIGHT")                           return KEY_RIGHT_ARROW;
    if (n == "CAPSLOCK")                        return KEY_CAPS_LOCK;
    
    
    if (n == "NUMLOCK")                         return 0x53;  
    if (n == "SCROLLLOCK")                      return 0x47;  
    if (n == "PRINTSCREEN")                     return 0x46;  
    if (n == "PAUSE")                           return 0x48;  
    if (n == "MENU"       || n == "APP")        return 0x65;  
    if (n == "F1")  return KEY_F1;
    if (n == "F2")  return KEY_F2;
    if (n == "F3")  return KEY_F3;
    if (n == "F4")  return KEY_F4;
    if (n == "F5")  return KEY_F5;
    if (n == "F6")  return KEY_F6;
    if (n == "F7")  return KEY_F7;
    if (n == "F8")  return KEY_F8;
    if (n == "F9")  return KEY_F9;
    if (n == "F10") return KEY_F10;
    if (n == "F11") return KEY_F11;
    if (n == "F12") return KEY_F12;
    return 0;
}

static uint8_t duckyModNameToCode(const String &name)
{
    String n = name;
    n.toUpperCase();
    n.trim();
    if (n == "GUI"     || n == "WINDOWS" || n == "META")  return KEY_LEFT_GUI;
    if (n == "SHIFT")                                     return KEY_LEFT_SHIFT;
    if (n == "ALT"     || n == "OPTION")                   return KEY_LEFT_ALT;
    if (n == "CTRL"    || n == "CONTROL")                  return KEY_LEFT_CTRL;
    return 0;
}

static long execDuckyLine(USBHIDKeyboard *kb, const String &line, long defaultDelay)
{
    String s = trimStr(line);
    if (s.length() == 0) return defaultDelay;

    
    if (s.startsWith("REM ")) return defaultDelay;

    int space = s.indexOf(' ');
    String cmd = (space < 0) ? s : s.substring(0, space);
    String arg = (space < 0) ? "" : trimStr(s.substring(space + 1));
    cmd.toUpperCase();

    
    if (cmd == "DEFAULT_DELAY" || cmd == "DEFAULTDELAY") {
        if (arg.length() > 0) return arg.toInt();
        return defaultDelay;
    }

    
    if (cmd == "DELAY") {
        long ms = arg.toInt();
        if (ms <= 0) ms = 100;
        delay(ms);
        return defaultDelay;
    }

    
    if (cmd == "STRING") {
        
        kb->print(arg);
        return defaultDelay;
    }

    
    
    if (cmd == "REPEAT") {
        return defaultDelay;
    }

    
    
    {
        std::vector<uint8_t> mods;
        std::vector<uint8_t> keys;
        bool isCombo = false;

        std::vector<String> tokens;
        int idx = 0;
        while (idx < (int)arg.length()) {
            int sp = arg.indexOf(' ', idx);
            String t = (sp < 0) ? arg.substring(idx) : arg.substring(idx, sp);
            t.toUpperCase();
            t.trim();
            if (t.length() > 0) tokens.push_back(t);
            if (sp < 0) break;
            idx = sp + 1;
        }

        for (const String &t : tokens) {
            uint8_t mod = duckyModNameToCode(t);
            if (mod != 0) {
                mods.push_back(mod);
                isCombo = true;
                continue;
            }
            uint8_t code = duckyKeyNameToCode(t);
            if (code != 0) {
                keys.push_back(code);
                isCombo = true;
            }
        }

        if (isCombo && !keys.empty()) {
            
            for (uint8_t m : mods) kb->press(m);
            
            for (uint8_t k : keys) {
                kb->press(k);
                delay(20);
            }
            kb->releaseAll();
            return defaultDelay;
        }
    }

    
    {
        uint8_t code = duckyKeyNameToCode(cmd);
        if (code != 0) {
            kb->press(code);
            delay(20);
            kb->release(code);
            return defaultDelay;
        }
    }

    
    return defaultDelay;
}

void ducky_setup_impl()
{
    if (!kitenSdBegin()) {
        displayError("SD card required", true);
        return;
    }

    
    std::vector<String> files = kitenSdListFiles("/", ".txt");
    if (files.empty()) {
        displayInfo(
            "No .txt scripts\n"
            "found on SD.\n\n"
            "Place Ducky Script\n"
            "files in / on the\n"
            "SD card.",
            true);
        return;
    }

    std::vector<Option> opts;
    for (const String &f : files) {
        int slash = f.lastIndexOf('/');
        String fname = (slash >= 0) ? f.substring(slash + 1) : f;
        opts.push_back({fname, [f]() {
            
            String script = kitenSdReadFile(f, 50000);
            if (script.length() == 0) {
                displayError("Cannot read script", true);
                return;
            }

            
            USBHIDKeyboard *kb = getHidKb();

            
            M5.Display.fillScreen(kitenConfig.bgColor);
            drawMainBorder(false);
            M5.Display.setTextSize(FP);
            M5.Display.setTextColor(TFT_GREEN, kitenConfig.bgColor);
            M5.Display.setCursor(4, 30);
            M5.Display.print("RUNNING:");
            M5.Display.setTextColor(TFT_WHITE, kitenConfig.bgColor);
            int slash2 = f.lastIndexOf('/');
            String fname2 = (slash2 >= 0) ? f.substring(slash2 + 1) : f;
            M5.Display.setCursor(60, 33);
            M5.Display.print(fname2);
            M5.Display.setCursor(4, 50);
            M5.Display.print("ESC to abort");

            
            long defaultDelay = 0;
            int lineNum = 0;
            int idx = 0;
            String line;
            bool aborted = false;

            while (idx < (int)script.length()) {
                char c = script[idx++];
                if (c == '\r') continue;
                if (c == '\n') {
                    lineNum++;
                    
                    M5.Display.fillRect(4, 70, SCREEN_WIDTH - 8, LH, kitenConfig.bgColor);
                    M5.Display.setCursor(4, 70);
                    M5.Display.printf("Line %d", lineNum);

                    long d = execDuckyLine(kb, line, defaultDelay);
                    if (d >= 0) defaultDelay = d;
                    if (defaultDelay > 0) delay(defaultDelay);

                    
                    M5.update();
                    pollKeyboard();
                    if (check(EscPress)) { aborted = true; break; }

                    line = "";
                } else {
                    line += c;
                }
            }
            
            if (!aborted && line.length() > 0) {
                execDuckyLine(kb, line, defaultDelay);
            }

            if (aborted) {
                displayWarning("Aborted by user", true);
            } else {
                displaySuccess("Script complete", true);
            }
            waitAllKeysReleased();
        }});
    }
    opts.push_back({"Back", [](){}});
    loopOptions(opts, MENU_TYPE_REGULAR, "Pick script");
}

void ducky_keyboard_impl()
{
    USBHIDKeyboard *kb = getHidKb();

    M5.Display.fillScreen(kitenConfig.bgColor);
    drawMainBorder(false);
    M5.Display.setTextSize(FP);
    M5.Display.setTextColor(TFT_GREEN, kitenConfig.bgColor);
    M5.Display.setCursor(4, 30);
    M5.Display.print("USB KEYBOARD");
    M5.Display.setTextColor(TFT_WHITE, kitenConfig.bgColor);
    M5.Display.setCursor(4, 45);
    M5.Display.print("Type on Cardputer.");
    M5.Display.setCursor(4, 55);
    M5.Display.print("Host receives keys.");
    M5.Display.setCursor(4, 70);
    M5.Display.setTextColor(TFT_RED, kitenConfig.bgColor);
    M5.Display.print("Backspace = ESC");

    waitAllKeysReleased();
    while (!check(EscPress)) {
        M5.update();
        M5Cardputer.update();
        if (M5Cardputer.Keyboard.isChange()) {
            if (M5Cardputer.Keyboard.isPressed()) {
                Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
                
                uint8_t modMask = 0;
                if (status.ctrl)  modMask |= KEY_LEFT_CTRL;
                if (status.alt)   modMask |= KEY_LEFT_ALT;
                if (status.shift) modMask |= KEY_LEFT_SHIFT;
                if (status.opt)   modMask |= KEY_LEFT_GUI;

                
                if (modMask) {
                    if (status.ctrl)  kb->press(KEY_LEFT_CTRL);
                    if (status.alt)   kb->press(KEY_LEFT_ALT);
                    if (status.shift) kb->press(KEY_LEFT_SHIFT);
                    if (status.opt)   kb->press(KEY_LEFT_GUI);
                }

                
                for (char c : status.word) {
                    if (c == 0) continue;
                    
                    if (c == 8) continue;
                    kb->press(c);
                    delay(10);
                    kb->release(c);
                }
                
                if (status.enter) {
                    kb->press(KEY_RETURN);
                    delay(10);
                    kb->release(KEY_RETURN);
                }
                
                if (status.tab) {
                    kb->press(KEY_TAB);
                    delay(10);
                    kb->release(KEY_TAB);
                }
                
                if (modMask) kb->releaseAll();
            }
        }
        pollKeyboard();
        delay(5);
    }
    waitAllKeysReleased();
}

struct KbdRemapEntry {
    char ascii;
    uint8_t keycode;     
    uint8_t modifier;    
};

static const KbdRemapEntry DE_REMAP[] = {
    {'@',  0x14, 0x40},   
    {'|',  0x33, 0x40},   
    {'~',  0x35, 0x40},   
    {'{',  0x24, 0x40},   
    {'}',  0x2D, 0x40},   
    {'[',  0x2A, 0x40},   
    {']',  0x27, 0x40},   
    {'\\', 0x2E, 0x40},   
    {'^',  0x30, 0x02},   
    {'<',  0x33, 0x02},   
    {'>',  0x33, 0x02},   
    {'\'', 0x34, 0x02},   
    
};
static const int DE_REMAP_COUNT = sizeof(DE_REMAP) / sizeof(DE_REMAP[0]);

static void sendLayoutChar(USBHIDKeyboard *kb, char c, int layoutIdx)
{
    if (layoutIdx == 1) {  
        for (int i = 0; i < DE_REMAP_COUNT; i++) {
            if (DE_REMAP[i].ascii == c) {
                
                
                
                
                
                uint8_t mod = DE_REMAP[i].modifier;
                if (mod & 0x02) kb->press(KEY_LEFT_SHIFT);
                if (mod & 0x40) kb->press(KEY_RIGHT_ALT);   
                if (mod & 0x04) kb->press(KEY_LEFT_ALT);
                
                
                
                
                kb->press(DE_REMAP[i].keycode);
                delay(5);
                kb->release(DE_REMAP[i].keycode);
                if (mod & 0x02) kb->release(KEY_LEFT_SHIFT);
                if (mod & 0x40) kb->release(KEY_RIGHT_ALT);
                if (mod & 0x04) kb->release(KEY_LEFT_ALT);
                return;
            }
        }
    }
    
    
    kb->print(c);
}

static void sendLayoutString(USBHIDKeyboard *kb, const String &s, int layoutIdx)
{
    for (char c : s) {
        sendLayoutChar(kb, c, layoutIdx);
        delay(2);
    }
}

static long execDuckyLineWithLayout(USBHIDKeyboard *kb, const String &line,
                                     long defaultDelay, int layoutIdx)
{
    String s = trimStr(line);
    if (s.length() == 0) return defaultDelay;
    if (s.startsWith("REM ")) return defaultDelay;

    int space = s.indexOf(' ');
    String cmd = (space < 0) ? s : s.substring(0, space);
    String arg = (space < 0) ? "" : trimStr(s.substring(space + 1));
    cmd.toUpperCase();

    if (cmd == "STRING") {
        sendLayoutString(kb, arg, layoutIdx);
        return defaultDelay;
    }
    
    return execDuckyLine(kb, line, defaultDelay);
}

void ducky_run_file(const String &path, int layoutIdx, int osIdx)
{
    if (!kitenSdBegin()) {
        displayError("SD card required", true);
        return;
    }

    String script = kitenSdReadFile(path, 50000);
    if (script.length() == 0) {
        displayError("Cannot read script", true);
        return;
    }

    USBHIDKeyboard *kb = getHidKb();

    
    
    if (osIdx == 1) {  
        delay(500);
    }

    int slash = path.lastIndexOf('/');
    String fname = (slash >= 0) ? path.substring(slash + 1) : path;

    
    M5.Display.fillScreen(kitenConfig.bgColor);
    drawMainBorder(false);
    M5.Display.setTextSize(FP);
    M5.Display.setTextColor(TFT_GREEN, kitenConfig.bgColor);
    M5.Display.setCursor(4, 30);
    M5.Display.print("RUNNING:");
    M5.Display.setTextColor(TFT_WHITE, kitenConfig.bgColor);
    M5.Display.setCursor(60, 33);
    M5.Display.print(fname);
    
    M5.Display.setTextColor(TFT_CYAN, kitenConfig.bgColor);
    M5.Display.setCursor(4, 45);
    M5.Display.printf("Layout:%d  OS:%d", layoutIdx, osIdx);
    M5.Display.setTextColor(TFT_WHITE, kitenConfig.bgColor);
    M5.Display.setCursor(4, 60);
    M5.Display.print("ESC to abort");

    
    long defaultDelay = 0;
    int lineNum = 0;
    int idx = 0;
    String line;
    bool aborted = false;

    while (idx < (int)script.length()) {
        char c = script[idx++];
        if (c == '\r') continue;
        if (c == '\n') {
            lineNum++;
            M5.Display.fillRect(4, 75, SCREEN_WIDTH - 8, LH, kitenConfig.bgColor);
            M5.Display.setCursor(4, 75);
            M5.Display.printf("Line %d", lineNum);

            long d = execDuckyLineWithLayout(kb, line, defaultDelay, layoutIdx);
            if (d >= 0) defaultDelay = d;
            if (defaultDelay > 0) delay(defaultDelay);

            M5.update();
            pollKeyboard();
            if (check(EscPress)) { aborted = true; break; }

            line = "";
        } else {
            line += c;
        }
    }
    if (!aborted && line.length() > 0) {
        execDuckyLineWithLayout(kb, line, defaultDelay, layoutIdx);
    }

    if (aborted) {
        displayWarning("Aborted by user", true);
    } else {
        displaySuccess("Script complete", true);
    }
    waitAllKeysReleased();
}
