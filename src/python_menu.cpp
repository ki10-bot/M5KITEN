

#include "python_menu.h"
#include "py_interpreter.h"
#include "py_module_loader.h"
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

#define PY_STATUS_BAR_H     26      
#define PY_FOOTER_H         12      
#define PY_LINE_H           LH      
#define PY_MAX_LINES        ((SCREEN_HEIGHT - PY_STATUS_BAR_H - PY_FOOTER_H) / PY_LINE_H)
#define PY_MAX_BUFFER_LINES 200     
#define PY_CHARS_PER_LINE   (SCREEN_WIDTH / LW)   

static std::vector<String> g_consoleLines;
static size_t g_consoleScroll = 0;     
static bool   g_consoleDirty  = true;
static String g_currentInput;          
static String g_currentPrompt;         
static bool   g_inputActive = false;   
static bool   g_inputDone   = false;
static String g_inputResult;
static unsigned long g_lastCaretBlink = 0;
static bool g_caretOn = false;
static bool g_abortRequested = false;
static String g_currentScript = "";

void pyConsolePrint(const String &s) {
    
    String line;
    for (char c : s) {
        if (c == '\n') {
            g_consoleLines.push_back(line);
            line = "";
        } else if (c == '\r') {
            
        } else {
            line += c;
        }
    }
    if (line.length() > 0) g_consoleLines.push_back(line);
    while (g_consoleLines.size() > PY_MAX_BUFFER_LINES) g_consoleLines.erase(g_consoleLines.begin());
    g_consoleDirty = true;
}

class KitPyConsole : public IPyConsole {
public:
    void pyPrint(const String &s) override {
        pyConsolePrint(s);
        render();
    }

    String pyInput(const String &prompt) override {
        g_currentPrompt = prompt;
        g_currentInput = "";
        g_inputActive  = true;
        g_inputDone    = false;
        g_consoleDirty = true;
        
        render();

        
        
        
        
        
        
        
        
        
        

        
        while (!g_inputDone) {
            M5.update();
            M5Cardputer.update();

            
            bool hasChange = M5Cardputer.Keyboard.isChange();
            bool isPressed = M5Cardputer.Keyboard.isPressed();

            if (hasChange && isPressed) {
                auto st = M5Cardputer.Keyboard.keysState();

                
                if (st.enter) {
                    g_inputResult = g_currentInput;
                    g_inputActive = false;
                    g_inputDone = true;
                    
                    pyConsolePrint(prompt + g_currentInput);
                    break;
                }
                
                if (st.del) {
                    if (g_currentInput.length() > 0) {
                        g_currentInput.remove(g_currentInput.length() - 1);
                        g_consoleDirty = true;
                    } else {
                        
                        g_abortRequested = true;
                        g_inputActive = false;
                        g_inputDone = true;
                        g_inputResult = "";
                        break;
                    }
                }
                
                
                
                for (char c : st.word) {
                    if (c >= 0x20 && c < 0x7F) {
                        g_currentInput += c;
                        g_consoleDirty = true;
                    }
                }
                
                if (st.space) {
                    g_currentInput += ' ';
                    g_consoleDirty = true;
                }
                
                if (st.tab) {
                    g_currentInput += '\t';
                    g_consoleDirty = true;
                }
                
                if (st.opt) {
                    g_abortRequested = true;
                    g_inputActive = false;
                    g_inputDone = true;
                    g_inputResult = "";
                    break;
                }
            }

            
            if (M5.BtnPWR.wasPressed()) {
                g_abortRequested = true;
                g_inputActive = false;
                g_inputDone = true;
                g_inputResult = "";
                break;
            }

            
            if (millis() - g_lastCaretBlink > 500) {
                g_caretOn = !g_caretOn;
                g_lastCaretBlink = millis();
                g_consoleDirty = true;
            }

            if (g_consoleDirty) render();
            delay(5);
        }
        g_inputActive = false;
        g_consoleDirty = true;
        delay(120);  
        return g_inputResult;
    }

    bool pyTick() override {
        M5.update();
        pollKeyboard();
        if (check(EscPress)) {
            g_abortRequested = true;
            return false;
        }
        
        if (check(UpPress)) {
            if (g_consoleScroll < g_consoleLines.size()) g_consoleScroll++;
            g_consoleDirty = true;
        }
        if (check(DownPress)) {
            if (g_consoleScroll > 0) g_consoleScroll--;
            g_consoleDirty = true;
        }
        if (g_consoleDirty) render();
        
        if (millis() - g_lastCaretBlink > 500) {
            g_caretOn = !g_caretOn;
            g_lastCaretBlink = millis();
        }
        return !g_abortRequested;
    }

