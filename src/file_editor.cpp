

#include "file_editor.h"
#include "config.h"
#include "settings.h"
#include "keyboard.h"
#include "ui.h"
#include "sd_helper.h"
#include <Arduino.h>
#include <M5Unified.h>
#include <M5Cardputer.h>

#define ED_HEADER_H    12
#define ED_FOOTER_H    22
#define ED_LINE_H      LH
#define ED_BODY_Y      ED_HEADER_H
#define ED_BODY_H      (SCREEN_HEIGHT - ED_HEADER_H - ED_FOOTER_H)
#define ED_MAX_LINES   (ED_BODY_H / ED_LINE_H)
#define ED_CHARS_PER_LINE ((SCREEN_WIDTH - 4) / LW)
#define ED_MAX_FILE    16384   

static int lineStartOffset(const String &buf, int caret)
{
    if (caret <= 0) return 0;
    int idx = caret;
    while (idx > 0 && buf[idx - 1] != '\n') idx--;
    return idx;
}

static int lineEndOffset(const String &buf, int caret)
{
    int idx = caret;
    while (idx < (int)buf.length() && buf[idx] != '\n') idx++;
    return idx;
}

static int totalLines(const String &buf)
{
    if (buf.length() == 0) return 1;
    int n = 1;
    for (char c : buf) if (c == '\n') n++;
    return n;
}

static int currentLineNum(const String &buf, int caret)
{
    int n = 1;
    for (int i = 0; i < caret && i < (int)buf.length(); i++) {
        if (buf[i] == '\n') n++;
    }
    return n;
}

static int caretUp(const String &buf, int caret)
{
    int ls = lineStartOffset(buf, caret);
    if (ls == 0) return 0;  
    
    
    int prevEnd = ls - 1;   
    int prevStart = lineStartOffset(buf, prevEnd);
    int col = caret - ls;
    int newOffset = prevStart + col;
    if (newOffset > prevEnd) newOffset = prevEnd;
    return newOffset;
}

static int caretDown(const String &buf, int caret)
{
    int ls = lineStartOffset(buf, caret);
    int le = lineEndOffset(buf, caret);
    if (le >= (int)buf.length()) return le;  
    int nextStart = le + 1;   
    int nextEnd   = lineEndOffset(buf, nextStart);
    int col = caret - ls;
    int newOffset = nextStart + col;
    if (newOffset > nextEnd) newOffset = nextEnd;
    return newOffset;
}

static void editorRender(const String &buf, int caret, const String &path)
{
    
    M5.Display.fillRect(0, 0, SCREEN_WIDTH, ED_HEADER_H, kitenConfig.priColor);
    M5.Display.fillRect(0, 0, SCREEN_WIDTH, ED_HEADER_H - 1, kitenConfig.bgColor);
    M5.Display.setTextSize(FP);
    M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
    M5.Display.setCursor(2, 2);
    String header = path;
    if (header.length() > ED_CHARS_PER_LINE - 6) {
        header = "..." + header.substring(header.length() - (ED_CHARS_PER_LINE - 9));
    }
    M5.Display.print(header);
    
    int ln = currentLineNum(buf, caret);
    int total = totalLines(buf);
    String pos = String(ln) + "/" + String(total);
    int posX = SCREEN_WIDTH - pos.length() * LW - 2;
    M5.Display.setCursor(posX, 2);
    M5.Display.print(pos);

    
    M5.Display.fillRect(0, ED_BODY_Y, SCREEN_WIDTH, ED_BODY_H, kitenConfig.bgColor);

    int curLine = currentLineNum(buf, caret);
    int firstLine = curLine - ED_MAX_LINES / 2;
    if (firstLine < 1) firstLine = 1;

    
    int lineNum = 1;
    int idx = 0;
    while (idx <= (int)buf.length() && lineNum < firstLine) {
        if (idx < (int)buf.length() && buf[idx] == '\n') lineNum++;
        idx++;
    }
    
    if (idx > (int)buf.length()) idx = buf.length();

    for (int row = 0; row < ED_MAX_LINES; row++) {
        int lineNo = firstLine + row;
        if (lineNo > totalLines(buf) && buf.length() > 0) break;

        int y = ED_BODY_Y + row * ED_LINE_H;
        
        M5.Display.setTextSize(FP);
        M5.Display.setTextColor(TFT_DARKGREY, kitenConfig.bgColor);
        char gutter[8];
        snprintf(gutter, sizeof(gutter), "%3d ", lineNo);
        M5.Display.setCursor(0, y);
        M5.Display.print(gutter);

        
        M5.Display.setTextColor(TFT_WHITE, kitenConfig.bgColor);
        
        String line;
        while (idx < (int)buf.length() && buf[idx] != '\n') {
            line += buf[idx];
            idx++;
        }
        
        if (idx < (int)buf.length() && buf[idx] == '\n') idx++;

        
        if (line.length() > (unsigned)(ED_CHARS_PER_LINE - 4)) {
            line = line.substring(0, ED_CHARS_PER_LINE - 5) + "$";
        }
        M5.Display.setCursor(4 * LW, y);
        M5.Display.print(line);

        
        if (lineNo == curLine) {
            int col = caret - lineStartOffset(buf, caret);
            int cx = 4 * LW + col * LW;
            
            if ((millis() / 500) % 2 == 0) {
                M5.Display.fillRect(cx, y, LW, ED_LINE_H, kitenConfig.priColor);
            }
        }
    }

    
    int footY = SCREEN_HEIGHT - ED_FOOTER_H;
    M5.Display.fillRect(0, footY, SCREEN_WIDTH, ED_FOOTER_H, kitenConfig.bgColor);
    M5.Display.drawFastHLine(0, footY, SCREEN_WIDTH, kitenConfig.priColor);
    M5.Display.setTextSize(FP);
    M5.Display.setTextColor(TFT_DARKGREY, kitenConfig.bgColor);
    M5.Display.setCursor(2, footY + 2);
    M5.Display.print("ENT:newl BS:del ,/:updn ;':lr");
    M5.Display.setCursor(2, footY + 12);
    M5.Display.print("OPT+ENT:save OPT+BS:cancel");
}

