

#include "badusb_menu.h"
#include "others_menu.h"     
#include "file_editor.h"
#include "config.h"
#include "settings.h"
#include "keyboard.h"
#include "ui.h"
#include "menu.h"
#include "sd_helper.h"
#include <Arduino.h>
#include <M5Unified.h>
#include <M5Cardputer.h>
#include <SD.h>
#include <FS.h>
#include <Preferences.h>

struct KbdLayout {
    const char *code;     
    const char *name;     
};
static const KbdLayout KBD_LAYOUTS[] = {
    {"US", "English (US)"},
    {"DE", "German (DE)"},
    {"UK", "English (UK)"},
    {"FR", "French (FR)"},
    {"ES", "Spanish (ES)"},
    {"IT", "Italian (IT)"},
    {"BR", "Portuguese (BR)"},
};
static const int KBD_LAYOUTS_COUNT = sizeof(KBD_LAYOUTS) / sizeof(KBD_LAYOUTS[0]);

struct OsPreset {
    const char *code;
    const char *name;
};
static const OsPreset OS_PRESETS[] = {
    {"WIN", "Windows"},
    {"MAC", "macOS"},
    {"LIN", "Linux"},
};
static const int OS_PRESETS_COUNT = sizeof(OS_PRESETS) / sizeof(OS_PRESETS[0]);

int badusbGetLayout()
{
    Preferences prefs;
    prefs.begin("kiten", true);
    int v = prefs.getInt("badusb_layout", 0);
    prefs.end();
    if (v < 0 || v >= KBD_LAYOUTS_COUNT) v = 0;
    return v;
}
void badusbSetLayout(int idx)
{
    if (idx < 0 || idx >= KBD_LAYOUTS_COUNT) return;
    Preferences prefs;
    prefs.begin("kiten", false);
    prefs.putInt("badusb_layout", idx);
    prefs.end();
}
int badusbGetOsPreset()
{
    Preferences prefs;
    prefs.begin("kiten", true);
    int v = prefs.getInt("badusb_os", 0);
    prefs.end();
    if (v < 0 || v >= OS_PRESETS_COUNT) v = 0;
    return v;
}
void badusbSetOsPreset(int idx)
{
    if (idx < 0 || idx >= OS_PRESETS_COUNT) return;
    Preferences prefs;
    prefs.begin("kiten", false);
    prefs.putInt("badusb_os", idx);
    prefs.end();
}

std::vector<String> getBadusbScriptsList()
{
    std::vector<String> result;
    if (!kitenSdBegin()) return result;

    const char *folders[] = {"/scrusb", "/Scrusb", "/SCRSUB", "/badusb", "/BadUSB"};
    for (const char *folder : folders) {
        if (!SD.exists(folder)) continue;
        File root = SD.open(folder);
        if (!root) continue;
        File entry = root.openNextFile();
        while (entry) {
            if (!entry.isDirectory()) {
                String name = entry.name();
                String lower = name;
                lower.toLowerCase();
                if (lower.endsWith(".txt")) {
                    String full = String(folder) + "/" + name;
                    result.push_back(full);
                }
            }
            entry = root.openNextFile();
        }
        root.close();
    }
    return result;
}

extern void ducky_run_file(const String &path, int layoutIdx, int osIdx);

static void runBadusbFile(const String &path)
{
    int layoutIdx = badusbGetLayout();
    int osIdx     = badusbGetOsPreset();
    ducky_run_file(path, layoutIdx, osIdx);
}

void badusbScriptContextMenu(const String &path)
{
    int slash = path.lastIndexOf('/');
    String fname = (slash >= 0) ? path.substring(slash + 1) : path;

    std::vector<Option> opts = {
        {"Run",    [path]() { runBadusbFile(path); }},
        {"Edit",   [path]() { fileEditor(path); }},
        {"Delete", [path, fname]() {
            int8_t choice = displayMessage(
                "Delete " + fname + " ?",
                nullptr, "OK", "Cancel", TFT_RED);
            if (choice == 1) {
                if (kitenSdRemove(path)) {
                    displaySuccess("Deleted " + fname, true);
                } else {
                    displayError("Delete failed", true);
                }
            }
        }},
        {"Back",   [](){}},
    };
    loopOptions(opts, MENU_TYPE_SUBMENU, fname.c_str());
}

void badusbLayoutMenu()
{
    int cur = badusbGetLayout();
    std::vector<Option> opts;
    for (int i = 0; i < KBD_LAYOUTS_COUNT; i++) {
        String label = String(KBD_LAYOUTS[i].code) + " " + KBD_LAYOUTS[i].name;
        opts.push_back({label, [i]() {
            badusbSetLayout(i);
            displaySuccess(String("Layout: ") + KBD_LAYOUTS[i].code, true);
        }, i == cur, true});
    }
    opts.push_back({"Back", [](){}});
    loopOptions(opts, MENU_TYPE_SUBMENU, "KB Layout");
}

void badusbOsMenu()
{
    int cur = badusbGetOsPreset();
    std::vector<Option> opts;
    for (int i = 0; i < OS_PRESETS_COUNT; i++) {
        String label = String(OS_PRESETS[i].code) + " " + OS_PRESETS[i].name;
        opts.push_back({label, [i]() {
            badusbSetOsPreset(i);
            displaySuccess(String("OS: ") + OS_PRESETS[i].code, true);
        }, i == cur, true});
    }
    opts.push_back({"Back", [](){}});
    loopOptions(opts, MENU_TYPE_SUBMENU, "Host OS");
}

void badusbMenuEntry()
{
    if (!kitenSdBegin()) {
        displayError("SD card required", true);
        return;
    }
    std::vector<String> files = getBadusbScriptsList();
    std::vector<Option> opts;

    
    for (const String &f : files) {
        int slash = f.lastIndexOf('/');
        String fname = (slash >= 0) ? f.substring(slash + 1) : f;
        opts.push_back({fname, [f]() { badusbScriptContextMenu(f); }});
    }

    
    int curLayout = badusbGetLayout();
    int curOs     = badusbGetOsPreset();
    String layoutLabel = "Layout: " + String(KBD_LAYOUTS[curLayout].code);
    String osLabel     = "OS: " + String(OS_PRESETS[curOs].code);
    opts.push_back({layoutLabel, []() { badusbLayoutMenu(); }});
    opts.push_back({osLabel,     []() { badusbOsMenu();     }});
    opts.push_back({"USB Keyboard", []() { ducky_keyboard(); }});
    opts.push_back({"Back",       [](){}});
    loopOptions(opts, MENU_TYPE_REGULAR, "BadUSB");
}

void run_badusb_script()
{
    badusbMenuEntry();
}
