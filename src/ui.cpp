#include "ui.h"
#include "config.h"
#include "keyboard.h"
#include "wifi_menu.h"
#include <Arduino.h>
#include <M5Unified.h>
#include <time.h>

static unsigned long lastActivity = 0;
static bool          screenDimmed  = false;
static bool          screenOff     = false;
static uint8_t       userBrightness = 100;
static const uint16_t SCREEN_OFF_DELAY_MS = 5000;

char timeStr[16] = "--:--:--";

static uint16_t dimColor(uint16_t c, int factor);

void initUI()
{
    M5.Display.setRotation(kitenConfig.rotation);
    M5.Display.setBrightness(kitenConfig.bright);
    M5.Display.invertDisplay(kitenConfig.colorInverted);
    M5.Display.setTextFont(0);
    M5.Display.setTextSize(1);
    userBrightness = (uint8_t)kitenConfig.bright;
}

uint8_t getBattery()
{
    int lvl = M5.Power.getBatteryLevel();
    if (lvl < 0) return 0;
    if (lvl > 100) lvl = 100;
    return (uint8_t)lvl;
}

bool isCharging()
{
    return M5.Power.isCharging();
}

static void drawLightningBolt(int x, int y, uint16_t color)
{
    
    
    
    static const int8_t ptsX[] = {4, 0, 3, 1, 7, 3};
    static const int8_t ptsY[] = {0, 6, 6, 11, 4, 4};
    
    M5.Display.fillTriangle(x + ptsX[0], y + ptsY[0],
                            x + ptsX[1], y + ptsY[1],
                            x + ptsX[5], y + ptsY[5], color);
    M5.Display.fillTriangle(x + ptsX[1], y + ptsY[1],
                            x + ptsX[2], y + ptsY[2],
                            x + ptsX[5], y + ptsY[5], color);
    M5.Display.fillTriangle(x + ptsX[2], y + ptsY[2],
                            x + ptsX[3], y + ptsY[3],
                            x + ptsX[4], y + ptsY[4], color);
    M5.Display.fillTriangle(x + ptsX[2], y + ptsY[2],
                            x + ptsX[4], y + ptsY[4],
                            x + ptsX[5], y + ptsY[5], color);
}

void drawBatteryStatus(uint8_t pct)
{
    uint16_t color = (pct < 16) ? TFT_RED : kitenConfig.priColor;
    int bx = SCREEN_WIDTH - 42;
    int by = 7;
    M5.Display.drawRect(bx, by, 34, 17, color);
    
    M5.Display.fillRect(bx + 34, by + 5, 2, 7, color);

    
    String txt = String(pct) + "%";
    M5.Display.setTextColor(TFT_WHITE);   
    M5.Display.setTextSize(FP);
    
    int tw = txt.length() * LW;
    int tx = bx + (34 - tw) / 2;
    if (tx < bx + 2) tx = bx + 2;
    M5.Display.setCursor(tx, by + 3);
    M5.Display.print(txt);

    
    
    uint16_t barColor = color;
    if (pct == 0) barColor = kitenConfig.bgColor;
    int fillW = (30 * pct) / 100;
    M5.Display.fillRect(bx + 2, by + 2, fillW, 13, barColor);
    
    M5.Display.drawFastVLine(bx + 12, by + 2, 13, kitenConfig.bgColor);
    M5.Display.drawFastVLine(bx + 22, by + 2, 13, kitenConfig.bgColor);

    
    M5.Display.setCursor(tx, by + 3);
    M5.Display.print(txt);

    
}

static void drawWifiStatus()
{
    if (!wifiConnected) return;

    int32_t rssi = WiFi.RSSI();
    
    if (rssi < -100) rssi = -100;
    if (rssi > -50) rssi = -50;

    
    
    
    
    
    int arcs = 1;
    if (rssi >= -50) arcs = 4;
    else if (rssi >= -65) arcs = 3;
    else if (rssi >= -75) arcs = 2;

    
    int cx = SCREEN_WIDTH - 54;   
    int cy = 14;                 

    
    uint16_t wfColor = kitenConfig.priColor;

    
    float scale = 0.45f;
    int deltaY = (int)(scale * 16);   
    int dotRadius = max(2, (int)(scale * 4));  

    
    M5.Display.fillCircle(cx, cy + deltaY, dotRadius, wfColor);

    
    
    if (arcs >= 1) {
        M5.Display.drawArc(cx, cy + deltaY,
                           (int)(scale * 12), (int)(scale * 10),
                           220, 320, wfColor);
    }
    
    if (arcs >= 2) {
        M5.Display.drawArc(cx, cy + deltaY,
                           (int)(scale * 20), (int)(scale * 18),
                           220, 320, wfColor);
    }
    
    if (arcs >= 3) {
        M5.Display.drawArc(cx, cy + deltaY,
                           (int)(scale * 28), (int)(scale * 26),
                           220, 320, wfColor);
    }
    
    if (arcs >= 4) {
        M5.Display.drawArc(cx, cy + deltaY,
                           (int)(scale * 36), (int)(scale * 34),
                           220, 320, wfColor);
    }
}

static void drawCPUDonut()
{
    
    int cx = SCREEN_WIDTH - 72;   
    int cy = 14;                 
    int outerR = 7;              
    int innerR = 4;              

    
    
    uint32_t freeHeap = ESP.getFreeHeap();
    uint32_t totalHeap = 520 * 1024;  
    int loadPct = 100 - (int)((freeHeap * 100) / totalHeap);
    if (loadPct < 5) loadPct = 5;   
    if (loadPct > 98) loadPct = 98;

    
    uint16_t donutColor = kitenConfig.priColor;  

    
    M5.Display.drawArc(cx, cy, innerR, outerR, 0, 360, dimColor(kitenConfig.priColor, 80));

    
    
    int angle = (loadPct * 360) / 100;
    if (angle > 0) {
        M5.Display.fillArc(cx, cy, innerR, outerR, -90, -90 + angle, donutColor);
    }

    
    if (loadPct >= 70) {
        M5.Display.setTextColor(donutColor, kitenConfig.bgColor);
        M5.Display.setTextSize(FP);  
        M5.Display.setTextDatum(top_center);
        M5.Display.drawString(String(loadPct), cx, cy - 2);
        M5.Display.setTextDatum(top_left);
    }
}

void drawStatusBar()
{
    
    M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
    M5.Display.setTextSize(FP);
    M5.Display.setCursor(12, 12);

    if (kitenConfig.clockSet) {
        
        struct tm ti;
        if (getLocalTime(&ti, 0)) updateTimeStr(ti);
        
        M5.Display.fillRect(12, 12, 85, LH, kitenConfig.bgColor);
        M5.Display.setCursor(12, 12);
        M5.Display.print(timeStr);
    } else {
        M5.Display.fillRect(12, 12, 85, LH, kitenConfig.bgColor);
        M5.Display.setCursor(12, 12);
        M5.Display.print(String(FW_NAME) + " " + FW_VERSION);
    }

    
    drawCPUDonut();

    
    drawWifiStatus();

    
    uint8_t bat = getBattery();
    if (bat > 0 || isCharging()) drawBatteryStatus(bat);

    
    M5.Display.drawFastHLine(0, 28, SCREEN_WIDTH, kitenConfig.priColor);
}

void drawMainBorder(bool clear)
{
    if (clear) {
        M5.Display.drawPixel(0, 0, 0);
        M5.Display.fillScreen(kitenConfig.bgColor);
    }
    drawStatusBar();
}

void drawMainBorderWithTitle(const String &title, bool clear)
{
    drawMainBorder(clear);
    
    (void)title;
}

