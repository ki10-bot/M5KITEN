

#include "ipgen_menu.h"
#include "config.h"
#include "settings.h"
#include "keyboard.h"
#include "ui.h"
#include "menu.h"
#include <Arduino.h>
#include <M5Unified.h>
#include <vector>
#include <String>

#define IPGEN_MAX_COUNT    500
#define IPGEN_DEFAULT_COUNT 10

static std::vector<String> g_generatedIPs;
static int g_ipgenCount = IPGEN_DEFAULT_COUNT;
static int g_scrollOffset = 0;   

static String generateRandomIPPort()
{
    
    String ip = String(random(0, 255)) + "." +
                String(random(0, 255)) + "." +
                String(random(0, 255)) + "." +
                String(random(0, 255));
    
    
    String result = ip + ":" + String(random(0, 65536));
    
    return result;
}

static void generateIPs(int count)
{
    g_generatedIPs.clear();
    g_scrollOffset = 0;
    
    
    if (count < 1) count = 1;
    if (count > IPGEN_MAX_COUNT) count = IPGEN_MAX_COUNT;
    
    
    g_generatedIPs.reserve(count);
    
    
    for (int i = 0; i < count; i++) {
        g_generatedIPs.push_back(generateRandomIPPort());
    }
    
    
    randomSeed(millis() + analogRead(0));
}

static void renderIPList()
{
    M5.Display.fillScreen(kitenConfig.bgColor);
    drawMainBorder(false);
    
    
    M5.Display.setTextSize(FM);
    M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
    M5.Display.setCursor(BORDER_PAD_X, BORDER_PAD_Y);
    M5.Display.print("IP Generator Results");
    
    
    M5.Display.drawFastHLine(BORDER_PAD_X, BORDER_PAD_Y + LH * FM + 2,
                             SCREEN_WIDTH - 2 * BORDER_PAD_X, kitenConfig.secColor);
    
    
    int startY = BORDER_PAD_Y + LH * FM + 6;
    int lineHeight = LH * FP + 2;  
    int maxVisible = (SCREEN_HEIGHT - startY - 12) / lineHeight;
    if (maxVisible < 2) maxVisible = 2;
    if (maxVisible > 6) maxVisible = 6;
    
    
    M5.Display.setTextSize(FP);
    int displayCount = g_generatedIPs.size();
    
    for (int i = 0; i < maxVisible; i++) {
        int idx = g_scrollOffset + i;
        int y = startY + i * lineHeight;
        
        if (idx >= 0 && idx < displayCount) {
            
            M5.Display.setTextColor(kitenConfig.secColor, kitenConfig.bgColor);
            M5.Display.setCursor(BORDER_PAD_X, y);
            
            
            char numBuf[8];
            snprintf(numBuf, sizeof(numBuf), "%4d ", idx + 1);
            M5.Display.print(numBuf);
            
            
            M5.Display.setTextColor(TFT_WHITE, kitenConfig.bgColor);
            
            
            String entry = g_generatedIPs[idx];
            int maxChars = (SCREEN_WIDTH - 2 * BORDER_PAD_X - 30) / LW;  
            if ((int)entry.length() > maxChars) {
                entry = entry.substring(0, maxChars - 2) + "..";
            }
            M5.Display.print(entry);
        } else {
            
            M5.Display.setTextColor(kitenConfig.bgColor, kitenConfig.bgColor);
            M5.Display.setCursor(BORDER_PAD_X, y);
            M5.Display.print("                    ");
        }
    }
    
    
    int bottomY = SCREEN_HEIGHT - LH * FP - 2;
    M5.Display.fillRect(0, bottomY - 2, SCREEN_WIDTH, LH * FP + 4, kitenConfig.bgColor);
    M5.Display.drawFastHLine(0, bottomY - 2, SCREEN_WIDTH, kitenConfig.secColor);
    
    M5.Display.setTextSize(FP);
    M5.Display.setTextColor(kitenConfig.secColor, kitenConfig.bgColor);
    M5.Display.setCursor(BORDER_PAD_X, bottomY);
    
    
    int viewEnd = g_scrollOffset + maxVisible;
    if (viewEnd > displayCount) viewEnd = displayCount;
    char statusBuf[32];
    if (displayCount > 0) {
        snprintf(statusBuf, sizeof(statusBuf), "%d-%d / %d", 
                 g_scrollOffset + 1, viewEnd, displayCount);
    } else {
        snprintf(statusBuf, sizeof(statusBuf), "No results");
    }
    M5.Display.print(statusBuf);
    
    
    if (g_scrollOffset > 0) {
        M5.Display.setCursor(SCREEN_WIDTH - 20, bottomY);
        M5.Display.print("^");
    }
    if (viewEnd < displayCount) {
        M5.Display.setCursor(SCREEN_WIDTH - 10, bottomY);
        M5.Display.print("v");
    }
}