    void render() {
        if (!g_consoleDirty) {
            
            drawStatusBar();
            return;
        }
        g_consoleDirty = false;

        
        drawStatusBar();

        
        int bodyY = PY_STATUS_BAR_H;
        int bodyH = SCREEN_HEIGHT - PY_STATUS_BAR_H - PY_FOOTER_H;
        M5.Display.fillRect(0, bodyY, SCREEN_WIDTH, bodyH, kitenConfig.bgColor);

        
        size_t total = g_consoleLines.size();
        size_t visible = (total < PY_MAX_LINES) ? total : PY_MAX_LINES;
        size_t startIdx = (total > visible) ? total - visible : 0;
        
        if (g_consoleScroll > 0) {
            if (g_consoleScroll > startIdx) g_consoleScroll = startIdx;
            startIdx -= g_consoleScroll;
        }

        M5.Display.setTextSize(FP);
        M5.Display.setTextColor(TFT_WHITE, kitenConfig.bgColor);
        for (size_t i = 0; i < visible; i++) {
            size_t lineIdx = startIdx + i;
            if (lineIdx >= total) break;
            int y = bodyY + i * PY_LINE_H;
            String line = g_consoleLines[lineIdx];
            
            if (line.length() > PY_CHARS_PER_LINE) {
                line = line.substring(0, PY_CHARS_PER_LINE - 1) + "$";
            }
            M5.Display.setCursor(0, y);
            M5.Display.print(line);
        }

        
        if (g_inputActive) {
            int y = bodyY + (visible)*PY_LINE_H;
            if (y + PY_LINE_H > SCREEN_HEIGHT - PY_FOOTER_H) {
                
                y = SCREEN_HEIGHT - PY_FOOTER_H - PY_LINE_H;
            }
            
            String displayLine = g_currentPrompt + g_currentInput;
            if (displayLine.length() > PY_CHARS_PER_LINE - 1) {
                
                int excess = displayLine.length() - (PY_CHARS_PER_LINE - 1);
                displayLine = displayLine.substring(excess);
            }
            M5.Display.setCursor(0, y);
            M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
            M5.Display.print(displayLine);
            if (g_caretOn) {
                int cx = displayLine.length() * LW;
                M5.Display.fillRect(cx, y, LW, PY_LINE_H, kitenConfig.priColor);
            }
            M5.Display.setTextColor(TFT_WHITE, kitenConfig.bgColor);
        }

        
        int footY = SCREEN_HEIGHT - PY_FOOTER_H;
        M5.Display.fillRect(0, footY, SCREEN_WIDTH, PY_FOOTER_H, kitenConfig.bgColor);
        M5.Display.setTextSize(FP);
        M5.Display.setTextColor(TFT_DARKGREY, kitenConfig.bgColor);
        M5.Display.setCursor(0, footY + 2);
        if (g_inputActive) {
            M5.Display.print("ENTER: submit  ESC: abort");
        } else if (g_abortRequested) {
            M5.Display.setTextColor(TFT_RED, kitenConfig.bgColor);
            M5.Display.print("Aborting... ");
        } else {
            M5.Display.print("ESC: stop  ,/: scroll");
        }
    }
};

