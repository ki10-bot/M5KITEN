#pragma once
#include <M5Unified.h>
#include "settings.h"
#include <time.h>

void initUI();   

void drawMainBorder(bool clear = true);
void drawMainBorderWithTitle(const String &title, bool clear = true);
void drawStatusBar();

void drawBatteryStatus(uint8_t pct);
uint8_t getBattery();

void displayRedStripe(const String &text, uint16_t fg = TFT_WHITE, uint16_t bg = TFT_RED);
void displayError  (const String &text, bool wait = false);
void displayWarning(const String &text, bool wait = false);
void displayInfo   (const String &text, bool wait = false);
void displaySuccess(const String &text, bool wait = false);

int8_t displayMessage(const String &message,
                      const char *leftBtn,
                      const char *centerBtn,
                      const char *rightBtn,
                      uint16_t color);

struct OptCoord {
    int   x       = 0;
    int   y       = 0;
    int   size    = 0;
    uint16_t fg   = 0;
    uint16_t bg   = 0;
};

OptCoord drawOptions(int index, const std::vector<String> &labels,
                     int fg, int sel, int bg, bool firstRender);

void drawSubmenu(int index, const std::vector<String> &labels, const char *title,
                 int animOffset = 0);   

void drawMainMenu(int index, const std::vector<String> &labels, int animOffset = 0);

void animEnter();

void animExit();

void uiBeep(uint16_t freq = 880, uint16_t dur = 40);

void applyBrightness(uint8_t pct, bool instant = false);
void checkPowerSaveTime();
void wakeUpScreen();
void fadeOutScreen();
void sleepModeOn();
void sleepModeOff();

extern char timeStr[16];  
void updateTimeStr(struct tm timeInfo);

extern "C" void markActivity();

void logClear();
void logPrint(const String &s);
void logRender();