static void viewResults()
{
    if (g_generatedIPs.empty()) {
        displayError("No IPs generated!");
        delay(1000);
        return;
    }
    
    renderIPList();
    waitAllKeysReleased();
    
    bool running = true;
    while (running) {
        M5.update();
        pollKeyboard();
        
        if (check(PrevPress) || check(UpPress)) {
            
            if (g_scrollOffset > 0) {
                g_scrollOffset--;
                renderIPList();
                uiBeep(660, 25);
            } else {
                uiBeep(220, 25);  
            }
            markActivity();
        }
        
        if (check(NextPress) || check(DownPress)) {
            
            int maxVisible = (SCREEN_HEIGHT - BORDER_PAD_Y - LH * FM - 18) / (LH * FP + 2);
            if (maxVisible < 2) maxVisible = 2;
            if (maxVisible > 6) maxVisible = 6;
            
            if (g_scrollOffset < (int)g_generatedIPs.size() - maxVisible) {
                g_scrollOffset++;
                renderIPList();
                uiBeep(660, 25);
            } else {
                uiBeep(220, 25);  
            }
            markActivity();
        }
        
        if (check(EscPress)) {
            uiBeep(440, 30);
            waitAllKeysReleased();
            running = false;
        }
        
        if (check(SelPress)) {
            
            uiBeep(880, 50);
            waitAllKeysReleased();
            generateIPs(g_ipgenCount);
            renderIPList();
            displaySuccess("Regenerated!", true);
            markActivity();
        }
        
        checkPowerSaveTime();
        delay(5);
    }
}

void ipgenMenuEntry()
{
    int sel = 0;
    std::vector<Option> opts;
    
    for (;;) {
        opts.clear();
        
        
        opts.push_back({"Count: " + String(g_ipgenCount), []() {
            String s = keyboard(String(g_ipgenCount), 3, "How many IPs? (1-500):");
            if (s == "\x1B" || s.length() == 0) return;
            long v = s.toInt();
            if (v < 1) v = 1;
            if (v > IPGEN_MAX_COUNT) v = IPGEN_MAX_COUNT;
            g_ipgenCount = (int)v;
        }});
        
        
        opts.push_back({"Generate & View", []() {
            
            M5.Display.fillScreen(kitenConfig.bgColor);
            drawMainBorder(false);
            M5.Display.setTextSize(FM);
            M5.Display.setTextColor(TFT_GREEN, kitenConfig.bgColor);
            M5.Display.setTextDatum(top_center);
            M5.Display.drawString("Generating...", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 - 10);
            M5.Display.setTextDatum(top_left);
            
            
            generateIPs(g_ipgenCount);
            
            delay(200);  
            
            
            viewResults();
        }});
        
        
        opts.push_back({"Quick Gen (" + String(g_ipgenCount) + ")", []() {
            generateIPs(g_ipgenCount);
            viewResults();
        }});
        
        
        opts.push_back({"Format: x.x.x.x:p", []() {
            displayInfo("Generates random IP:Port\nIP: 0-254 per octet\nPort: 0-65535", true);
        }});
        
        
        opts.push_back({"Back", [](){}});
        
        sel = loopOptions(opts, MENU_TYPE_REGULAR, "IP Generator");
        if (sel == -1 || sel == (int)opts.size() - 1) return;
    }
}
