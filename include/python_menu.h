

#ifndef __KITEN_PYTHON_MENU_H__
#define __KITEN_PYTHON_MENU_H__

#include <Arduino.h>
#include <vector>

void pythonMenuEntry();

void run_py_script();

void pythonScriptContextMenu(const String &path);

std::vector<String> getPythonScriptsList();

#endif
