#include "menu.h"
#include "config.h"
#include "settings.h"
#include "keyboard.h"
#include "ui.h"
#include <Arduino.h>
#include <M5Unified.h>

static std::vector<String> labelsOf(const std::vector<Option> &options)
{
    std::vector<String> v;
    v.reserve(options.size());
    for (auto &o : options) v.push_back(o.label);
    return v;
}

int findNextEnabled(const std::vector<Option> &options, int from, int dir)
{
    int n = options.size();
    if (n == 0) return 0;
    int idx = from;
    for (int i = 0; i < n; i++) {
        idx = (idx + dir + n) % n;
        if (options[idx].enabled) return idx;
    }
    return from;
}

int loopOptions(std::vector<Option> &options, uint8_t menuType, const char *title)
{
    if (options.empty()) return -1;

    int index = 0;
    
    if (!options[index].enabled) index = findNextEnabled(options, index, +1);

    bool firstRender = true;
    int  prevIndex   = index;
    int  animOffset  = 0;   
    bool animating   = false;
    int  animDir     = 0;
    int  animStep    = 0;

    
    markActivity();

    
    if (menuType == MENU_TYPE_SUBMENU || menuType == MENU_TYPE_REGULAR) {
        drawMainBorder(true);
    } else if (menuType == MENU_TYPE_MAIN) {
        drawMainBorder(true);
    }

    for (;;) {
        M5.update();
        pollKeyboard();
        checkPowerSaveTime();

        
        if (animating) {
            
            int maxOffset = 10;
            int magnitude = maxOffset * (ANIM_SLIDE_STEPS - animStep) / ANIM_SLIDE_STEPS;
            animOffset = animDir * magnitude;
            animStep++;
            if (animStep > ANIM_SLIDE_STEPS) {
                animOffset = 0;
                animating = false;
            }
            if (menuType == MENU_TYPE_SUBMENU) {
                drawSubmenu(index, labelsOf(options), title, animOffset);
            } else if (menuType == MENU_TYPE_MAIN) {
                drawMainMenu(index, labelsOf(options), animOffset);
            }
            delay(ANIM_SLIDE_STEP_MS);
            continue;
        }

        
        bool redraw = firstRender;
        if (menuType == MENU_TYPE_SUBMENU) {
            if (redraw || index != prevIndex) {
                drawSubmenu(index, labelsOf(options), title, 0);
                prevIndex = index;
                firstRender = false;
                redraw = false;
            } else {
                
                
                static unsigned long lastStatus = 0;
                if (millis() - lastStatus > 1000) {
                    drawStatusBar();
                    lastStatus = millis();
                }
            }
        } else if (menuType == MENU_TYPE_REGULAR) {
            if (redraw || index != prevIndex) {
                
                std::vector<String> labels;
                labels.reserve(options.size());
                for (auto &o : options) labels.push_back(o.label);
                drawOptions(index, labels,
                            kitenConfig.priColor, kitenConfig.secColor, kitenConfig.bgColor,
                            firstRender);
                prevIndex = index;
                firstRender = false;
            }
        } else {
            
            if (redraw || index != prevIndex) {
                std::vector<String> labels;
                labels.reserve(options.size());
                for (auto &o : options) labels.push_back(o.label);
                drawMainMenu(index, labels, 0);
                prevIndex = index;
                firstRender = false;
            } else {
                
                static unsigned long lastStatus = 0;
                if (millis() - lastStatus > 1000) {
                    
                    std::vector<String> labels;
                    labels.reserve(options.size());
                    for (auto &o : options) labels.push_back(o.label);
                    drawMainMenu(index, labels, 0);
                    lastStatus = millis();
                }
            }
        }

        
        if (check(PrevPress) || check(UpPress)) {
            int newIdx = findNextEnabled(options, index, -1);
            if (newIdx != index) {
                index = newIdx;
                animDir = +1;   
                animating = (menuType == MENU_TYPE_SUBMENU || menuType == MENU_TYPE_MAIN);
                animStep = 0;
                uiBeep(660, 25);
            }
            markActivity();
        }
        if (check(NextPress) || check(DownPress)) {
            int newIdx = findNextEnabled(options, index, +1);
            if (newIdx != index) {
                index = newIdx;
                animDir = -1;   
                animating = (menuType == MENU_TYPE_SUBMENU || menuType == MENU_TYPE_MAIN);
                animStep = 0;
                uiBeep(660, 25);
            }
            markActivity();
        }
        if (check(SelPress)) {
            if (options[index].enabled) {
                uiBeep(880, 50);
                waitAllKeysReleased();
                options[index].operation();
                return index;
            } else {
                uiBeep(220, 80);   
            }
            markActivity();
        }
        if (check(EscPress)) {
            if (menuType != MENU_TYPE_MAIN) {
                uiBeep(440, 30);
                waitAllKeysReleased();
                return -1;
            }
            markActivity();
        }
        if (check(AnyKeyPress)) {
            markActivity();
            wakeUpScreen();
        }

        delay(5);
    }
}
