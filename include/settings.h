#pragma once
#include <Arduino.h>
#include <vector>

#include <Preferences.h>

#define THEME_COUNT 4
#define THEME_CUSTOM_ID 3

struct ThemePreset {
    const char *name;
    uint8_t  r1, g1, b1;   
    uint8_t  r2, g2, b2;   
    uint8_t  pr, pg, pb;   
    uint8_t  sr, sg, sb;   
    uint8_t  br, bg_, bb;  
    uint8_t  ar, ag, ab;   
};

static const ThemePreset THEME_PRESETS[THEME_COUNT] = {
     {"Aurora",   138,255,230,  131,112,255,  131,112,255,   80, 70,180,    0,  0,  0,  138,255,230},
     {"Lavender", 184,133,255,  133,255,228,  184,133,255,  120, 90,200,    0,  0,  0,  133,255,228},
     {"Ocean",     74,143,255,   95, 74,255,   95, 74,255,   60, 50,180,    0,  0,  0,   74,143,255},
     {"Custom",   138,255,230,  131,112,255,  131,112,255,   80, 70,180,    0,  0,  0,  138,255,230},
};

struct KitenConfig {
    
    int       bright          = 100;
    int       dimmerSet       = 60;
    int       rotation        = 1;            
    int       themeId         = 0;            
    uint16_t  priColor        = 0;            
    uint16_t  secColor        = 0;
    uint16_t  bgColor         = 0;
    uint16_t  accentColor     = 0;
    uint16_t  gradStart       = 0;            
    uint16_t  gradEnd         = 0;            
    bool      colorInverted   = false;

    
    bool      soundEnabled    = true;
    int       soundVolume     = 100;

    
    float     tmz             = 0.0f;
    bool      dst             = false;
    bool      clock24hr       = true;
    bool      clockSet        = false;

    
    bool      instantBoot     = false;
    bool      wifiAtStartup   = false;
    bool      sleepDisabled   = false;   
    String    keyboardLang    = "QWERTY";

    
    bool      sshTelnetLog    = false;   

    
    
    
    String    wifiMAC         = "";      

    
    
    
    
    
    String    evilGatewayIp   = "172.0.0.1";  
    bool      evilPasswordMode= false;        
    String    evilCredsPath   = "/creds";     
    bool      evilAllowGetCreds = false;      
    String    evilSsidPath    = "/ssid";      
    bool      evilAllowSetSsid  = false;      
    bool      evilAllowEndpointDisplay = false;  

    
    
    void addWifiCredential(const String &ssid, const String &pwd);
    
    String getWifiPassword(const String &ssid);
    
    void removeWifiCredential(const String &ssid);
    
    std::vector<String> listWifiSSIDs();

    
    void begin();           
    void save();            
    void factoryReset();    

    
    
    
    void setTheme(int id);
    
    
    void setCustomPriColor  (uint16_t c);
    void setCustomSecColor  (uint16_t c);
    void setCustomBgColor   (uint16_t c);
    void setCustomAccent    (uint16_t c);
    void setCustomGradStart (uint16_t c);
    void setCustomGradEnd   (uint16_t c);

    
    void setBright(int v);
    void setDimmerSet(int v);
    void setRotation(int v);
    void setColorInverted(bool v);
    void setSoundEnabled(bool v);
    void setSoundVolume(int v);
    void setTmz(float v);
    void setDST(bool v);
    void setClock24Hr(bool v);
    void setClockSet(bool v);
    void setInstantBoot(bool v);
    void setWifiAtStartup(bool v);
    void setSleepDisabled(bool v);
    void setKeyboardLang(const String &v);
    void setWifiMAC(const String &v);
    void setSshTelnetLog(bool v);

    
    void setEvilGatewayIp(const String &v);
    void setEvilPasswordMode(bool v);
    void setEvilCredsPath(const String &v);
    void setEvilAllowGetCreds(bool v);
    void setEvilSsidPath(const String &v);
    void setEvilAllowSetSsid(bool v);
    void setEvilAllowEndpointDisplay(bool v);

  private:
    void applyThemePreset(int id);   
    void persistColors();
};

extern KitenConfig kitenConfig;

inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    return ((r & 0xF8) << 8) |
           ((g & 0xFC) << 3) |
           ( b >> 3);
}

inline void rgb565_to_rgb888(uint16_t c, uint8_t &r, uint8_t &g, uint8_t &b)
{
    r = (c >> 8) & 0xF8; r |= r >> 5;
    g = (c >> 3) & 0xFC; g |= g >> 6;
    b = (c << 3) & 0xF8; b |= b >> 5;
}

uint16_t getColorVariation(uint16_t color);

uint16_t getComplementaryColor(uint16_t color);

uint16_t lerpColor565(uint16_t a, uint16_t b, float t);
