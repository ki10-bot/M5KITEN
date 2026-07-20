

#include "usb_tools_menu.h"
#include "badusb_menu.h"
#include "usb_dropper.h"
#include "config.h"
#include "settings.h"
#include "keyboard.h"
#include "ui.h"
#include "menu.h"
#include <Arduino.h>
#include <M5Unified.h>

void usbToolsMenuEntry()
{
    std::vector<Option> opts = {
        {"BadUSB",    []() { badusbMenuEntry();    }},
        {"USB Drop",  []() { usbDropperMenuEntry(); }},
        {"Back",      [](){}},
    };
    loopOptions(opts, MENU_TYPE_SUBMENU, "USB Tools");
}