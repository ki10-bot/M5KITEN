#pragma once
#include <Arduino.h>
#include <functional>
#include <vector>

#include "config.h"   
#include "ui.h"
#include "keyboard.h"

struct Option {
    String label;
    std::function<void()> operation;
    bool selected = false;   
    bool enabled  = true;    

    Option(const String &lbl, const std::function<void()> &op,
           bool sel = false, bool en = true)
        : label(lbl), operation(op), selected(sel), enabled(en) {}
};

int loopOptions(std::vector<Option> &options,
                uint8_t menuType = MENU_TYPE_SUBMENU,
                const char *title = "");

inline int addBackItem(std::vector<Option> &options, const String &label = "Back")
{
    options.push_back({label, [](){}});
    return options.size() - 1;
}

int findNextEnabled(const std::vector<Option> &options, int from, int dir);
