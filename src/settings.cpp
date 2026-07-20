#include "settings.h"
#include "config.h"
#include <Arduino.h>

KitenConfig kitenConfig;

static Preferences prefs;
static const char *NVS_NAMESPACE = "kiten";

void KitenConfig::applyThemePreset(int id)
{
    if (id < 0 || id >= THEME_COUNT) id = 0;
    const ThemePreset &t = THEME_PRESETS[id];
    priColor    = rgb565(t.pr, t.pg, t.pb);
    secColor    = rgb565(t.sr, t.sg, t.sb);
    bgColor     = rgb565(t.br, t.bg_, t.bb);
    accentColor = rgb565(t.ar, t.ag, t.ab);
    gradStart   = rgb565(t.r1, t.g1, t.b1);
    gradEnd     = rgb565(t.r2, t.g2, t.b2);
}

void KitenConfig::persistColors()
{
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putUShort("priColor",    priColor);
    prefs.putUShort("secColor",    secColor);
    prefs.putUShort("bgColor",     bgColor);
    prefs.putUShort("accentColor", accentColor);
    prefs.putUShort("gradStart",   gradStart);
    prefs.putUShort("gradEnd",     gradEnd);
    prefs.end();
}

void KitenConfig::begin()
{
    prefs.begin(NVS_NAMESPACE, true);  
    bright         = prefs.getInt  ("bright",        bright);
    dimmerSet      = prefs.getInt  ("dimmerSet",     dimmerSet);
    rotation       = prefs.getInt  ("rotation",      rotation);
    themeId        = prefs.getInt  ("themeId",       themeId);
    colorInverted  = prefs.getBool ("colorInverted", colorInverted);
    soundEnabled   = prefs.getBool ("soundEnabled",  soundEnabled);
    soundVolume    = prefs.getInt  ("soundVolume",   soundVolume);
    tmz            = prefs.getFloat("tmz",           tmz);
    dst            = prefs.getBool ("dst",           dst);
    clock24hr      = prefs.getBool ("clock24hr",     clock24hr);
    clockSet       = prefs.getBool ("clockSet",      clockSet);
    instantBoot    = prefs.getBool ("instantBoot",   instantBoot);
    wifiAtStartup  = prefs.getBool ("wifiAtStartup", wifiAtStartup);
    sleepDisabled  = prefs.getBool ("sleepDisabled", sleepDisabled);
    keyboardLang   = prefs.getString("keyboardLang", keyboardLang);
    wifiMAC        = prefs.getString("wifiMAC",      wifiMAC);
    sshTelnetLog   = prefs.getBool ("sshTelnetLog",  sshTelnetLog);

    
    evilGatewayIp            = prefs.getString("evilGwIp",   evilGatewayIp);
    evilPasswordMode         = prefs.getBool  ("evilPwdMod", evilPasswordMode);
    evilCredsPath            = prefs.getString("evilCredsP", evilCredsPath);
    evilAllowGetCreds        = prefs.getBool  ("evilACred",  evilAllowGetCreds);
    evilSsidPath             = prefs.getString("evilSsidP",  evilSsidPath);
    evilAllowSetSsid         = prefs.getBool  ("evilASsid",  evilAllowSetSsid);
    evilAllowEndpointDisplay = prefs.getBool  ("evilAEndp",  evilAllowEndpointDisplay);

    
    
    
    
    
    
    
    
    priColor    = (uint16_t)prefs.getUShort("priColor",    0xFFFF);
    secColor    = (uint16_t)prefs.getUShort("secColor",    0xFFFF);
    bgColor     = (uint16_t)prefs.getUShort("bgColor",     0xFFFF);
    accentColor = (uint16_t)prefs.getUShort("accentColor", 0xFFFF);
    gradStart   = (uint16_t)prefs.getUShort("gradStart",   0xFFFF);
    gradEnd     = (uint16_t)prefs.getUShort("gradEnd",     0xFFFF);
    prefs.end();

    if (themeId >= 0 && themeId < THEME_CUSTOM_ID) {
        
        
        applyThemePreset(themeId);
    } else {
        
        
        if (priColor    == 0xFFFF) applyThemePreset(THEME_CUSTOM_ID);
    }

    
    if (bright <= 0) bright = 1;
    if (bright > 255) bright = 255;
    if (soundVolume < 0) soundVolume = 0;
    if (soundVolume > 100) soundVolume = 100;
    if (keyboardLang.length() == 0) keyboardLang = "QWERTY";
    if (themeId < 0 || themeId >= THEME_COUNT) themeId = 0;
}

