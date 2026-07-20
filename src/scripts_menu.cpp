

#include "scripts_menu.h"
#include "config.h"
#include "settings.h"
#include "keyboard.h"
#include "ui.h"
#include "menu.h"
#include "sd_helper.h"
#include <Arduino.h>
#include <M5Unified.h>
#include <SD.h>
#include <FS.h>

int8_t interpreter_state = -1;

std::vector<String> getScriptsOptionsList()
{
    std::vector<String> result;
    const char *folders[] = {"/scripts", "/KITENScripts", "/KITENJS"};
    const char *exts[]    = {".js", ".bjs"};

    if (kitenSdBegin()) {
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
                    for (const char *ext : exts) {
                        if (lower.endsWith(ext)) {
                            String full = String(folder) + "/" + name;
                            result.push_back(full);
                            break;
                        }
                    }
                }
                entry = root.openNextFile();
            }
            root.close();
        }
    }
    return result;
}

static void showScriptPreview(const String &path)
{
    
    String content = kitenSdReadFile(path, 2048);

    
    M5.Display.fillScreen(kitenConfig.bgColor);
    drawMainBorder(false);

    
    M5.Display.setTextSize(FP);
    M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
    M5.Display.setCursor(4, 30);
    int slash = path.lastIndexOf('/');
    String fname = (slash >= 0) ? path.substring(slash + 1) : path;
    M5.Display.print("Script: ");
    M5.Display.println(fname);

    M5.Display.setTextColor(TFT_WHITE, kitenConfig.bgColor);
    int y = 30 + LH * 2;
    int lineCount = 0;
    int maxLines = 11;
    String line;
    for (int i = 0; i < (int)content.length() && lineCount < maxLines; i++) {
        char c = content[i];
        if (c == '\r') continue;
        if (c == '\n') {
            M5.Display.setCursor(4, y);
            
            if (line.length() > 38) line = line.substring(0, 37) + "$";
            M5.Display.println(line);
            line = "";
            y += LH;
            lineCount++;
        } else {
            line += c;
        }
    }
    if (line.length() > 0 && lineCount < maxLines) {
        if (line.length() > 38) line = line.substring(0, 37) + "$";
        M5.Display.setCursor(4, y);
        M5.Display.println(line);
    }

    
    delay(300);
    waitAllKeysReleased();
    displayInfo(
        "JS script loaded.\n"
        "Running requires\n"
        "mQuickJS engine:\n"
        "  mquickjs=...\n"
        "in platformio.ini\n"
        "+ interpreter.cpp\n"
        "(322 lines) +\n"
        "display_js.cpp,\n"
        "keyboard_js.cpp,\n"
        "etc. (~4,000 lines\n"
        "total). Coming in a\n"
        "future KITEN build.",
        true);
}

void run_bjs_script()
{
    if (!kitenSdBegin()) {
        displayError("SD card required", true);
        return;
    }

    std::vector<String> files = getScriptsOptionsList();
    if (files.empty()) {
        displayInfo(
            "No .js / .bjs files\n"
            "found.\n\n"
            "Place scripts in\n"
            "/scripts, /KITENScripts,\n"
            "or /KITENJS on the SD\n"
            "card.",
            true);
        return;
    }

    
    std::vector<Option> opts;
    for (const String &f : files) {
        int slash = f.lastIndexOf('/');
        String fname = (slash >= 0) ? f.substring(slash + 1) : f;
        opts.push_back({fname, [f]() { showScriptPreview(f); }});
    }
    opts.push_back({"Back", [](){}});
    loopOptions(opts, MENU_TYPE_REGULAR, "Load script");
}

void scriptsMenuEntry()
{
    
    
    if (interpreter_state >= 0) {
        interpreter_state = 1;
        return;
    }

    std::vector<Option> opts;

    
    std::vector<String> files = getScriptsOptionsList();
    for (const String &f : files) {
        int slash = f.lastIndexOf('/');
        String fname = (slash >= 0) ? f.substring(slash + 1) : f;
        opts.push_back({fname, [f]() { showScriptPreview(f); }});
    }

    
    opts.push_back({"Load...", []() { run_bjs_script(); }});

    
    opts.push_back({"Main Menu", [](){}});

    loopOptions(opts, MENU_TYPE_SUBMENU, "Scripts");
}
