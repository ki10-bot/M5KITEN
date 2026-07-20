

#include "others_menu.h"
#include "config.h"
#include "settings.h"
#include "keyboard.h"
#include "ui.h"
#include "menu.h"
#include <Arduino.h>
#include <M5Unified.h>
#include <USB.h>
#include <USBHID.h>
#include <USBHIDMouse.h>

static USBHIDMouse *g_hidMouse = nullptr;
static USBHIDMouse *getHidMouse()
{
    if (!g_hidMouse) {
        g_hidMouse = new USBHIDMouse();
        g_hidMouse->begin();
        USB.begin();
        delay(100);
    }
    return g_hidMouse;
}

static unsigned long clickerDelay = 100;   
static int clickerButton = 0;              
static int clickerMaxClicks = 0;           

void clicker_setup_impl()
{
    USBHIDMouse *mouse = getHidMouse();

    int sel = 0;
    std::vector<Option> opts;
    for (;;) {
        opts.clear();
        opts.push_back({"Delay: " + String(clickerDelay) + " ms", []() {
            String s = keyboard(String(clickerDelay), 5, "Delay (ms, 1-2000):");
            if (s == "\x1B" || s.length() == 0) return;
            long v = s.toInt();
            if (v < 1) v = 1;
            if (v > 2000) v = 2000;
            clickerDelay = (unsigned long)v;
        }});
        const char *btnNames[] = {"Left", "Right", "Middle"};
        opts.push_back({"Button: " + String(btnNames[clickerButton]), []() {
            std::vector<Option> bopts;
            for (int i = 0; i < 3; i++) {
                const char *names[] = {"Left", "Right", "Middle"};
                bopts.push_back({names[i], [i]() { clickerButton = i; }});
            }
            bopts.push_back({"Back", [](){}});
            loopOptions(bopts, MENU_TYPE_REGULAR, "Pick button");
        }});
        opts.push_back({"Clicks: " + String(clickerMaxClicks == 0 ? String("inf") : String(clickerMaxClicks)), []() {
            String s = keyboard(String(clickerMaxClicks), 4, "Clicks (0=infinite):");
            if (s == "\x1B" || s.length() == 0) return;
            long v = s.toInt();
            if (v < 0) v = 0;
            if (v > 999) v = 999;
            clickerMaxClicks = (int)v;
        }});
        opts.push_back({"Start", [mouse]() {
            M5.Display.fillScreen(kitenConfig.bgColor);
            drawMainBorder(false);
            M5.Display.setTextSize(FM);
            M5.Display.setTextColor(TFT_GREEN, kitenConfig.bgColor);
            M5.Display.setCursor(4, 30);
            M5.Display.print("CLICKING");
            M5.Display.setTextSize(FP);
            M5.Display.setTextColor(TFT_WHITE, kitenConfig.bgColor);
            M5.Display.setCursor(4, 55);
            M5.Display.print("ESC to stop");
            M5.Display.setCursor(4, 70);
            M5.Display.print("Clicks: 0");

            waitAllKeysReleased();
            unsigned long lastClick = 0;
            int count = 0;
            while (!check(EscPress)) {
                if (millis() - lastClick >= clickerDelay) {
                    
                    if      (clickerButton == 0) mouse->press(MOUSE_LEFT);
                    else if (clickerButton == 1) mouse->press(MOUSE_RIGHT);
                    else                          mouse->press(MOUSE_MIDDLE);
                    delay(20);
                    
                    if      (clickerButton == 0) mouse->release(MOUSE_LEFT);
                    else if (clickerButton == 1) mouse->release(MOUSE_RIGHT);
                    else                          mouse->release(MOUSE_MIDDLE);

                    count++;
                    lastClick = millis();
                    M5.Display.fillRect(4, 70, 120, LH, kitenConfig.bgColor);
                    M5.Display.setCursor(4, 70);
                    M5.Display.printf("Clicks: %d", count);

                    if (clickerMaxClicks > 0 && count >= clickerMaxClicks) break;
                }
                M5.update();
                pollKeyboard();
                delay(5);
            }
            waitAllKeysReleased();
        }});
        opts.push_back({"Back", [](){}});
        sel = loopOptions(opts, MENU_TYPE_REGULAR, "USB Clicker");
        if (sel == -1 || sel == (int)opts.size() - 1) return;
    }
}