void KitenConfig::save()
{
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putInt   ("bright",        bright);
    prefs.putInt   ("dimmerSet",     dimmerSet);
    prefs.putInt   ("rotation",      rotation);
    prefs.putInt   ("themeId",       themeId);
    prefs.putUShort("priColor",      priColor);
    prefs.putUShort("secColor",      secColor);
    prefs.putUShort("bgColor",       bgColor);
    prefs.putUShort("accentColor",   accentColor);
    prefs.putUShort("gradStart",     gradStart);
    prefs.putUShort("gradEnd",       gradEnd);
    prefs.putBool  ("colorInverted", colorInverted);
    prefs.putBool  ("soundEnabled",  soundEnabled);
    prefs.putInt   ("soundVolume",   soundVolume);
    prefs.putFloat ("tmz",           tmz);
    prefs.putBool  ("dst",           dst);
    prefs.putBool  ("clock24hr",     clock24hr);
    prefs.putBool  ("clockSet",      clockSet);
    prefs.putBool  ("instantBoot",   instantBoot);
    prefs.putBool  ("wifiAtStartup", wifiAtStartup);
    prefs.putBool  ("sleepDisabled", sleepDisabled);
    prefs.putString("keyboardLang",  keyboardLang);
    prefs.putString("wifiMAC",       wifiMAC);
    prefs.putBool  ("sshTelnetLog",  sshTelnetLog);
    
    prefs.putString("evilGwIp",   evilGatewayIp);
    prefs.putBool  ("evilPwdMod", evilPasswordMode);
    prefs.putString("evilCredsP", evilCredsPath);
    prefs.putBool  ("evilACred",  evilAllowGetCreds);
    prefs.putString("evilSsidP",  evilSsidPath);
    prefs.putBool  ("evilASsid",  evilAllowSetSsid);
    prefs.putBool  ("evilAEndp",  evilAllowEndpointDisplay);
    prefs.end();
}

void KitenConfig::factoryReset()
{
    prefs.begin(NVS_NAMESPACE, false);
    prefs.clear();
    prefs.end();
    delay(50);
    ESP.restart();
}

void KitenConfig::setTheme(int id)
{
    if (id < 0 || id >= THEME_COUNT) id = 0;
    themeId = id;
    if (id != THEME_CUSTOM_ID) {
        
        applyThemePreset(id);
    }
    
    
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putInt("themeId", themeId);
    prefs.end();
    persistColors();
}

void KitenConfig::setCustomPriColor(uint16_t c)
{
    priColor = c;
    
    if (themeId != THEME_CUSTOM_ID) {
        themeId = THEME_CUSTOM_ID;
        prefs.begin(NVS_NAMESPACE, false);
        prefs.putInt("themeId", themeId);
        prefs.end();
    }
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putUShort("priColor", c);
    prefs.end();
}

void KitenConfig::setCustomSecColor(uint16_t c)
{
    secColor = c;
    if (themeId != THEME_CUSTOM_ID) {
        themeId = THEME_CUSTOM_ID;
        prefs.begin(NVS_NAMESPACE, false);
        prefs.putInt("themeId", themeId);
        prefs.end();
    }
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putUShort("secColor", c);
    prefs.end();
}

void KitenConfig::setCustomBgColor(uint16_t c)
{
    bgColor = c;
    if (themeId != THEME_CUSTOM_ID) {
        themeId = THEME_CUSTOM_ID;
        prefs.begin(NVS_NAMESPACE, false);
        prefs.putInt("themeId", themeId);
        prefs.end();
    }
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putUShort("bgColor", c);
    prefs.end();
}

void KitenConfig::setCustomAccent(uint16_t c)
{
    accentColor = c;
    if (themeId != THEME_CUSTOM_ID) {
        themeId = THEME_CUSTOM_ID;
        prefs.begin(NVS_NAMESPACE, false);
        prefs.putInt("themeId", themeId);
        prefs.end();
    }
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putUShort("accentColor", c);
    prefs.end();
}

void KitenConfig::setCustomGradStart(uint16_t c)
{
    gradStart = c;
    if (themeId != THEME_CUSTOM_ID) {
        themeId = THEME_CUSTOM_ID;
        prefs.begin(NVS_NAMESPACE, false);
        prefs.putInt("themeId", themeId);
        prefs.end();
    }
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putUShort("gradStart", c);
    prefs.end();
}

void KitenConfig::setCustomGradEnd(uint16_t c)
{
    gradEnd = c;
    if (themeId != THEME_CUSTOM_ID) {
        themeId = THEME_CUSTOM_ID;
        prefs.begin(NVS_NAMESPACE, false);
        prefs.putInt("themeId", themeId);
        prefs.end();
    }
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putUShort("gradEnd", c);
    prefs.end();
}

