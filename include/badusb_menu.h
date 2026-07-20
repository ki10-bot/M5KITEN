

#ifndef __KITEN_BADUSB_MENU_H__
#define __KITEN_BADUSB_MENU_H__

#include <Arduino.h>
#include <vector>

void badusbMenuEntry();

void run_badusb_script();

std::vector<String> getBadusbScriptsList();

int  badusbGetLayout();

int  badusbGetOsPreset();

void badusbSetLayout(int idx);

void badusbSetOsPreset(int idx);

#endif