void displayRedStripe(const String &text, uint16_t fg, uint16_t bg)
{
    uint8_t size;
    if (fg == bg && fg == TFT_WHITE) fg = TFT_BLACK;
    if (text.length() * LW * FM < (SCREEN_WIDTH - 2 * FM * LW)) size = FM;
    else size = FP;

    M5.Display.drawPixel(0, 0, 0);
    M5.Display.fillRect(10, SCREEN_HEIGHT / 2 - 13, SCREEN_WIDTH - 20, 26, bg);
    M5.Display.drawRect(10, SCREEN_HEIGHT / 2 - 13, SCREEN_WIDTH - 20, 26, bg);
    M5.Display.setTextColor(fg, bg);
    if (size == FM) {
        M5.Display.setTextSize(FM);
        M5.Display.setTextDatum(top_center);
        M5.Display.drawString(text, SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 - 8);
    } else {
        M5.Display.setTextSize(FP);
        
        int totalW = text.length() * LW;
        if (totalW < SCREEN_WIDTH - 20) {
            M5.Display.setTextDatum(top_center);
            M5.Display.drawString(text, SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 - 4);
        } else {
            int mid = text.length() / 2;
            String a = text.substring(0, mid);
            String b = text.substring(mid);
            M5.Display.setTextDatum(top_center);
            M5.Display.drawString(a, SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 - 12);
            M5.Display.drawString(b, SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);
        }
    }
    M5.Display.setTextDatum(top_left);
}

void displayError  (const String &t, bool w){ displayRedStripe(t, TFT_WHITE, TFT_RED);        delay(200); if (w) { waitAllKeysReleased(); while (!check(AnyKeyPress)) { M5.update(); pollKeyboard(); delay(10); } waitAllKeysReleased(); } }
void displayWarning(const String &t, bool w){ displayRedStripe(t, TFT_BLACK, TFT_YELLOW);     delay(200); if (w) { waitAllKeysReleased(); while (!check(AnyKeyPress)) { M5.update(); pollKeyboard(); delay(10); } waitAllKeysReleased(); } }
void displayInfo   (const String &t, bool w){ displayRedStripe(t, TFT_WHITE, TFT_BLUE);       delay(200); if (w) { waitAllKeysReleased(); while (!check(AnyKeyPress)) { M5.update(); pollKeyboard(); delay(10); } waitAllKeysReleased(); } }
void displaySuccess(const String &t, bool w){ displayRedStripe(t, TFT_WHITE, TFT_DARKGREEN);  delay(200); if (w) { waitAllKeysReleased(); while (!check(AnyKeyPress)) { M5.update(); pollKeyboard(); delay(10); } waitAllKeysReleased(); } }

int8_t displayMessage(const String &message,
                      const char *leftBtn,
                      const char *centerBtn,
                      const char *rightBtn,
                      uint16_t color)
{
    drawMainBorder(true);

    
    M5.Display.setTextDatum(top_center);
    M5.Display.setTextSize(FM);
    M5.Display.setTextColor(color, kitenConfig.bgColor);
    int lineCount = 1;
    int lineStart = 0;
    int lineIdx = 0;
    int y0 = SCREEN_HEIGHT / 2 - 30;
    for (int i = 0; i <= message.length(); i++) {
        if (i == message.length() || message[i] == '\n') {
            String line = message.substring(lineStart, i);
            M5.Display.drawString(line, SCREEN_WIDTH / 2, y0 + lineIdx * (FM * LH + 2));
            lineStart = i + 1;
            lineIdx++;
        }
    }
    M5.Display.setTextDatum(top_left);

    
    const char *btns[3] = {leftBtn, centerBtn, rightBtn};
    int8_t sel = 0;
    
    while (sel < 3 && btns[sel] == nullptr) sel++;
    int8_t prevSel = -1;

    int btnY = SCREEN_HEIGHT - 25;
    int btnH = 20;
    int btnW = SCREEN_WIDTH / 3 - 10;

    delay(200); 
    for (;;) {
        M5.update();
        pollKeyboard();

        if (prevSel != sel) {
            
            for (int i = 0; i < 3; i++) {
                if (btns[i] == nullptr) continue;
                int bx = 5 + i * (SCREEN_WIDTH / 3);
                if (i == sel) {
                    M5.Display.fillRect(bx, btnY, btnW, btnH, color);
                    M5.Display.drawRect(bx, btnY, btnW, btnH, color);
                    M5.Display.setTextColor(TFT_BLACK, color);
                } else {
                    M5.Display.fillRect(bx, btnY, btnW, btnH, kitenConfig.bgColor);
                    M5.Display.drawRect(bx, btnY, btnW, btnH, color);
                    M5.Display.setTextColor(color, kitenConfig.bgColor);
                }
                M5.Display.setTextSize(FP);
                M5.Display.setTextDatum(top_center);
                M5.Display.drawString(String(btns[i]), bx + btnW / 2, btnY + 6);
                M5.Display.setTextDatum(top_left);
            }
            prevSel = sel;
        }

        if (check(PrevPress) || check(EscPress)) {
            int8_t old = sel;
            do {
                sel = (sel - 1 + 3) % 3;
            } while (btns[sel] == nullptr && sel != old);
            uiBeep(440, 30);
        }
        if (check(NextPress)) {
            int8_t old = sel;
            do {
                sel = (sel + 1) % 3;
            } while (btns[sel] == nullptr && sel != old);
            uiBeep(440, 30);
        }
        if (check(SelPress)) {
            uiBeep(880, 50);
            waitAllKeysReleased();
            return sel;
        }
        delay(5);
    }
}

OptCoord drawOptions(int index, const std::vector<String> &options,
                     int fgcolor, int selcolor, int bgcolor, bool firstRender)
{
    OptCoord coord;
    int menuSize = options.size();
    if (menuSize > MAX_MENU_SIZE) menuSize = MAX_MENU_SIZE;

    int32_t optionsTopY = SCREEN_HEIGHT / 2 - menuSize * (FM * 8 + 4) / 2 - 5;
    M5.Display.drawPixel(0, 0, bgcolor);

    
    
    M5.Display.fillRect(0, 29, SCREEN_WIDTH, SCREEN_HEIGHT - 29, bgcolor);
    M5.Display.fillRect(SCREEN_WIDTH * 0.10, optionsTopY,
                              SCREEN_WIDTH * 0.8,
                              (FM * 8 + 4) * menuSize + 10, bgcolor);
    M5.Display.drawRect(SCREEN_WIDTH * 0.10,
                              SCREEN_HEIGHT / 2 - menuSize * (FM * 8 + 4) / 2 - 5,
                              SCREEN_WIDTH * 0.8,
                              (FM * 8 + 4) * menuSize + 10, fgcolor);

    M5.Display.setTextColor(fgcolor, bgcolor);
    M5.Display.setTextSize(FM);
    M5.Display.setCursor(SCREEN_WIDTH * 0.10 + 5,
                          SCREEN_HEIGHT / 2 - menuSize * (FM * 8 + 4) / 2);

    int init = 0;
    int cont = 1;
    int totalMenuSize = options.size();
    if (index >= MAX_MENU_SIZE) init = index - MAX_MENU_SIZE + 1;

    for (int i = 0; i < totalMenuSize; i++) {
        if (i >= init) {
            
            
            
            bool isSel = (i == index);
            M5.Display.setTextColor(isSel ? selcolor : fgcolor, bgcolor);

            String text = "";
            if (i == index) {
                text += ">";
                coord.x = SCREEN_WIDTH * 0.10 + 5 + FM * LW;
                coord.y = M5.Display.getCursorY() + 4;
                coord.size = (SCREEN_WIDTH * 0.8 - 10) / (LW * FM) - 1;
                coord.fg = fgcolor;
                coord.bg = bgcolor;
            } else {
                text += " ";
            }
            text += options[i] + "              ";
            
            int maxChars = (SCREEN_WIDTH * 0.8 - 10) / (LW * FM) - 1;
            if (text.length() > maxChars) text = text.substring(0, maxChars);

            M5.Display.setCursor(SCREEN_WIDTH * 0.10 + 5, M5.Display.getCursorY() + 4);
            M5.Display.println(text);
            cont++;
        }
        if (cont > MAX_MENU_SIZE) break;
    }

    
    if (totalMenuSize > MAX_MENU_SIZE) {
        M5.Display.fillRect(SCREEN_WIDTH - 5, 30, 5, SCREEN_HEIGHT - 35, bgcolor);
        int thumbH = (SCREEN_HEIGHT - 35) * MAX_MENU_SIZE / totalMenuSize;
        int thumbY = 30 + (SCREEN_HEIGHT - 35) * (index - init) / totalMenuSize;
        M5.Display.fillRect(SCREEN_WIDTH - 5, thumbY, 5, thumbH, fgcolor);
    }

    return coord;
}

