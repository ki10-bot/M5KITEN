#include "wifi_menu.h"
#include "wifiscanner_menu.h"
#include "config.h"
#include "settings.h"
#include "keyboard.h"
#include "ui.h"
#include "menu.h"
#include "clock_app.h"   
#include <Arduino.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_netif.h>

bool showHiddenNetworks = false;
bool wifiConnected = false;
String wifiIP = "";

static bool _wifiConnect(const String &ssid, int encryption);
static bool _connectToWifiNetwork(const String &ssid, const String &pwd);
static void wifiConfigMenu();
static void evilWifiSettingsMenu();

void wifiDisconnect()
{
    WiFi.softAPdisconnect(true);
    delay(10);
    WiFi.disconnect(true, true);
    delay(10);
    WiFi.mode(WIFI_OFF);
    delay(10);
    wifiConnected = false;
}

void applyConfiguredMAC()
{
    if (kitenConfig.wifiMAC.length() == 17 && validateMACFormat(kitenConfig.wifiMAC)) {
        uint8_t newMAC[6];
        sscanf(kitenConfig.wifiMAC.c_str(),
               "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
               &newMAC[0], &newMAC[1], &newMAC[2],
               &newMAC[3], &newMAC[4], &newMAC[5]);
        if (esp_wifi_set_mac(WIFI_IF_STA, newMAC) == ESP_OK) {
            Serial.println("[WiFi] Custom MAC applied: " + kitenConfig.wifiMAC);
        } else {
            Serial.println("[WiFi] Failed to apply custom MAC");
        }
    }
}

static bool _connectToWifiNetwork(const String &ssid, const String &pwd)
{
    drawMainBorder(true);
    M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
    M5.Display.setTextSize(FP);
    M5.Display.setTextDatum(top_left);
    M5.Display.setCursor(8, 32);
    M5.Display.println("");
    M5.Display.println("Connecting to: " + ssid);

    WiFi.mode(WIFI_MODE_STA);
    delay(10);
    WiFi.begin(ssid, pwd);

    int i = 1;
    while (WiFi.status() != WL_CONNECTED) {
        M5.Display.print(".");
        if (i > 20) {
            displayError("Wifi Offline");
            delay(500);
            break;
        }
        delay(500);
        i++;
    }

    return WiFi.status() == WL_CONNECTED;
}

static bool _wifiConnect(const String &ssid, int encryption)
{
    String password = kitenConfig.getWifiPassword(ssid);
    if (password == "" && encryption > 0) {
        password = keyboard("", 63, "Network Password:", true);
    }
    if (password == "\x1B") return false;

    bool connected = _connectToWifiNetwork(ssid, password);
    bool retry = false;

    while (!connected) {
        wakeUpScreen();
        std::vector<Option> opts = {
            {"Retry",  [&]() { retry = true;  }},
            {"Cancel", [&]() { retry = false; }},
        };
        loopOptions(opts, MENU_TYPE_SUBMENU, "WiFi Connect");

        if (!retry) {
            wifiDisconnect();
            return false;
        }

        password = keyboard(password, 63, "Network Password:", true);
        if (password == "\x1B") {
            wifiDisconnect();
            return false;
        }
        connected = _connectToWifiNetwork(ssid, password);
    }

    if (connected) {
        wifiConnected = true;
        wifiIP = WiFi.localIP().toString();
        kitenConfig.addWifiCredential(ssid, password);
        
        updateClockTimezone();
    }
    delay(200);
    return connected;
}

