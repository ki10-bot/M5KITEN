#include "bootscreen.h"
#include "config.h"
#include "settings.h"   
#include <Arduino.h>
#include <string.h>

static const char* LOGO[] = {
    "      ___   ",
    "     /  /\\  ",
    "    /  /:/  ",
    "   /  /:/   ",
    "  /  /::\\___",
    " /__/:/\\::::\\",
    " \\__\\/~|:|~~~",
    "    |  |:|   ",
    "    |__|:|   ",
    "     \\__\\|   ",
};

static const int LOGO_LINES = sizeof(LOGO) / sizeof(LOGO[0]);

static const char* BOOT_STEPS[] = {
    "Initializing...",
    "Mounting filesystem...",
    "Loading keyboard...",
    "Loading runtime...",
    "Launching interface..."
};

static const int BOOT_STEPS_COUNT = sizeof(BOOT_STEPS) / sizeof(BOOT_STEPS[0]);

static uint8_t lerp8(uint8_t a, uint8_t b, float t)
{
    return (uint8_t)(a + (b - a) * t);
}

static uint16_t gradientColor(float t)
{
    uint16_t c1 = kitenConfig.gradStart;
    uint16_t c2 = kitenConfig.gradEnd;

    
    
    
    if (c1 == 0 && c2 == 0) {
        c1 = rgb565(138, 255, 230);   
        c2 = rgb565(131, 112, 255);   
    }

    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    uint8_t r1, g1, b1, r2, g2, b2;
    rgb565_to_rgb888(c1, r1, g1, b1);
    rgb565_to_rgb888(c2, r2, g2, b2);

    return rgb565(
        lerp8(r1, r2, t),
        lerp8(g1, g2, t),
        lerp8(b1, b2, t)
    );
}

static void drawGradientText(M5GFX &display, int x, int y, const char* text)
{
    size_t len = strlen(text);
    int cursorX = x;

    for (size_t i = 0; i < len; i++)
    {
        char buf[2] = { text[i], '\0' };
        float t = (len <= 1) ? 0.0f : (float)i / (float)(len - 1);
        display.setTextColor(gradientColor(t));
        display.setCursor(cursorX, y);
        display.print(buf);
        cursorX += 6; 
    }
}

static void drawProgressBar(M5GFX &display, int barX, int barY, int barW, int barH, float progress)
{
    
    uint16_t trackColor = rgb565(30, 30, 40);
    display.fillRect(barX, barY, barW, barH, trackColor);

    
    int fillW = (int)(barW * progress);
    for (int px = 0; px < fillW; px++)
    {
        uint16_t c = gradientColor((float)px / (float)barW);
        for (int py = 0; py < barH; py++)
        {
            display.drawPixel(barX + px, barY + py, c);
        }
    }
}

void runBootscreen(M5GFX &display)
{
    
    
    uint16_t bg = kitenConfig.bgColor;
    if (bg == 0 && kitenConfig.gradStart == 0 && kitenConfig.gradEnd == 0) {
        
        bg = 0x0000;
    }

    display.fillScreen(bg);
    display.setTextFont(0);   
    display.setTextSize(1);

    const int charH = 8;
    const int barH  = 2;     
    const int barW  = 140;   
    const int gapLogoBar = 10;
    const int gapBarText = 6;

    
    int logoH   = LOGO_LINES * charH;                   
    int totalH  = logoH + gapLogoBar + barH + gapBarText + charH; 
    int startY  = (SCREEN_HEIGHT - totalH) / 2;

    
    for (int i = 0; i < LOGO_LINES; i++)
    {
        int16_t w = display.textWidth(LOGO[i]);
        int x = (SCREEN_WIDTH - w) / 2;
        int y = startY + i * charH;

        float t = (LOGO_LINES <= 1) ? 0.0f : (float)i / (float)(LOGO_LINES - 1);
        display.setTextColor(gradientColor(t));
        display.setCursor(x, y);
        display.print(LOGO[i]);

        delay(BOOT_LINE_DELAY);
    }

    
    int barX = (SCREEN_WIDTH - barW) / 2;
    int barY = startY + logoH + gapLogoBar;

    
    int textY = barY + barH + gapBarText;

    
    for (int i = 0; i < BOOT_STEPS_COUNT; i++)
    {
        
        float progress = (float)(i + 1) / (float)BOOT_STEPS_COUNT;
        drawProgressBar(display, barX, barY, barW, barH, progress);

        
        display.fillRect(0, textY, SCREEN_WIDTH, charH, bg);
        int16_t tw = display.textWidth(BOOT_STEPS[i]);
        drawGradientText(display, (SCREEN_WIDTH - tw) / 2, textY, BOOT_STEPS[i]);

        delay(BOOT_STEP_DELAY);
    }

    delay(BOOT_DONE_HOLD);
}