void drawSubmenu(int index, const std::vector<String> &options, const char *title,
                 int animOffset)
{
    drawStatusBar();

    int menuSize = options.size();

    
    
    
    
    
    
    
    
    
    M5.Display.fillRect(BORDER_PAD_X, 30,
                         SCREEN_WIDTH - 2 * BORDER_PAD_X,
                         SCREEN_HEIGHT - 30 - BORDER_PAD_X,
                         kitenConfig.bgColor);

    
    M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
    M5.Display.setTextSize(FP);
    M5.Display.drawPixel(0, 0, 0);
    M5.Display.setTextDatum(top_left);
    M5.Display.drawString(String(title), 12, 30);

    int middle      = 25 + (SCREEN_HEIGHT - 30) / 2;
    int middle_up   = middle - (SCREEN_HEIGHT - 42) / 3 - FM * LH / 2 + 4;
    int middle_down = middle + (SCREEN_HEIGHT - 42) / 3 - FM * LH / 2;

    
    middle      += animOffset;
    middle_up   += animOffset;
    middle_down += animOffset;

    
    int firstIndex = index - 1 >= 0 ? index - 1 : menuSize - 1;
    M5.Display.setTextSize(FM);
    M5.Display.setTextColor(kitenConfig.secColor, kitenConfig.bgColor);
    M5.Display.fillRect(6, middle_up, SCREEN_WIDTH - 12, 8 * FM, kitenConfig.bgColor);
    M5.Display.setTextDatum(top_center);
    M5.Display.drawString(options[firstIndex], SCREEN_WIDTH / 2, middle_up);

    
    int selectedTextSize = options[index].length() <= SCREEN_WIDTH / (LW * FG) - 1 ? FG : FM;
    M5.Display.setTextSize(selectedTextSize);
    M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
    M5.Display.fillRect(6, middle - FG * LH / 2 - 1, SCREEN_WIDTH - 12, FG * LH + 5, kitenConfig.bgColor);
    M5.Display.drawString(options[index], SCREEN_WIDTH / 2, middle - selectedTextSize * LH / 2);

    
    int ulen = options[index].length() * selectedTextSize * LW;
    M5.Display.drawFastHLine(SCREEN_WIDTH / 2 - ulen / 2,
                              middle + selectedTextSize * LH / 2 + 1,
                              ulen, kitenConfig.priColor);

    
    int thirdIndex = index + 1 < menuSize ? index + 1 : 0;
    M5.Display.setTextSize(FM);
    M5.Display.setTextColor(kitenConfig.secColor, kitenConfig.bgColor);
    M5.Display.fillRect(6, middle_down, SCREEN_WIDTH - 12, 8 * FM, kitenConfig.bgColor);
    M5.Display.drawString(options[thirdIndex], SCREEN_WIDTH / 2, middle_down);

    M5.Display.setTextDatum(top_left);

    
    M5.Display.fillRect(SCREEN_WIDTH - 5, 0, 5, SCREEN_HEIGHT, kitenConfig.bgColor);
    int thumbH = SCREEN_HEIGHT / menuSize;
    int thumbY = index * SCREEN_HEIGHT / menuSize;
    M5.Display.fillRect(SCREEN_WIDTH - 5, thumbY, 5, thumbH, kitenConfig.priColor);
}

static int g_iconAreaH   = SCREEN_HEIGHT - 2 * BORDER_PAD_Y;
static int g_iconCenterX = SCREEN_WIDTH  / 2;
static int g_iconCenterY = SCREEN_HEIGHT / 2 + 3;  

static void drawClockIcon(float scale, uint16_t color, uint16_t bg)
{
    int radius      = (int)(scale * 30);
    int pointerSize = (int)(scale * 15);

    
    
    
    
    M5.Display.drawArc(g_iconCenterX, g_iconCenterY,
                       radius, (int)(1.1f * radius),
                       0, 360, color);

    
    M5.Display.fillCircle(g_iconCenterX, g_iconCenterY, radius / 10, color);
    
    M5.Display.drawLine(g_iconCenterX, g_iconCenterY,
                        g_iconCenterX - 2 * pointerSize / 3,
                        g_iconCenterY - 2 * pointerSize / 3, color);
    
    M5.Display.drawLine(g_iconCenterX, g_iconCenterY,
                        g_iconCenterX + pointerSize,
                        g_iconCenterY - pointerSize, color);

    (void)bg;  
}

static void drawDigitalClockDisplay(uint16_t color)
{
    int cx = g_iconCenterX;
    int cy = g_iconCenterY;

    
    struct tm ti;
    bool hasTime = getLocalTime(&ti, 0);
    
    
    String timeText;
    if (hasTime) {
        updateTimeStr(ti);
        timeText = String(timeStr);
    } else {
        timeText = "--:--:--";
    }

    
    
    M5.Display.setTextColor(color, kitenConfig.bgColor);
    M5.Display.setTextSize(FG);   
    M5.Display.setTextDatum(top_center);

    
    M5.Display.drawString(timeText, cx, cy - FG * LH / 2 + 2);

    
    M5.Display.setTextDatum(top_left);
    M5.Display.setTextSize(FP);
}

static void drawConfigIcon(float scale, uint16_t color, uint16_t bg)
{
    int radius = (int)(scale * 9);

    
    for (int i = 0; i < 6; i++) {
        M5.Display.fillArc(g_iconCenterX, g_iconCenterY,
                           (int)(2 * radius), (int)(3.5f * radius),
                           15 + 60 * i, 45 + 60 * i,
                           color);
    }
    
    M5.Display.drawArc(g_iconCenterX, g_iconCenterY,
                       radius, (int)(2.5f * radius),
                       0, 360, color);

    (void)bg;
}

static void drawMainMenuArrows(uint16_t color, uint16_t bg)
{
    int arrowSize = 10;

    int arrowX = BORDER_PAD_X + arrowSize + 2;
    int arrowY = g_iconCenterY;

    
    int ax = BORDER_PAD_X;
    int aw = g_iconCenterX - g_iconAreaH / 2 - ax;
    M5.Display.fillRect(ax, g_iconCenterY - g_iconAreaH / 2, aw, g_iconAreaH, bg);
    M5.Display.fillRect(SCREEN_WIDTH - ax - aw,
                        g_iconCenterY - g_iconAreaH / 2, aw, g_iconAreaH, bg);

    
    
    
    M5.Display.fillTriangle(arrowX,            arrowY,
                            arrowX + arrowSize, arrowY - arrowSize,
                            arrowX + arrowSize, arrowY + arrowSize, color);

    
    M5.Display.fillTriangle(SCREEN_WIDTH - arrowX,            arrowY,
                            SCREEN_WIDTH - arrowX - arrowSize, arrowY - arrowSize,
                            SCREEN_WIDTH - arrowX - arrowSize, arrowY + arrowSize, color);
}

static void drawWiFiIcon(float scale, uint16_t color, uint16_t bg)
{
    int deltaY = (int)(scale * 20);
    int radius = (int)(scale * 6);

    
    M5.Display.fillCircle(g_iconCenterX, g_iconCenterY + deltaY, radius, color);

    
    
    M5.Display.drawArc(g_iconCenterX, g_iconCenterY + deltaY,
                       deltaY + radius, deltaY,
                       220, 320, color);
    M5.Display.drawArc(g_iconCenterX, g_iconCenterY + deltaY,
                       2 * deltaY + radius, 2 * deltaY,
                       220, 320, color);

    (void)bg;
}