bool wifiConnectMenu()
{
    if (WiFi.isConnected()) return false;

    int nets;
    WiFi.mode(WIFI_MODE_STA);
    applyConfiguredMAC();

    bool refresh_scan = false;
    do {
        displayInfo("Scanning..", false);
        nets = WiFi.scanNetworks();

        std::vector<Option> opts;
        for (int i = 0; i < nets; i++) {
            if (opts.size() < 250) {
                String ssid = WiFi.SSID(i);
                int encryptionType = WiFi.encryptionType(i);
                int32_t rssi = WiFi.RSSI(i);
                int32_t ch = WiFi.channel(i);

                String encryptionPrefix = (encryptionType == WIFI_AUTH_OPEN) ? "" : "#";
                String encryptionTypeStr;
                switch (encryptionType) {
                    case WIFI_AUTH_OPEN:             encryptionTypeStr = "Open";          break;
                    case WIFI_AUTH_WEP:              encryptionTypeStr = "WEP";           break;
                    case WIFI_AUTH_WPA_PSK:          encryptionTypeStr = "WPA/PSK";       break;
                    case WIFI_AUTH_WPA2_PSK:         encryptionTypeStr = "WPA2/PSK";      break;
                    case WIFI_AUTH_WPA_WPA2_PSK:     encryptionTypeStr = "WPA/WPA2/PSK";  break;
                    case WIFI_AUTH_WPA2_ENTERPRISE:  encryptionTypeStr = "WPA2/Ent";      break;
                    default:                         encryptionTypeStr = "Unknown";       break;
                }

                
                if (!showHiddenNetworks && ssid.length() == 0) continue;

                String dispSsid = ssid.length() == 0 ? String("(hidden)") : ssid;
                String optionText = encryptionPrefix + dispSsid + "(" + String(rssi) +
                                    "|" + encryptionTypeStr + "|ch." + String(ch) + ")";

                opts.push_back({optionText, [ssid, encryptionType]() {
                    _wifiConnect(ssid, encryptionType);
                }});
            }
        }

        opts.push_back({"Hidden SSID", []() {
            String __ssid = keyboard("", 32, "Your SSID");
            if (__ssid != "\x1B") _wifiConnect(__ssid, 8);
        }});

        opts.push_back({"Back", [](){}});
        int sel = loopOptions(opts, MENU_TYPE_REGULAR, "WiFi Networks");
        if (sel == -1 || sel == (int)opts.size() - 1) {
            refresh_scan = false;
        } else if (wifiConnected) {
            
            refresh_scan = false;
        } else {
            refresh_scan = true;
        }
    } while (refresh_scan);

    WiFi.scanDelete();
    return wifiConnected;
}

bool startWiFiAP()
{
    String apSsid = "KITEN-AP";
    String apPwd  = "kiten1234";   

    IPAddress AP_GATEWAY(172, 0, 0, 1);
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(AP_GATEWAY, AP_GATEWAY, IPAddress(255, 255, 255, 0));
    bool ok = WiFi.softAP(apSsid.c_str(), apPwd.c_str(), 6, 0, 4);
    if (ok) {
        wifiIP = WiFi.softAPIP().toString();
        wifiConnected = true;
        displayInfo("AP: " + apSsid + "\nIP: " + wifiIP + "\nPwd: " + apPwd, true);
    } else {
        displayError("Failed to start AP", true);
    }
    return ok;
}

void displayAPInfo()
{
    drawMainBorder(true);
    M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
    M5.Display.setTextSize(FP);
    M5.Display.setTextWrap(true);
    M5.Display.setCursor(4, 30);

    if (WiFi.status() != WL_CONNECTED) {
        M5.Display.println("Not connected to any AP.");
    } else {
        M5.Display.println("SSID: " + WiFi.SSID());
        M5.Display.println("IP: "   + WiFi.localIP().toString());
        M5.Display.println("GW: "   + WiFi.gatewayIP().toString());
        M5.Display.println("Mask: " + WiFi.subnetMask().toString());
        M5.Display.println("RSSI: " + String(WiFi.RSSI()) + " dBm");
        M5.Display.println("Chan: " + String(WiFi.channel()));
        M5.Display.println("MAC: "  + WiFi.macAddress());
        M5.Display.println("BSSID:" + WiFi.BSSIDstr());
    }
    M5.Display.println();
    M5.Display.println("Press any key to exit");
    M5.Display.setTextWrap(false);

    waitAllKeysReleased();
    while (!check(AnyKeyPress)) {
        M5.update();
        pollKeyboard();
        delay(10);
    }
    waitAllKeysReleased();
}

String checkMAC()
{
    return WiFi.macAddress();
}

bool validateMACFormat(const String &mac)
{
    if (mac.length() != 17) return false;
    for (int i = 0; i < 17; i++) {
        if ((i + 1) % 3 == 0) {
            if (mac[i] != ':') return false;
        } else {
            if (!isxdigit(mac[i])) return false;
        }
    }
    return true;
}

bool setCustomMAC(const String &mac)
{
    if (!validateMACFormat(mac)) {
        displayError("Invalid MAC Format!", true);
        return false;
    }
    kitenConfig.setWifiMAC(mac);
    return true;
}

String generateRandomMAC()
{
    uint8_t mac[6];
    mac[0] = 0x02;
    for (int i = 1; i < 6; i++) mac[i] = random(0, 256);
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(buf);
}

