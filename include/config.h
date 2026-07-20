#pragma once

#define FW_AUTHOR       "KI10sus"
#define FW_VERSION      "0.2.0"
#define FW_NAME         "KITEN"

#define SCREEN_WIDTH    240
#define SCREEN_HEIGHT   135

#define BOOT_LINE_DELAY     25
#define BOOT_STEP_DELAY     320
#define BOOT_DONE_HOLD      600

#define BOOT_SOUND_VOLUME   32

#define FP  1   
#define FM  2   
#define FG  3   
#define LW  6   
#define LH  8   

#define STATUS_BAR_HEIGHT           30
#define BORDER_OFFSET_FROM_SCREEN_EDGE 5
#define BORDER_PAD_X                10
#define BORDER_PAD_Y                28
#define MAX_MENU_SIZE               5      

#define MENU_TYPE_MAIN              0   
#define MENU_TYPE_SUBMENU           1   
#define MENU_TYPE_REGULAR           2   

#define COL_BG          0x0000   

#define TFT_BLACK       0x0000
#define TFT_NAVY        0x000F
#define TFT_DARKGREEN   0x03E0
#define TFT_DARKCYAN    0x03EF
#define TFT_MAROON      0x7800
#define TFT_PURPLE      0x780F
#define TFT_OLIVE       0x7BE0
#define TFT_LIGHTGREY   0xD69A
#define TFT_DARKGREY    0x7BEF
#define TFT_BLUE        0x001F
#define TFT_GREEN       0x07E0
#define TFT_CYAN        0x07FF
#define TFT_RED         0xF800
#define TFT_MAGENTA     0xF81F
#define TFT_YELLOW      0xFFE0
#define TFT_WHITE       0xFFFF
#define TFT_ORANGE      0xFDA0
#define TFT_GREENYELLOW 0xB7E0
#define TFT_PINK        0xFE19
#define TFT_BROWN       0x9A60
#define TFT_GOLD        0xFEA0
#define TFT_SILVER      0xC618
#define TFT_SKYBLUE     0x867D
#define TFT_VIOLET      0x915C

#define DEFAULT_PRICOLOR 0xA80F   
#define DEFAULT_SECCOLOR 0xCB76   

#define ANIM_FADE_STEP_MS      8     
#define ANIM_FADE_STEPS        6     
#define ANIM_CURSOR_PULSE_MS   1200  
#define ANIM_SLIDE_STEP_MS     6     
#define ANIM_SLIDE_STEPS       4     
#define ANIM_CLOCK_COLON_MS    500   

#define TZ_ENTRIES_TOTAL       38
#define NTP_SERVER             "pool.ntp.org"