static void drawBLEIcon(float scale, uint16_t color, uint16_t bg)
{
    
    
    
    int lineWidth  = (int)(scale * 2);   
    int iconW      = (int)(scale * 20);  
    int iconH      = (int)(scale * 40);  
    int radius     = (int)(scale * 4);   
    int deltaR     = (int)(scale * 7);   

    if (iconW % 2 != 0) iconW++;
    if (iconH % 4 != 0) iconH += 4 - (iconH % 4);

    
    M5.Display.drawWideLine(
        g_iconCenterX, g_iconCenterY + iconH / 4,
        g_iconCenterX - iconW, g_iconCenterY - iconH / 4,
        lineWidth, color);
    M5.Display.drawWideLine(
        g_iconCenterX, g_iconCenterY - iconH / 4,
        g_iconCenterX - iconW, g_iconCenterY + iconH / 4,
        lineWidth, color);
    M5.Display.drawWideLine(
        g_iconCenterX, g_iconCenterY + iconH / 4,
        g_iconCenterX - iconW / 2, g_iconCenterY + iconH / 2,
        lineWidth, color);
    M5.Display.drawWideLine(
        g_iconCenterX, g_iconCenterY - iconH / 4,
        g_iconCenterX - iconW / 2, g_iconCenterY - iconH / 2,
        lineWidth, color);

    
    M5.Display.drawWideLine(
        g_iconCenterX - iconW / 2, g_iconCenterY - iconH / 2,
        g_iconCenterX - iconW / 2, g_iconCenterY + iconH / 2,
        lineWidth, color);

    
    
    M5.Display.drawArc(g_iconCenterX, g_iconCenterY,
                       (int)(2.5 * radius), 2 * radius,
                       300, 420, color);
    M5.Display.drawArc(g_iconCenterX, g_iconCenterY,
                       (int)(2.5 * radius + deltaR), 2 * radius + deltaR,
                       300, 420, color);
    M5.Display.drawArc(g_iconCenterX, g_iconCenterY,
                       (int)(2.5 * radius + 2 * deltaR), 2 * radius + 2 * deltaR,
                       300, 420, color);

    (void)bg;
}

static void drawIRIcon(float scale, uint16_t color, uint16_t bg)
{
    int iconSize    = (int)(scale * 40);
    int radius      = (int)(scale * 5);
    int deltaRadius = (int)(scale * 7);

    if (iconSize % 2 != 0) iconSize++;

    
    M5.Display.fillRect(
        g_iconCenterX - iconSize / 6, g_iconCenterY - iconSize / 2,
        iconSize / 6, iconSize, color);
    
    M5.Display.fillRect(
        g_iconCenterX - iconSize / 6 + 2, g_iconCenterY - iconSize / 3,
        iconSize / 6 - 4, iconSize / 6, bg);

    
    M5.Display.fillCircle(g_iconCenterX - iconSize / 6, g_iconCenterY - iconSize / 2, radius, color);

    
    
    M5.Display.drawArc(g_iconCenterX - iconSize / 6, g_iconCenterY - iconSize / 2,
                       (int)(2.5 * radius), 2 * radius,
                       140, 220, color);
    M5.Display.drawArc(g_iconCenterX - iconSize / 6, g_iconCenterY - iconSize / 2,
                       (int)(2.5 * radius + deltaRadius), 2 * radius + deltaRadius,
                       140, 220, color);
    M5.Display.drawArc(g_iconCenterX - iconSize / 6, g_iconCenterY - iconSize / 2,
                       (int)(2.5 * radius + 2 * deltaRadius), 2 * radius + 2 * deltaRadius,
                       140, 220, color);
    (void)bg;
}

static void drawRFIcon(float scale, uint16_t color, uint16_t bg)
{
    int radius      = (int)(scale * 4);
    int deltaRadius = (int)(scale * 7);
    int triangleSize = (int)(scale * 22);

    if (triangleSize % 2 != 0) triangleSize++;

    
    M5.Display.fillCircle(g_iconCenterX, g_iconCenterY - radius * 4, radius, color);
    
    M5.Display.fillTriangle(
        g_iconCenterX, g_iconCenterY - radius * 4 + radius,
        g_iconCenterX - triangleSize / 2, g_iconCenterY + triangleSize / 2,
        g_iconCenterX + triangleSize / 2, g_iconCenterY + triangleSize / 2,
        color);

    
    
    
    
    
    
    
    M5.Display.drawArc(g_iconCenterX, g_iconCenterY - radius * 4,
                       (int)(2.5 * radius), 2 * radius,
                       130, 230, color);
    M5.Display.drawArc(g_iconCenterX, g_iconCenterY - radius * 4,
                       (int)(2.5 * radius + deltaRadius), 2 * radius + deltaRadius,
                       130, 230, color);
    M5.Display.drawArc(g_iconCenterX, g_iconCenterY - radius * 4,
                       (int)(2.5 * radius + 2 * deltaRadius), 2 * radius + 2 * deltaRadius,
                       130, 230, color);

    
    M5.Display.drawArc(g_iconCenterX, g_iconCenterY - radius * 4,
                       (int)(2.5 * radius), 2 * radius,
                       310, 410, color);
    M5.Display.drawArc(g_iconCenterX, g_iconCenterY - radius * 4,
                       (int)(2.5 * radius + deltaRadius), 2 * radius + deltaRadius,
                       310, 410, color);
    M5.Display.drawArc(g_iconCenterX, g_iconCenterY - radius * 4,
                       (int)(2.5 * radius + 2 * deltaRadius), 2 * radius + 2 * deltaRadius,
                       310, 410, color);
    (void)bg;
}

static void drawRFIDIcon(float scale, uint16_t color, uint16_t bg)
{
    int iconSize    = (int)(scale * 50);
    int iconRadius  = (int)(scale * 4);
    int deltaRadius = (int)(scale * 6);

    if (iconSize % 2 != 0) iconSize++;

    
    M5.Display.drawRect(
        g_iconCenterX - iconSize / 2,
        g_iconCenterY - iconSize / 2,
        iconSize, iconSize, color);
    
    
    
    M5.Display.fillRect(
        g_iconCenterX, g_iconCenterY - iconSize / 2,
        iconSize / 2, iconSize / 2, bg);

    
    int chipX = g_iconCenterX - iconSize / 2 + deltaRadius;
    int chipY = g_iconCenterY + iconSize / 2 - deltaRadius;
    M5.Display.fillCircle(chipX, chipY, iconRadius, color);

    
    
    
    
    
    M5.Display.drawArc(chipX, chipY,
                       (int)(2.5 * iconRadius), 2 * iconRadius,
                       270, 360, color);
    M5.Display.drawArc(chipX, chipY,
                       (int)(2.5 * iconRadius + deltaRadius), 2 * iconRadius + deltaRadius,
                       270, 360, color);
    M5.Display.drawArc(chipX, chipY,
                       (int)(2.5 * iconRadius + 2 * deltaRadius), 2 * iconRadius + 2 * deltaRadius,
                       270, 360, color);
    (void)bg;
}

static void drawNRF24Icon(float scale, uint16_t color, uint16_t bg)
{
    
    int iconW = (int)(scale * 40);
    int iconH = (int)(scale * 30);
    if (iconW % 2 != 0) iconW++;
    if (iconH % 2 != 0) iconH++;

    int caseW = 3 * iconW / 4;
    int caseH = 2 * iconH / 3;
    int caseX = g_iconCenterX - iconW / 2;
    int caseY = g_iconCenterY - iconH / 6;

    int antW  = iconW / 8;
    if (antW < 1) antW = 1;
    int connR = iconH / 20;
    if (connR < 1) connR = 1;

    
    M5.Display.drawRect(caseX, caseY, caseW, caseH, color);

    
    M5.Display.fillRect(caseX + caseW, caseY + caseH / 2 - antW / 2, antW, antW, color);
    
    M5.Display.fillRect(
        caseX + caseW + antW,
        caseY + caseH - iconH,
        antW,
        iconH - caseH / 2 + antW / 2,
        color);

    
    M5.Display.fillCircle(caseX + caseW / 6, caseY + 1 * caseH / 5, connR, color);
    M5.Display.fillCircle(caseX + caseW / 6, caseY + 2 * caseH / 5, connR, color);
    M5.Display.fillCircle(caseX + caseW / 6, caseY + 3 * caseH / 5, connR, color);
    M5.Display.fillCircle(caseX + caseW / 6, caseY + 4 * caseH / 5, connR, color);
    M5.Display.fillCircle(caseX + caseW / 3, caseY + 1 * caseH / 5, connR, color);
    M5.Display.fillCircle(caseX + caseW / 3, caseY + 2 * caseH / 5, connR, color);
    M5.Display.fillCircle(caseX + caseW / 3, caseY + 3 * caseH / 5, connR, color);
    M5.Display.fillCircle(caseX + caseW / 3, caseY + 4 * caseH / 5, connR, color);

    
    M5.Display.fillRect(
        caseX + caseW - 2 * antW - connR,
        caseY + caseH / 2 - antW / 2,
        antW, antW, color);
    (void)bg;
}