void wifiMACMenu()
{
    String currentMAC;
    if (kitenConfig.wifiMAC != "" && validateMACFormat(kitenConfig.wifiMAC)) {
        currentMAC = kitenConfig.wifiMAC + " (Custom)";
    } else {
        currentMAC = WiFi.macAddress() + " (Default)";
    }

    displayInfo("Current MAC: " + currentMAC, true);

    std::vector<Option> opts = {
        {"Default MAC", []() {
            kitenConfig.setWifiMAC("");
            displayInfo("Default MAC set", true);
        }},
        {"Set MAC", []() {
            String newMAC = keyboard("", 17, "MAC XX:YY:ZZ:AA:BB:CC");
            if (newMAC == "\x1B") return;
            if (setCustomMAC(newMAC)) {
                displayInfo("MAC Saved: " + newMAC, true);
            }
        }},
        {"Random MAC", []() {
            String randMAC = generateRandomMAC();
            setCustomMAC(randMAC);
            displayInfo("Random MAC: " + randMAC, true);
        }},
        {"Back", [](){}},
    };
    loopOptions(opts, MENU_TYPE_REGULAR, "Change MAC");
}

void addEvilWifiMenu()
{
    String ssid = keyboard("", 32, "SSID to add");
    if (ssid == "\x1B") return;
    String pwd = keyboard("", 63, "Password", true);
    if (pwd == "\x1B") return;
    kitenConfig.addWifiCredential(ssid, pwd);
    displaySuccess("Added: " + ssid, true);
}

void removeEvilWifiMenu()
{
    
    auto ssids = kitenConfig.listWifiSSIDs();
    if (ssids.empty()) {
        displayInfo("No saved WiFi credentials.", true);
        return;
    }
    std::vector<Option> opts;
    for (const String &ssid : ssids) {
        
        String s = ssid;
        opts.push_back({s, [s]() {
            kitenConfig.removeWifiCredential(s);
            displaySuccess("Removed: " + s, true);
        }});
    }
    opts.push_back({"Back", [](){}});
    loopOptions(opts, MENU_TYPE_REGULAR, "Remove Evil Wifi");
}

static void evilWifiSettingsMenu()
{
    for (;;) {
        std::vector<Option> opts = {
            {"Set Gateway IP", []() {
                String v = keyboard(kitenConfig.evilGatewayIp, 15, "Gateway IP");
                if (v != "\x1B" && v.length() > 0) {
                    kitenConfig.setEvilGatewayIp(v);
                    displayInfo("Gateway IP: " + v, true);
                }
            }},
            {String("Password Mode: ") + (kitenConfig.evilPasswordMode ? "ON" : "OFF"), []() {
                kitenConfig.setEvilPasswordMode(!kitenConfig.evilPasswordMode);
                displayInfo(String("Password Mode: ") +
                            (kitenConfig.evilPasswordMode ? "ON" : "OFF"), true);
            }},
            {"Rename /creds", []() {
                String v = keyboard(kitenConfig.evilCredsPath, 32, "/creds path");
                if (v != "\x1B" && v.length() > 0) {
                    kitenConfig.setEvilCredsPath(v);
                    displayInfo("/creds -> " + v, true);
                }
            }},
            {String("Allow /creds access: ") + (kitenConfig.evilAllowGetCreds ? "ON" : "OFF"), []() {
                kitenConfig.setEvilAllowGetCreds(!kitenConfig.evilAllowGetCreds);
                displayInfo(String("Allow /creds: ") +
                            (kitenConfig.evilAllowGetCreds ? "ON" : "OFF"), true);
            }},
            {"Rename /ssid", []() {
                String v = keyboard(kitenConfig.evilSsidPath, 32, "/ssid path");
                if (v != "\x1B" && v.length() > 0) {
                    kitenConfig.setEvilSsidPath(v);
                    displayInfo("/ssid -> " + v, true);
                }
            }},
            {String("Allow /ssid access: ") + (kitenConfig.evilAllowSetSsid ? "ON" : "OFF"), []() {
                kitenConfig.setEvilAllowSetSsid(!kitenConfig.evilAllowSetSsid);
                displayInfo(String("Allow /ssid: ") +
                            (kitenConfig.evilAllowSetSsid ? "ON" : "OFF"), true);
            }},
            {String("Display endpoints: ") + (kitenConfig.evilAllowEndpointDisplay ? "ON" : "OFF"), []() {
                kitenConfig.setEvilAllowEndpointDisplay(!kitenConfig.evilAllowEndpointDisplay);
                displayInfo(String("Display endpoints: ") +
                            (kitenConfig.evilAllowEndpointDisplay ? "ON" : "OFF"), true);
            }},
            {"Back", [](){}},
        };
        int sel = loopOptions(opts, MENU_TYPE_SUBMENU, "Evil Wifi Settings");
        if (sel == -1 || sel == (int)opts.size() - 1) return;
    }
}