void KitenConfig::setBright(int v)          { bright = v;          prefs.begin(NVS_NAMESPACE, false); prefs.putInt("bright", v); prefs.end(); }
void KitenConfig::setDimmerSet(int v)       { dimmerSet = v;       prefs.begin(NVS_NAMESPACE, false); prefs.putInt("dimmerSet", v); prefs.end(); }
void KitenConfig::setRotation(int v)        { rotation = v;        prefs.begin(NVS_NAMESPACE, false); prefs.putInt("rotation", v); prefs.end(); }
void KitenConfig::setColorInverted(bool v)  { colorInverted = v; prefs.begin(NVS_NAMESPACE, false); prefs.putBool("colorInverted", v); prefs.end(); }
void KitenConfig::setSoundEnabled(bool v)   { soundEnabled = v; prefs.begin(NVS_NAMESPACE, false); prefs.putBool("soundEnabled", v); prefs.end(); }
void KitenConfig::setSoundVolume(int v)     { soundVolume = v; prefs.begin(NVS_NAMESPACE, false); prefs.putInt("soundVolume", v); prefs.end(); }
void KitenConfig::setTmz(float v)           { tmz = v; prefs.begin(NVS_NAMESPACE, false); prefs.putFloat("tmz", v); prefs.end(); }
void KitenConfig::setDST(bool v)            { dst = v; prefs.begin(NVS_NAMESPACE, false); prefs.putBool("dst", v); prefs.end(); }
void KitenConfig::setClock24Hr(bool v)      { clock24hr = v; prefs.begin(NVS_NAMESPACE, false); prefs.putBool("clock24hr", v); prefs.end(); }
void KitenConfig::setClockSet(bool v)       { clockSet = v; prefs.begin(NVS_NAMESPACE, false); prefs.putBool("clockSet", v); prefs.end(); }
void KitenConfig::setInstantBoot(bool v)    { instantBoot = v; prefs.begin(NVS_NAMESPACE, false); prefs.putBool("instantBoot", v); prefs.end(); }
void KitenConfig::setWifiAtStartup(bool v)  { wifiAtStartup = v; prefs.begin(NVS_NAMESPACE, false); prefs.putBool("wifiAtStartup", v); prefs.end(); }
void KitenConfig::setSleepDisabled(bool v)  { sleepDisabled = v; prefs.begin(NVS_NAMESPACE, false); prefs.putBool("sleepDisabled", v); prefs.end(); }
void KitenConfig::setKeyboardLang(const String &v) { keyboardLang = v; prefs.begin(NVS_NAMESPACE, false); prefs.putString("keyboardLang", v); prefs.end(); }
void KitenConfig::setWifiMAC(const String &v)      { wifiMAC = v;     prefs.begin(NVS_NAMESPACE, false); prefs.putString("wifiMAC", v); prefs.end(); }
void KitenConfig::setSshTelnetLog(bool v)   { sshTelnetLog = v; prefs.begin(NVS_NAMESPACE, false); prefs.putBool("sshTelnetLog", v); prefs.end(); }

void KitenConfig::setEvilGatewayIp(const String &v)         { evilGatewayIp = v;           prefs.begin(NVS_NAMESPACE, false); prefs.putString("evilGwIp",  v); prefs.end(); }
void KitenConfig::setEvilPasswordMode(bool v)               { evilPasswordMode = v;        prefs.begin(NVS_NAMESPACE, false); prefs.putBool  ("evilPwdMod", v); prefs.end(); }
void KitenConfig::setEvilCredsPath(const String &v)         { evilCredsPath = v;           prefs.begin(NVS_NAMESPACE, false); prefs.putString("evilCredsP", v); prefs.end(); }
void KitenConfig::setEvilAllowGetCreds(bool v)              { evilAllowGetCreds = v;       prefs.begin(NVS_NAMESPACE, false); prefs.putBool  ("evilACred",  v); prefs.end(); }
void KitenConfig::setEvilSsidPath(const String &v)          { evilSsidPath = v;            prefs.begin(NVS_NAMESPACE, false); prefs.putString("evilSsidP",  v); prefs.end(); }
void KitenConfig::setEvilAllowSetSsid(bool v)               { evilAllowSetSsid = v;        prefs.begin(NVS_NAMESPACE, false); prefs.putBool  ("evilASsid",  v); prefs.end(); }
void KitenConfig::setEvilAllowEndpointDisplay(bool v)       { evilAllowEndpointDisplay = v; prefs.begin(NVS_NAMESPACE, false); prefs.putBool("evilAEndp",  v); prefs.end(); }