static void drawConnectIcon(float scale, uint16_t color, uint16_t bg)
{
    int iconW = (int)(scale * 50);
    int iconH = (int)(scale * 40);
    int radius = (int)(scale * 7);
    if (iconW % 2 != 0) iconW++;
    if (iconH % 2 != 0) iconH++;
    if (radius < 2) radius = 2;

    
    int leftX = g_iconCenterX - iconW / 2;
    int leftY = g_iconCenterY;
    M5.Display.fillCircle(leftX, leftY, radius, color);

    
    int rX1 = g_iconCenterX + (int)(0.3f * iconW);
    int rY1 = g_iconCenterY - iconH / 2;
    int rX2 = g_iconCenterX + (int)(0.5f * iconW);
    int rY2 = g_iconCenterY;
    int rX3 = g_iconCenterX + (int)(0.3f * iconW);
    int rY3 = g_iconCenterY + iconH / 2;
    M5.Display.fillCircle(rX1, rY1, radius, color);
    M5.Display.fillCircle(rX2, rY2, radius, color);
    M5.Display.fillCircle(rX3, rY3, radius, color);

    
    M5.Display.drawLine(leftX, leftY, rX1, rY1, color);
    M5.Display.drawLine(leftX, leftY, rX2, rY2, color);
    M5.Display.drawLine(leftX, leftY, rX3, rY3, color);
    (void)bg;
}

static void drawFMIcon(float scale, uint16_t color, uint16_t bg)
{
    int iconW = (int)(scale * 40);
    int iconH = (int)(scale * 30);
    if (iconW % 2 != 0) iconW++;
    if (iconH % 2 != 0) iconH++;

    int caseH = 5 * iconH / 6;
    int caseX = g_iconCenterX - iconW / 2;
    int caseY = g_iconCenterY - iconH / 3;

    int btnY  = (2 * caseY + caseH + iconH / 10 + iconH / 3) / 2;
    int potX  = (2 * caseX + iconW + iconW / 10 + iconW / 2) / 2;
    int potY  = caseY + caseH / 3 + iconH / 10;
    int potR  = iconH / 7;
    if (potR < 1) potR = 1;
    int btnR  = iconH / 12;
    if (btnR < 1) btnR = 1;

    
    M5.Display.drawRect(caseX, caseY, iconW, caseH, color);

    
    M5.Display.fillCircle(potX, potY, potR, color);

    
    M5.Display.drawRect(caseX + iconW / 10, caseY + iconH / 10,
                        iconW / 2, iconH / 3, color);

    
    int antBaseX = caseX + iconW / 10;
    int antBaseY = caseY;
    int antTipX  = caseX + iconW / 10 + iconH / 3;
    int antTipY  = caseY - iconH / 6;
    M5.Display.drawLine(antBaseX, antBaseY, antTipX, antTipY, color);
    M5.Display.fillCircle(antTipX, antTipY, std::max(1, iconH / 30), color);

    
    M5.Display.fillCircle(caseX + iconW / 10 + iconH / 8, btnY, btnR, color);
    M5.Display.fillCircle(caseX + iconW / 10 + iconW / 2 - iconH / 8, btnY, btnR, color);
    (void)bg;
}

static void drawScriptsIcon(float scale, uint16_t color, uint16_t bg)
{
    int iconW = (int)(scale * 40);
    int iconH = (int)(scale * 60);
    if (iconW % 2 != 0) iconW++;
    if (iconH % 2 != 0) iconH++;

    int foldSize = iconH / 4;
    int arrowSize = iconW / 10;
    if (arrowSize < 1) arrowSize = 1;
    int arrowPadX = 2 * arrowSize;
    int arrowPadBottom = 3 * arrowPadX;
    int slashSize = 2 * arrowSize;

    
    M5.Display.drawRect(g_iconCenterX - iconW / 2, g_iconCenterY - iconH / 2,
                        iconW, iconH, color);
    
    M5.Display.fillRect(g_iconCenterX + iconW / 2 - foldSize,
                        g_iconCenterY - iconH / 2, foldSize, foldSize, bg);
    M5.Display.fillTriangle(
        g_iconCenterX + iconW / 2 - foldSize,
        g_iconCenterY - iconH / 2,
        g_iconCenterX + iconW / 2 - foldSize,
        g_iconCenterY - iconH / 2 + foldSize - 1,
        g_iconCenterX + iconW / 2 - 1,
        g_iconCenterY - iconH / 2 + foldSize - 1, color);

    
    M5.Display.drawLine(g_iconCenterX - iconW / 2 + arrowPadX,
                        g_iconCenterY + iconH / 2 - arrowPadBottom,
                        g_iconCenterX - iconW / 2 + arrowPadX + arrowSize,
                        g_iconCenterY + iconH / 2 - arrowPadBottom + arrowSize, color);
    M5.Display.drawLine(g_iconCenterX - iconW / 2 + arrowPadX,
                        g_iconCenterY + iconH / 2 - arrowPadBottom,
                        g_iconCenterX - iconW / 2 + arrowPadX + arrowSize,
                        g_iconCenterY + iconH / 2 - arrowPadBottom - arrowSize, color);

    
    M5.Display.drawLine(g_iconCenterX - slashSize / 2,
                        g_iconCenterY + iconH / 2 - arrowPadBottom + arrowSize,
                        g_iconCenterX + slashSize / 2,
                        g_iconCenterY + iconH / 2 - arrowPadBottom - arrowSize, color);

    
    M5.Display.drawLine(g_iconCenterX + iconW / 2 - arrowPadX,
                        g_iconCenterY + iconH / 2 - arrowPadBottom,
                        g_iconCenterX + iconW / 2 - arrowPadX - arrowSize,
                        g_iconCenterY + iconH / 2 - arrowPadBottom + arrowSize, color);
    M5.Display.drawLine(g_iconCenterX + iconW / 2 - arrowPadX,
                        g_iconCenterY + iconH / 2 - arrowPadBottom,
                        g_iconCenterX + iconW / 2 - arrowPadX - arrowSize,
                        g_iconCenterY + iconH / 2 - arrowPadBottom - arrowSize, color);
    (void)bg;
}

static void drawPythonIcon(float scale, uint16_t color, uint16_t bg)
{
    (void)color;   

    
    const uint16_t PY_YELLOW = 0xFEA7;   
    const uint16_t PY_BLUE   = 0x3353;   

    int w = (int)(scale * 36);   
    int h = (int)(scale * 16);   
    if (w % 2) w++;
    if (h % 2) h++;
    int gap = (int)(scale * 4);

    int cx = g_iconCenterX;
    int cy = g_iconCenterY;

    
    int topX = cx - w / 2;
    int topY = cy - h / 2 - gap / 2;
    M5.Display.fillRoundRect(topX, topY, w, h, h / 3, PY_YELLOW);

    
    
    int botX = cx - w / 2 + w / 4;
    int botY = cy + h / 2 - gap / 2;
    M5.Display.fillRoundRect(botX, botY, w, h, h / 3, PY_BLUE);

    
    int eyeR = std::max(1, h / 8);
    M5.Display.fillCircle(topX + w - h / 2 - 1, topY + h / 2, eyeR, bg);
    M5.Display.fillCircle(botX + h / 2 + 1,     botY + h / 2, eyeR, bg);
    (void)bg;
}