std::vector<String> getPythonScriptsList()
{
    std::vector<String> result;
    if (!kitenSdBegin()) return result;

    const char *folders[] = {"/python", "/Python", "/py", "/KITENScripts"};
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
                if (lower.endsWith(".py")) {
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

static void runPythonFile(const String &path)
{
    if (!kitenSdBegin()) {
        displayError("SD card required", true);
        return;
    }

    String src = kitenSdReadFile(path, 65536);
    if (src.length() == 0) {
        displayError("Could not read script", true);
        return;
    }

    
    int slash = path.lastIndexOf('/');
    String fname = (slash >= 0) ? path.substring(slash + 1) : path;
    g_currentScript = fname;

    
    g_consoleLines.clear();
    g_consoleScroll = 0;
    g_consoleDirty = true;
    g_abortRequested = false;
    g_currentInput = "";
    g_currentPrompt = "";
    g_inputActive = false;

    
    pyConsolePrint(">>> Running " + fname + " <<<");
    pyConsolePrint("");

    
    KitPyConsole console;
    PyInterp interp(&console, pyLoadModule);

    
    M5.Display.fillScreen(kitenConfig.bgColor);
    console.render();

    unsigned long startMs = millis();
    bool ok = interp.exec(src, fname);
    unsigned long elapsed = millis() - startMs;

    if (!ok) {
        pyConsolePrint("");
        pyConsolePrint("ERROR: " + interp.lastError());
    }
    pyConsolePrint("");
    String doneMsg = (g_abortRequested ? "[aborted]" : "[done]") +
                    String(" in ") + String(elapsed) + " ms";
    pyConsolePrint(doneMsg);
    pyConsolePrint("");
    pyConsolePrint("Press ESC to return.");

    g_consoleDirty = true;
    console.render();

    
    waitAllKeysReleased();
    while (true) {
        M5.update();
        pollKeyboard();
        if (check(EscPress)) break;
        
        if (check(UpPress) && g_consoleScroll < g_consoleLines.size()) {
            g_consoleScroll++;
            g_consoleDirty = true;
            console.render();
        }
        if (check(DownPress) && g_consoleScroll > 0) {
            g_consoleScroll--;
            g_consoleDirty = true;
            console.render();
        }
        delay(20);
    }
    g_currentScript = "";
}

static void runPythonREPL()
{
    
    g_consoleLines.clear();
    g_consoleScroll = 0;
    g_consoleDirty = true;
    g_abortRequested = false;
    g_currentInput = "";
    g_currentPrompt = "";
    g_inputActive = false;
    g_currentScript = "<REPL>";

    pyConsolePrint("KITEN Python REPL");
    pyConsolePrint("Type 'exit()' to quit.");
    pyConsolePrint("");

    KitPyConsole console;
    PyInterp interp(&console, pyLoadModule);

    M5.Display.fillScreen(kitenConfig.bgColor);
    console.render();

    
    for (;;) {
        if (g_abortRequested) break;
        
        String line = console.pyInput(">>> ");
        if (g_abortRequested) break;
        if (line == "exit()" || line == "exit" || line == "quit") break;
        if (line.length() == 0) continue;

        
        
        bool isStmt = false;
        String trimmed = line;
        trimmed.trim();
        const char *stmtStarters[] = {
            "if ","for ","while ","def ","return ","import ","from ",
            "class ","break","continue","pass","global ","del ","elif","else",
            "try","except","raise","with","yield","async","lambda "
        };
        for (const char *s : stmtStarters) {
            if (trimmed.startsWith(s)) { isStmt = true; break; }
        }
        
        if (!isStmt && line.indexOf('=') >= 0 && line.indexOf("==") < 0 &&
            line.indexOf("<=") < 0 && line.indexOf(">=") < 0 && line.indexOf("!=") < 0) {
            isStmt = true;
        }

        if (isStmt) {
            if (!interp.exec(line)) {
                pyConsolePrint("ERROR: " + interp.lastError());
            }
        } else {
            
            String wrapped = "__repl_result = (" + line + ")";
            if (interp.exec(wrapped)) {
                PyValue v = interp.getGlobal("__repl_result");
                if (v.type != PyType::None) {
                    pyConsolePrint(v.repr());
                }
            } else {
                pyConsolePrint("ERROR: " + interp.lastError());
            }
        }
        g_consoleDirty = true;
        console.render();
    }

    pyConsolePrint("");
    pyConsolePrint("REPL exited.");
    g_consoleDirty = true;
    console.render();
    waitAllKeysReleased();
    while (!check(EscPress)) {
        M5.update(); pollKeyboard();
        delay(20);
    }
    g_currentScript = "";
}

void run_py_script()
{
    if (!kitenSdBegin()) {
        displayError("SD card required", true);
        return;
    }
    std::vector<String> files = getPythonScriptsList();
    if (files.empty()) {
        displayInfo(
            "No .py files found.\n\n"
            "Place scripts in /python\n"
            "on the SD card.",
            true);
        return;
    }
    std::vector<Option> opts;
    for (const String &f : files) {
        int slash = f.lastIndexOf('/');
        String fname = (slash >= 0) ? f.substring(slash + 1) : f;
        opts.push_back({fname, [f]() { pythonScriptContextMenu(f); }});
    }
    
    
    opts.push_back({"REPL", []() { runPythonREPL(); }});
    opts.push_back({"Back", [](){}});
    loopOptions(opts, MENU_TYPE_REGULAR, "Python");
}

void pythonScriptContextMenu(const String &path)
{
    int slash = path.lastIndexOf('/');
    String fname = (slash >= 0) ? path.substring(slash + 1) : path;

    std::vector<Option> opts = {
        {"Run",    [path]() { runPythonFile(path); }},
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

void pythonMenuEntry()
{
    run_py_script();
}