static const char *WIFI_SSIDS_KEY = "wifiSSIDs";
static const char *WIFI_PWDS_KEY  = "wifiPwds";

static std::vector<String> splitPipes(const String &s)
{
    std::vector<String> out;
    int start = 0;
    while (start <= (int)s.length()) {
        int idx = s.indexOf('|', start);
        if (idx < 0) {
            out.push_back(s.substring(start));
            break;
        }
        out.push_back(s.substring(start, idx));
        start = idx + 1;
    }
    return out;
}

static String joinPipes(const std::vector<String> &v)
{
    String out;
    for (size_t i = 0; i < v.size(); i++) {
        if (i > 0) out += "|";
        out += v[i];
    }
    return out;
}

void KitenConfig::addWifiCredential(const String &ssid, const String &pwd)
{
    prefs.begin(NVS_NAMESPACE, false);
    String ssids = prefs.getString(WIFI_SSIDS_KEY, "");
    String pwds  = prefs.getString(WIFI_PWDS_KEY, "");
    prefs.end();

    auto sList = splitPipes(ssids);
    auto pList = splitPipes(pwds);
    
    while (pList.size() < sList.size()) pList.push_back("");

    
    for (size_t i = 0; i < sList.size(); i++) {
        if (sList[i] == ssid) {
            pList[i] = pwd;
            prefs.begin(NVS_NAMESPACE, false);
            prefs.putString(WIFI_SSIDS_KEY, joinPipes(sList));
            prefs.putString(WIFI_PWDS_KEY,  joinPipes(pList));
            prefs.end();
            return;
        }
    }
    
    sList.push_back(ssid);
    pList.push_back(pwd);
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putString(WIFI_SSIDS_KEY, joinPipes(sList));
    prefs.putString(WIFI_PWDS_KEY,  joinPipes(pList));
    prefs.end();
}

String KitenConfig::getWifiPassword(const String &ssid)
{
    prefs.begin(NVS_NAMESPACE, true);
    String ssids = prefs.getString(WIFI_SSIDS_KEY, "");
    String pwds  = prefs.getString(WIFI_PWDS_KEY, "");
    prefs.end();

    auto sList = splitPipes(ssids);
    auto pList = splitPipes(pwds);
    for (size_t i = 0; i < sList.size() && i < pList.size(); i++) {
        if (sList[i] == ssid) return pList[i];
    }
    return "";
}

void KitenConfig::removeWifiCredential(const String &ssid)
{
    prefs.begin(NVS_NAMESPACE, false);
    String ssids = prefs.getString(WIFI_SSIDS_KEY, "");
    String pwds  = prefs.getString(WIFI_PWDS_KEY, "");
    prefs.end();

    auto sList = splitPipes(ssids);
    auto pList = splitPipes(pwds);
    std::vector<String> newS, newP;
    for (size_t i = 0; i < sList.size() && i < pList.size(); i++) {
        if (sList[i] != ssid) {
            newS.push_back(sList[i]);
            newP.push_back(pList[i]);
        }
    }
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putString(WIFI_SSIDS_KEY, joinPipes(newS));
    prefs.putString(WIFI_PWDS_KEY,  joinPipes(newP));
    prefs.end();
}

std::vector<String> KitenConfig::listWifiSSIDs()
{
    prefs.begin(NVS_NAMESPACE, true);
    String ssids = prefs.getString(WIFI_SSIDS_KEY, "");
    prefs.end();
    return splitPipes(ssids);
}

uint16_t getColorVariation(uint16_t color)
{
    uint8_t r, g, b;
    rgb565_to_rgb888(color, r, g, b);
    
    r = (r * 3) / 4;
    g = (g * 3) / 4;
    b = (b * 3) / 4;
    return rgb565(r, g, b);
}

uint16_t getComplementaryColor(uint16_t color)
{
    uint8_t r, g, b;
    rgb565_to_rgb888(color, r, g, b);
    
    return rgb565(255 - r, 255 - g, 255 - b);
}

uint16_t lerpColor565(uint16_t a, uint16_t b, float t)
{
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    uint8_t ar, ag, ab, br, bg, bb;
    rgb565_to_rgb888(a, ar, ag, ab);
    rgb565_to_rgb888(b, br, bg, bb);
    uint8_t r = (uint8_t)(ar + (br - ar) * t);
    uint8_t g = (uint8_t)(ag + (bg - ag) * t);
    uint8_t bl = (uint8_t)(ab + (bb - ab) * t);
    return rgb565(r, g, bl);
}