static void drawBadusbIcon(float scale, uint16_t color, uint16_t bg)
{
    int w = (int)(scale * 28);   
    int h = (int)(scale * 32);   
    if (w % 2) w++;
    if (h % 2) h++;

    int cx = g_iconCenterX;
    int cy = g_iconCenterY;

    int bodyW = w;
    int bodyH = (int)(h * 0.65);    
    int connH = h - bodyH;          
    int connW = (int)(bodyW * 0.7); 
    int bodyX = cx - bodyW / 2;
    int bodyY = cy + (connH - bodyH) / 2 + 2;
    int connX = cx - connW / 2;
    int connY = bodyY - connH;

    
    M5.Display.fillRect(connX, connY, connW, connH, color);

    
    int contactY = connY + connH / 2 - 1;
    int contactSpacing = connW / 5;
    for (int i = 0; i < 4; i++) {
        int cx2 = connX + contactSpacing * (i + 1);
        M5.Display.fillRect(cx2 - 1, contactY, 2, 3, bg);
    }

    
    M5.Display.fillRoundRect(bodyX, bodyY, bodyW, bodyH, 3, color);

    
    int skullR = (int)(scale * 3);
    int skullCx = cx;
    int skullCy = bodyY + bodyH / 2;
    M5.Display.fillCircle(skullCx, skullCy, skullR, bg);
    
    M5.Display.fillCircle(skullCx - skullR / 2, skullCy - skullR / 4, 1, color);
    M5.Display.fillCircle(skullCx + skullR / 2, skullCy - skullR / 4, 1, color);

    (void)bg;
}

static void drawWiFiScanIcon(float scale, uint16_t color, uint16_t bg)
{
    int cx = g_iconCenterX;
    int cy = g_iconCenterY;
    
    
    M5.Display.drawCircle(cx, cy, (int)(scale * 10), color);
    M5.Display.drawCircle(cx, cy, (int)(scale * 6), color);
    M5.Display.fillCircle(cx, cy, (int)(scale * 2), color);
    
    
    M5.Display.drawLine(cx, cy, cx + (int)(scale * 10), cy - (int)(scale * 5), color);
    
    (void)bg;
}

static void drawIPGenIcon(float scale, uint16_t color, uint16_t bg)
{
    int cx = g_iconCenterX;
    int cy = g_iconCenterY;
    int r = (int)(scale * 6);
    
    
    int dotR = (int)(scale * 3);
    int spacing = (int)(scale * 8);
    int offsetX = -(int)(scale * 4);  
    
    
    M5.Display.fillCircle(cx + offsetX - spacing/2, cy - spacing/2, dotR, color);  
    M5.Display.fillCircle(cx + offsetX + spacing/2, cy - spacing/2, dotR, color);  
    M5.Display.fillCircle(cx + offsetX - spacing/2, cy + spacing/2, dotR, color);  
    M5.Display.fillCircle(cx + offsetX + spacing/2, cy + spacing/2, dotR, color);  
    
    
    int lineOffset = dotR + 1;
    M5.Display.drawFastHLine(cx + offsetX - spacing/2 + lineOffset, cy - spacing/2,
                             spacing - 2*lineOffset, color);  
    M5.Display.drawFastHLine(cx + offsetX - spacing/2 + lineOffset, cy + spacing/2,
                             spacing - 2*lineOffset, color);  
    M5.Display.drawFastVLine(cx + offsetX - spacing/2, cy - spacing/2 + lineOffset,
                             spacing - 2*lineOffset, color);  
    M5.Display.drawFastVLine(cx + offsetX + spacing/2, cy - spacing/2 + lineOffset,
                             spacing - 2*lineOffset, color);  
    
    
    int portX = cx + (int)(scale * 14);
    
    
    M5.Display.fillCircle(portX, cy - (int)(scale * 3), (int)(scale * 1.5), color);
    M5.Display.fillCircle(portX, cy + (int)(scale * 3), (int)(scale * 1.5), color);
    
    
    int boxW = (int)(scale * 10);
    int boxH = (int)(scale * 12);
    M5.Display.drawRect(portX + (int)(scale * 4), cy - boxH/2, boxW, boxH, color);
    
    
    if (scale >= 0.8f) {
        M5.Display.setTextColor(color, bg);
        M5.Display.setTextSize(FP);
        M5.Display.setTextDatum(top_center);
        M5.Display.drawString("P", portX + (int)(scale * 4) + boxW/2, cy - 3);
        M5.Display.setTextDatum(top_left);
    }
    
    
    int sparkX = cx + (int)(scale * 18);
    int sparkY = cy - (int)(scale * 12);
    M5.Display.drawPixel(sparkX, sparkY, color);
    M5.Display.drawPixel(sparkX - 2, sparkY + 1, color);
    M5.Display.drawPixel(sparkX + 2, sparkY + 1, color);
    M5.Display.drawPixel(sparkX - 1, sparkY + 3, color);
    M5.Display.drawPixel(sparkX + 1, sparkY + 3, color);
}

static void drawRFIDCloneIcon(float scale, uint16_t color, uint16_t bg) {
    int cx = g_iconCenterX, cy = g_iconCenterY;
    M5.Display.fillCircle(cx, cy, 7*scale, color);
    M5.Display.fillCircle(cx, cy, 5*scale, kitenConfig.bgColor);
    M5.Display.drawLine(cx-6*scale, cy-3*scale, cx+6*scale, cy+3*scale, color);
    (void)bg;
}

static void drawGSMIcon(float scale, uint16_t color, uint16_t bg) {
    int cx = g_iconCenterX, cy = g_iconCenterY;
    M5.Display.fillCircle(cx, cy, 6*scale, color);
    M5.Display.fillCircle(cx, cy, 4*scale, bg);
    M5.Display.drawLine(cx-6*scale, cy-6*scale, cx-2*scale, cy-2*scale, color);
    M5.Display.drawLine(cx+6*scale, cy-6*scale, cx+2*scale, cy-2*scale, color);
    (void)bg;
}

static void drawMITMIcon(float scale, uint16_t color, uint16_t bg) {
    int cx = g_iconCenterX, cy = g_iconCenterY;
    
    M5.Display.fillCircle(cx-6*scale, cy, 3*scale, color);
    M5.Display.fillCircle(cx+6*scale, cy, 3*scale, color);
    M5.Display.drawLine(cx-3*scale, cy, cx+3*scale, cy, color);
    M5.Display.fillTriangle(cx, cy-2*scale, cx, cy+2*scale, cx+2*scale, cy, TFT_WHITE);
    (void)bg;
}

static void drawDDoSIcon(float scale, uint16_t color, uint16_t bg) {
    int cx = g_iconCenterX;
    int cy = g_iconCenterY;

    
    int pixelSize = (int)(scale * 5.0f + 0.5f);
    if (pixelSize < 1) pixelSize = 1;

    
    static const uint8_t skull[7][7] = {
        {0, 1, 1, 1, 1, 1, 0},
        {1, 1, 1, 1, 1, 1, 1},
        {1, 0, 0, 1, 0, 0, 1},
        {1, 0, 0, 1, 0, 0, 1},
        {1, 1, 1, 0, 1, 1, 1},
        {0, 1, 1, 1, 1, 1, 0},
        {0, 1, 0, 1, 0, 1, 0}
    };

    int w = 7;
    int h = 7;
    int totalW = w * pixelSize;
    int totalH = h * pixelSize;
    int startX = cx - totalW / 2;
    int startY = cy - totalH / 2;

    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            int x = startX + col * pixelSize;
            int y = startY + row * pixelSize;
            uint16_t c = skull[row][col] ? color : bg;
            M5.Display.fillRect(x, y, pixelSize, pixelSize, c);
        }
    }
}

static void drawWiFiDeauthIcon(float scale, uint16_t color, uint16_t bg) {
    int cx = g_iconCenterX, cy = g_iconCenterY;
    
    M5.Display.fillCircle(cx, cy, 6*scale, color);
    M5.Display.drawLine(cx-4*scale, cy-4*scale, cx+4*scale, cy+4*scale, bg);
    M5.Display.drawLine(cx+4*scale, cy-4*scale, cx-4*scale, cy+4*scale, bg);
}