static void wifiConfigMenu()
{
    for (;;) {
        std::vector<Option> opts = {
            {"Change MAC",       wifiMACMenu},
            {"Add Evil Wifi",    addEvilWifiMenu},
            {"Remove Evil Wifi", removeEvilWifiMenu},
            
            {String("SSH/Telnet Log: ") + (kitenConfig.sshTelnetLog ? "ON" : "OFF"), []() {
                kitenConfig.setSshTelnetLog(!kitenConfig.sshTelnetLog);
                displayInfo(String("SSH/Telnet Log: ") +
                            (kitenConfig.sshTelnetLog ? "ON" : "OFF"), true);
            }},
            {"Evil Wifi Settings", []() { evilWifiSettingsMenu(); }},
            {String("Hidden Networks: ") + (showHiddenNetworks ? "ON" : "OFF"), []() {
                showHiddenNetworks = !showHiddenNetworks;
                displayInfo(String("Hidden Networks: ") +
                            (showHiddenNetworks ? "ON" : "OFF"), true);
            }},
            {"Back", [](){}},
        };
        int sel = loopOptions(opts, MENU_TYPE_SUBMENU, "WiFi Config");
        if (sel == -1 || sel == (int)opts.size() - 1) return;
    }
}

void evilPortal()
{
    
    
    
    extern void evilPortalImpl();
    evilPortalImpl();
}

void evilPortal(const String &tssid, uint8_t channel, bool deauth, bool verify)
{
    extern void evilPortalTargetedImpl(const String &, uint8_t, bool, bool);
    evilPortalTargetedImpl(tssid, channel, deauth, verify);
}

void snifferSetup()
{
    sniffer_setup();
}

void wifiMenuEntry()
{
    for (;;) {
        std::vector<Option> opts;

        
        if (WiFi.status() != WL_CONNECTED) {
            opts.push_back({"Connect to Wifi", []() { wifiConnectMenu(); }});
            opts.push_back({"Start WiFi AP", []() {
                startWiFiAP();
            }});
        }
        if (WiFi.getMode() != WIFI_MODE_NULL) {
            opts.push_back({"Turn Off WiFi", []() {
                wifiDisconnect();
                displayInfo("WiFi turned off", true);
            }});
        }
        if (WiFi.getMode() == WIFI_MODE_STA || WiFi.getMode() == WIFI_MODE_APSTA) {
            opts.push_back({"AP info", displayAPInfo});
        }

        
        opts.push_back({"Wifi Atks", []() { wifiAtkMenu(); }});
        opts.push_back({"Evil Portal", []() { evilPortal(); }});
        opts.push_back({"NetCut", []() { netcutMenu(); }});

        
        
        
        
        opts.push_back({"Listen TCP",   []() { listenTcpPort(); }});
        opts.push_back({"Client TCP",   []() { clientTCP();     }});
        opts.push_back({"SOCKS4 Proxy", []() { socks4Proxy(1080); }});
        opts.push_back({"TelNET",       []() { telnet_setup();  }});
        opts.push_back({"SSH",          []() { ssh_setup("");   }});
        opts.push_back({"Sniffer",      []() { snifferSetup();  }});
        opts.push_back({"WiFiScan",     []() { wifiscannerMenuEntry(); }});
        opts.push_back({"Scan Hosts",   []() { scanHosts();     }});
        opts.push_back({"Wireguard",    []() { wg_setup();      }});
        opts.push_back({"Responder",    []() { responder();     }});
        opts.push_back({"KITENgotchi",  []() { brucegotchi_start(); }});
        opts.push_back({"WiFi Pass Recovery", []() { wifi_recover_menu(); }});

        
        opts.push_back({"Config", []() { wifiConfigMenu(); }});

        
        opts.push_back({"Main Menu", [](){}});

        int sel = loopOptions(opts, MENU_TYPE_SUBMENU, "WiFi");
        if (sel == -1 || sel == (int)opts.size() - 1) return;
    }
}