bool fileEditor(const String &path)
{
    if (!kitenSdBegin()) {
        displayError("SD card required", true);
        return false;
    }
    if (!kitenSdExists(path)) {
        displayError("File not found", true);
        return false;
    }

    String buf = kitenSdReadFile(path, ED_MAX_FILE);
    if (buf.length() == 0) {
        
        buf = "";
    }

    int caret = 0;   
    bool saved = false;

    waitAllKeysReleased();

    for (;;) {
        editorRender(buf, caret, path);

        M5.update();
        M5Cardputer.update();

        bool hasChange = M5Cardputer.Keyboard.isChange();
        bool isPressed = M5Cardputer.Keyboard.isPressed();

        if (hasChange && isPressed) {
            auto st = M5Cardputer.Keyboard.keysState();

            
            if (st.opt && st.enter) {
                if (kitenSdWriteFile(path, buf)) {
                    saved = true;
                    displaySuccess("Saved " + path, true);
                } else {
                    displayError("Save failed", true);
                }
                break;
            }
            
            if (st.opt && st.del) {
                break;
            }
            
            if (st.enter) {
                buf = buf.substring(0, caret) + "\n" + buf.substring(caret);
                caret++;
                continue;
            }
            
            if (st.del) {
                if (caret > 0) {
                    buf.remove(caret - 1, 1);
                    caret--;
                }
                continue;
            }
            
            for (char c : st.word) {
                if (c == ';') {
                    if (caret > 0) caret--;
                } else if (c == '\'') {
                    if (caret < (int)buf.length()) caret++;
                } else if (c == ',') {
                    caret = caretUp(buf, caret);
                } else if (c == '.') {
                    caret = caretDown(buf, caret);
                } else if (c >= 0x20 && c < 0x7F) {
                    
                    if (buf.length() < ED_MAX_FILE) {
                        buf = buf.substring(0, caret) + String(c) + buf.substring(caret);
                        caret++;
                    }
                }
            }
            
            if (st.space) {
                if (buf.length() < ED_MAX_FILE) {
                    buf = buf.substring(0, caret) + " " + buf.substring(caret);
                    caret++;
                }
            }
            
            if (st.tab) {
                if (buf.length() + 2 <= ED_MAX_FILE) {
                    buf = buf.substring(0, caret) + "  " + buf.substring(caret);
                    caret += 2;
                }
            }
        }

        
        if (M5.BtnPWR.wasPressed()) {
            break;
        }

        delay(20);
    }

    waitAllKeysReleased();
    return saved;
}