static void drawBLEJammerIcon(float scale, uint16_t color, uint16_t bg) {
    int cx = g_iconCenterX, cy = g_iconCenterY;
    
    M5.Display.drawLine(cx-6*scale, cy-6*scale, cx+6*scale, cy+6*scale, color);
    M5.Display.drawLine(cx+6*scale, cy-6*scale, cx-6*scale, cy+6*scale, color);
    (void)bg;
}

static void drawEmailOsintIcon(float scale, uint16_t color, uint16_t bg) {
    int cx = g_iconCenterX;
    int cy = g_iconCenterY;

    
    int textSize = (int)(scale * 5.0f + 0.5f);
    if (textSize < 1) textSize = 1;

    M5.Display.setTextSize(textSize);
    M5.Display.setTextColor(color, bg);

    
    int charWidth  = 6 * textSize;
    int charHeight = 8 * textSize;

    
    int x = cx - charWidth / 2;
    int y = cy - charHeight / 2;

    M5.Display.setCursor(x, y);
    M5.Display.print("@");
}

static void drawUSBDropIcon(float scale, uint16_t color, uint16_t bg) {
    int cx = g_iconCenterX, cy = g_iconCenterY;
    
    M5.Display.fillRoundRect(cx - 8*scale, cy - 6*scale, 16*scale, 12*scale, 2*scale, color);
    M5.Display.fillRect(cx - 4*scale, cy - 10*scale, 8*scale, 4*scale, color);
    M5.Display.fillTriangle(cx, cy - 10*scale, cx + 4*scale, cy - 14*scale, cx - 4*scale, cy - 14*scale, color);
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(bg, color);
    M5.Display.setCursor(cx - 3*scale, cy - 2*scale);
    M5.Display.print(">");
    (void)bg;
}

static void drawOthersIcon(float scale, uint16_t color, uint16_t bg)
{
    int radius = (int)(scale * 7);

    
    M5.Display.fillCircle(g_iconCenterX, g_iconCenterY, radius, color);

    
    
    M5.Display.drawArc(g_iconCenterX, g_iconCenterY,
                       (int)(2.5f * radius), (int)(2.0f * radius),
                       0, 340, color);
    M5.Display.drawArc(g_iconCenterX, g_iconCenterY,
                       (int)(3.5f * radius), (int)(3.0f * radius),
                       20, 360, color);
    M5.Display.drawArc(g_iconCenterX, g_iconCenterY,
                       (int)(4.5f * radius), (int)(4.0f * radius),
                       0, 200, color);
    M5.Display.drawArc(g_iconCenterX, g_iconCenterY,
                       (int)(4.5f * radius), (int)(4.0f * radius),
                       240, 360, color);
    (void)bg;
}

static uint16_t dimColor(uint16_t c, int factor)
{
    
    uint16_t r = (c >> 11) & 0x1F;
    uint16_t g = (c >> 5)  & 0x3F;
    uint16_t b =  c        & 0x1F;
    r = (r * factor) >> 8;
    g = (g * factor) >> 8;
    b = (b * factor) >> 8;
    return (r << 11) | (g << 5) | b;
}

void drawMainMenu(int index, const std::vector<String> &labels, int animOffset)
{
    drawStatusBar();

    int menuSize = labels.size();
    if (menuSize == 0) return;

    
    
    
    
    
    M5.Display.fillRect(0, 29, SCREEN_WIDTH, SCREEN_HEIGHT - 29, kitenConfig.bgColor);

    
    
    
    uint16_t iconColor = kitenConfig.priColor;
    if (animOffset != 0) {
        int absOff = animOffset < 0 ? -animOffset : animOffset;
        int factor = 256 - (absOff * 20);   
        if (factor < 100) factor = 100;
        iconColor = dimColor(kitenConfig.priColor, factor);
    }

    
    
    drawMainMenuArrows(kitenConfig.priColor, kitenConfig.bgColor);

    
    
    int savedCenterX = g_iconCenterX;
    g_iconCenterX = savedCenterX + animOffset;

    
    
    String lbl = labels[index];
    lbl.trim();
    if (lbl.equalsIgnoreCase("Clock")) {
        
        drawDigitalClockDisplay(iconColor);
    } else if (lbl.equalsIgnoreCase("Config")) {
        drawConfigIcon(1.0f, iconColor, kitenConfig.bgColor);
    } else if (lbl.equalsIgnoreCase("WiFi")) {
        drawWiFiIcon(1.0f, iconColor, kitenConfig.bgColor);
    } else if (lbl.equalsIgnoreCase("BLE") || lbl.equalsIgnoreCase("Bluetooth")) {
        drawBLEIcon(1.0f, iconColor, kitenConfig.bgColor);
    } else if (lbl.equalsIgnoreCase("IR") || lbl.equalsIgnoreCase("Infrared")) {
        drawIRIcon(1.0f, iconColor, kitenConfig.bgColor);
    } else if (lbl.equalsIgnoreCase("RF") || lbl.equalsIgnoreCase("Radio")) {
        drawRFIcon(1.0f, iconColor, kitenConfig.bgColor);
    } else if (lbl.equalsIgnoreCase("RFID") || lbl.equalsIgnoreCase("NFC")) {
        drawRFIDIcon(1.0f, iconColor, kitenConfig.bgColor);
    } else if (lbl.equalsIgnoreCase("NRF24")) {
        drawNRF24Icon(1.0f, iconColor, kitenConfig.bgColor);
    } else if (lbl.equalsIgnoreCase("Connect") || lbl.equalsIgnoreCase("ESPNOW")) {
        drawConnectIcon(1.0f, iconColor, kitenConfig.bgColor);
    } else if (lbl.equalsIgnoreCase("FM")) {
        drawFMIcon(1.0f, iconColor, kitenConfig.bgColor);
    } else if (lbl.equalsIgnoreCase("Scripts") || lbl.equalsIgnoreCase("JS")) {
        drawScriptsIcon(1.0f, iconColor, kitenConfig.bgColor);
    } else if (lbl.equalsIgnoreCase("Python") || lbl.equalsIgnoreCase("Py")) {
        drawPythonIcon(1.0f, iconColor, kitenConfig.bgColor);
    } else if (lbl.equalsIgnoreCase("BadUSB") || lbl.equalsIgnoreCase("BadUSB & HID")) {
        drawBadusbIcon(1.0f, iconColor, kitenConfig.bgColor);
    } else if (lbl.equalsIgnoreCase("DDoS")) {
        drawDDoSIcon(1.0f, iconColor, kitenConfig.bgColor);  
    } else if (lbl.equalsIgnoreCase("BLE Jam") || lbl.equalsIgnoreCase("BLE Jammer")) {
        drawBLEJammerIcon(1.0f, iconColor, kitenConfig.bgColor);
    } else if (lbl.equalsIgnoreCase("EmailOSINT")) {
        drawEmailOsintIcon(1.0f, iconColor, kitenConfig.bgColor);
    } else if (lbl.equalsIgnoreCase("USB Drop")) {
        drawUSBDropIcon(1.0f, iconColor, kitenConfig.bgColor);
    } else if (lbl.equalsIgnoreCase("RFID Clone")) {
        drawRFIDCloneIcon(1.0f, iconColor, kitenConfig.bgColor);
    } else if (lbl.equalsIgnoreCase("MITM")) {
        drawMITMIcon(1.0f, iconColor, kitenConfig.bgColor);
    } else if (lbl.equalsIgnoreCase("GSM Flood")) {
        drawGSMIcon(1.0f, iconColor, kitenConfig.bgColor);
    } else if (lbl.equalsIgnoreCase("WiFiScan")) {
        drawWiFiScanIcon(1.0f, iconColor, kitenConfig.bgColor);
    } else if (lbl.equalsIgnoreCase("Target Deauth")) {
        drawWiFiDeauthIcon(1.0f, iconColor, kitenConfig.bgColor);
    } else if (lbl.equalsIgnoreCase("Others") || lbl.equalsIgnoreCase("Other")) {
        drawOthersIcon(1.0f, iconColor, kitenConfig.bgColor);
    } else if (lbl.equalsIgnoreCase("IP Gen") || lbl.equalsIgnoreCase("IP Generator") || 
               lbl.equalsIgnoreCase("IPGen") || lbl.equalsIgnoreCase("IP")) {
        drawIPGenIcon(1.0f, iconColor, kitenConfig.bgColor);
    } else {
        
        int r = 25;
        M5.Display.drawRect(g_iconCenterX - r, g_iconCenterY - r,
                                 2 * r, 2 * r, iconColor);
        M5.Display.setTextColor(iconColor, kitenConfig.bgColor);
        M5.Display.setTextSize(FG);
        M5.Display.setTextDatum(top_center);
        M5.Display.drawString(String(lbl.charAt(0)),
                              g_iconCenterX, g_iconCenterY - FG * LH / 2);
        M5.Display.setTextDatum(top_left);
    }

    g_iconCenterX = savedCenterX;   

    
    int titleY = g_iconCenterY + g_iconAreaH / 2 + 4;
    if (titleY > SCREEN_HEIGHT - LH * FM) titleY = SCREEN_HEIGHT - LH * FM - 2;
    M5.Display.fillRect(BORDER_PAD_X, titleY,
                        SCREEN_WIDTH - 2 * BORDER_PAD_X, LH * FM,
                        kitenConfig.bgColor);
    M5.Display.setTextSize(FM);
    M5.Display.setTextColor(iconColor, kitenConfig.bgColor);
    M5.Display.setTextDatum(top_center);
    M5.Display.drawString(labels[index], SCREEN_WIDTH / 2, titleY);
    M5.Display.setTextDatum(top_left);

    
    M5.Display.fillRect(SCREEN_WIDTH - 5, 29, 5, SCREEN_HEIGHT - 29,
                        kitenConfig.bgColor);
    int thumbH = (SCREEN_HEIGHT - 29) / menuSize;
    int thumbY = 29 + index * (SCREEN_HEIGHT - 29) / menuSize;
    M5.Display.fillRect(SCREEN_WIDTH - 5, thumbY, 5, thumbH, kitenConfig.priColor);
}

