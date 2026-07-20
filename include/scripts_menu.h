

#ifndef __KITEN_SCRIPTS_MENU_H__
#define __KITEN_SCRIPTS_MENU_H__

#include <Arduino.h>
#include <vector>

void scriptsMenuEntry();

void run_bjs_script();

std::vector<String> getScriptsOptionsList();

#endif