void animEnter()
{
    int cy = SCREEN_HEIGHT / 2;
    int maxD = cy;
    for (int step = 0; step <= ANIM_FADE_STEPS; step++) {
        int reach = (maxD * step) / ANIM_FADE_STEPS;
        
        if (step > 0) {
            int prev = (maxD * (step - 1)) / ANIM_FADE_STEPS;
            
            M5.Display.fillRect(0, cy - reach, SCREEN_WIDTH, reach - prev, kitenConfig.bgColor);
            
            M5.Display.fillRect(0, cy + prev, SCREEN_WIDTH, reach - prev, kitenConfig.bgColor);
        }
        delay(ANIM_FADE_STEP_MS);
    }
}

void animExit()
{
    int cy = SCREEN_HEIGHT / 2;
    int maxD = cy;
    for (int step = 0; step <= ANIM_FADE_STEPS; step++) {
        int reach = (maxD * step) / ANIM_FADE_STEPS;
        
        if (step > 0) {
            int prev = (maxD * (step - 1)) / ANIM_FADE_STEPS;
            M5.Display.fillRect(0, 0, SCREEN_WIDTH, reach - prev, kitenConfig.bgColor);
            M5.Display.fillRect(0, SCREEN_HEIGHT - reach, SCREEN_WIDTH, reach - prev, kitenConfig.bgColor);
        }
        delay(ANIM_FADE_STEP_MS);
    }
}

void uiBeep(uint16_t freq, uint16_t dur)
{
    if (!kitenConfig.soundEnabled) return;
    uint8_t vol = (uint8_t)((kitenConfig.soundVolume * 32) / 100);
    M5.Speaker.setVolume(vol);
    M5.Speaker.tone(freq, dur);
}

void applyBrightness(uint8_t pct, bool instant)
{
    if (instant) {
        M5.Display.setBrightness(pct);
        return;
    }
    
    static uint8_t current = 100;
    if (pct == current) return;
    int step = (pct > current) ? 5 : -5;
    while (current != pct) {
        int delta = pct - current;
        if (abs(delta) < 5) current = pct;
        else current += step;
        M5.Display.setBrightness(current);
        delay(5);
    }
}

void checkPowerSaveTime()
{
    
    
    
    if (kitenConfig.sleepDisabled) return;

    if (kitenConfig.dimmerSet == 0) return; 

    unsigned long now = millis();
    unsigned long idle = now - lastActivity;
    unsigned long dimMs = (unsigned long)kitenConfig.dimmerSet * 1000UL;

    if (!screenDimmed && idle > dimMs) {
        
        uint8_t dim = (uint8_t)(kitenConfig.bright / 3);
        if (dim < 1) dim = 1;
        applyBrightness(dim);
        screenDimmed = true;
    }
    if (screenDimmed && !screenOff && idle > dimMs + SCREEN_OFF_DELAY_MS) {
        fadeOutScreen();
        screenOff = true;
        M5.Display.sleep();
    }
}

void wakeUpScreen()
{
    lastActivity = millis();
    if (screenOff) {
        M5.Display.wakeup();
        screenOff = false;
        applyBrightness(kitenConfig.bright);
    } else if (screenDimmed) {
        applyBrightness(kitenConfig.bright);
    }
    screenDimmed = false;
}

void fadeOutScreen()
{
    int b = kitenConfig.bright;
    while (b > 0) {
        M5.Display.setBrightness(b);
        b -= 5;
        if (b < 0) b = 0;
        delay(5);
    }
}

void sleepModeOn()
{
    fadeOutScreen();
    M5.Display.sleep();
    screenOff = true;
}

void sleepModeOff()
{
    M5.Display.wakeup();
    applyBrightness(kitenConfig.bright);
    screenOff = false;
    screenDimmed = false;
    lastActivity = millis();
}

extern "C" void markActivity()
{
    lastActivity = millis();
}

static const int LOG_TOP_Y      = 30;
static const int LOG_BOT_Y      = SCREEN_HEIGHT - 12;
static const int LOG_LINE_H     = LH * FP;
static const int LOG_MAX_LINES  = (LOG_BOT_Y - LOG_TOP_Y) / LOG_LINE_H;

static std::vector<String> g_logLines;
static bool g_logDirty = true;

void logClear()
{
    g_logLines.clear();
    g_logDirty = true;
}

void logPrint(const String &s)
{
    int start = 0;
    while (start <= (int)s.length()) {
        int nl = s.indexOf('\n', start);
        String line = (nl < 0) ? s.substring(start) : s.substring(start, nl);
        g_logLines.push_back(line);
        if (g_logLines.size() > 64) g_logLines.erase(g_logLines.begin());
        if (nl < 0) break;
        start = nl + 1;
    }
    g_logDirty = true;
}

void logRender()
{
    if (!g_logDirty) return;
    g_logDirty = false;
    M5.Display.fillRect(0, LOG_TOP_Y, SCREEN_WIDTH, LOG_BOT_Y - LOG_TOP_Y,
                        kitenConfig.bgColor);
    M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
    M5.Display.setTextSize(FP);
    M5.Display.setTextDatum(top_left);
    int n = g_logLines.size();
    int firstLine = (n > LOG_MAX_LINES) ? (n - LOG_MAX_LINES) : 0;
    int y = LOG_TOP_Y;
    for (int i = firstLine; i < n; i++) {
        M5.Display.drawString(g_logLines[i], 4, y);
        y += LOG_LINE_H;
        if (y >= LOG_BOT_Y) break;
    }
    M5.Display.setTextColor(kitenConfig.secColor, kitenConfig.bgColor);
    M5.Display.setTextSize(FP);
    M5.Display.drawString("Enter: send | Back: exit",
                          4, SCREEN_HEIGHT - LH * FP - 1);
}

void updateTimeStr(struct tm timeInfo)
{
    if (kitenConfig.clock24hr) {
        snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d",
                 timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec);
    } else {
        int hour12 = (timeInfo.tm_hour == 0)  ? 12 :
                     (timeInfo.tm_hour > 12) ? timeInfo.tm_hour - 12 :
                                                timeInfo.tm_hour;
        const char *ampm = (timeInfo.tm_hour < 12) ? "AM" : "PM";
        snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d %s",
                 hour12, timeInfo.tm_min, timeInfo.tm_sec, ampm);
    }
}
