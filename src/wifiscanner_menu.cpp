

#include "wifiscanner_menu.h"
#include "config.h"
#include "settings.h"
#include "keyboard.h"
#include "ui.h"
#include "menu.h"
#include <Arduino.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <vector>

#define MAX_DEVICES 150

enum VendorHint { HINT_NONE=0, HINT_APPLE=1, HINT_WINDOWS=2, HINT_ANDROID=3 };

struct RawDevice {
    uint8_t mac[6];
    uint8_t ip[4]; 
    uint16_t port; 
    bool isEncrypted;
    bool hasIp;
    VendorHint hint;
};

struct FinalDevice {
    String macStr;
    String ipStr;
    String portStr;
    String deviceName;
    String osName;
    String privacyInfo;
};

static int s_selected_network_index = -1;
static std::vector<FinalDevice> s_final_devices;
static RawDevice s_raw_devices[MAX_DEVICES];
static int s_raw_device_count = 0;

struct OuiEntry {
    const char* prefix;
    const char* device;
    const char* os;
};

static const OuiEntry ouiDatabase[] = {
    
    {"A4:83:E7", "Apple Device", "iOS/MacOS"}, {"00:1C:B3", "Apple Device", "iOS/MacOS"},
    {"3C:22:FB", "Apple Device", "iOS/MacOS"}, {"F0:18:98", "Apple Device", "iOS/MacOS"},
    {"00:1D:E1", "Apple Device", "iOS/MacOS"}, {"00:24:36", "Apple Device", "iOS/MacOS"},
    {"00:25:00", "Apple Device", "iOS/MacOS"}, {"00:25:9B", "Apple Device", "iOS/MacOS"},
    {"00:26:08", "Apple Device", "iOS/MacOS"}, {"00:26:BB", "Apple Device", "iOS/MacOS"},
    {"00:50:E4", "Apple Device", "iOS/MacOS"}, {"00:A1:50", "Apple Device", "iOS/MacOS"},
    {"04:0C:CE", "Apple Device", "iOS/MacOS"}, {"04:15:52", "Apple Device", "iOS/MacOS"},
    {"04:1E:64", "Apple Device", "iOS/MacOS"}, {"04:26:65", "Apple Device", "iOS/MacOS"},
    {"04:4B:ED", "Apple Device", "iOS/MacOS"}, {"04:54:53", "Apple Device", "iOS/MacOS"},
    {"0C:30:21", "Apple Device", "iOS/MacOS"}, {"0C:4D:E9", "Apple Device", "iOS/MacOS"},
    {"0C:74:C2", "Apple Device", "iOS/MacOS"}, {"0C:77:1A", "Apple Device", "iOS/MacOS"},
    {"10:1C:0C", "Apple Device", "iOS/MacOS"}, {"10:9A:DD", "Apple Device", "iOS/MacOS"},
    {"10:DD:B1", "Apple Device", "iOS/MacOS"}, {"14:10:9F", "Apple Device", "iOS/MacOS"},
    {"14:5A:05", "Apple Device", "iOS/MacOS"}, {"14:8F:C6", "Apple Device", "iOS/MacOS"},
    {"14:99:E2", "Apple Device", "iOS/MacOS"}, {"18:34:51", "Apple Device", "iOS/MacOS"},
    {"18:AF:61", "Apple Device", "iOS/MacOS"}, {"18:E7:F4", "Apple Device", "iOS/MacOS"},
    {"1C:1A:70", "Apple Device", "iOS/MacOS"}, {"1C:91:80", "Apple Device", "iOS/MacOS"},
    {"1C:AB:A7", "Apple Device", "iOS/MacOS"}, {"1C:BA:8C", "Apple Device", "iOS/MacOS"},
    {"20:3C:AE", "Apple Device", "iOS/MacOS"}, {"20:4D:07", "Apple Device", "iOS/MacOS"},
    {"28:37:37", "Apple Device", "iOS/MacOS"}, {"28:6A:BA", "Apple Device", "iOS/MacOS"},
    {"28:CF:DA", "Apple Device", "iOS/MacOS"}, {"28:E0:2C", "Apple Device", "iOS/MacOS"},
    {"34:15:9E", "Apple Device", "iOS/MacOS"}, {"34:51:C9", "Apple Device", "iOS/MacOS"},
    {"34:A8:4E", "Apple Device", "iOS/MacOS"}, {"38:0F:4A", "Apple Device", "iOS/MacOS"},
    {"38:48:4C", "Apple Device", "iOS/MacOS"}, {"38:66:F0", "Apple Device", "iOS/MacOS"},
    {"38:C9:86", "Apple Device", "iOS/MacOS"}, {"3C:07:54", "Apple Device", "iOS/MacOS"},
    {"3C:15:C2", "Apple Device", "iOS/MacOS"}, {"3C:AB:8E", "Apple Device", "iOS/MacOS"},
    {"3C:D0:F8", "Apple Device", "iOS/MacOS"}, {"40:33:1A", "Apple Device", "iOS/MacOS"},
    {"40:4D:7F", "Apple Device", "iOS/MacOS"}, {"40:6C:8F", "Apple Device", "iOS/MacOS"},
    {"40:8D:5C", "Apple Device", "iOS/MacOS"}, {"40:A6:D9", "Apple Device", "iOS/MacOS"},
    {"40:B3:CD", "Apple Device", "iOS/MacOS"}, {"40:B8:37", "Apple Device", "iOS/MacOS"},
    {"40:BC:60", "Apple Device", "iOS/MacOS"}, {"40:D3:2D", "Apple Device", "iOS/MacOS"},
    {"44:4C:0C", "Apple Device", "iOS/MacOS"}, {"44:65:0D", "Apple Device", "iOS/MacOS"},
    {"48:24:76", "Apple Device", "iOS/MacOS"}, {"48:3B:38", "Apple Device", "iOS/MacOS"},
    {"48:43:7C", "Apple Device", "iOS/MacOS"}, {"48:60:BC", "Apple Device", "iOS/MacOS"},
    {"48:74:6E", "Apple Device", "iOS/MacOS"}, {"48:A1:95", "Apple Device", "iOS/MacOS"},
    {"48:C6:70", "Apple Device", "iOS/MacOS"}, {"48:D7:8E", "Apple Device", "iOS/MacOS"},
    {"4C:32:75", "Apple Device", "iOS/MacOS"}, {"4C:57:CA", "Apple Device", "iOS/MacOS"},
    {"4C:74:BF", "Apple Device", "iOS/MacOS"}, {"4C:79:37", "Apple Device", "iOS/MacOS"},
    {"50:32:75", "Apple Device", "iOS/MacOS"}, {"50:7A:55", "Apple Device", "iOS/MacOS"},
    {"50:EA:D6", "Apple Device", "iOS/MacOS"}, {"54:26:96", "Apple Device", "iOS/MacOS"},
    {"54:4E:90", "Apple Device", "iOS/MacOS"}, {"54:6F:73", "Apple Device", "iOS/MacOS"},
    {"54:72:08", "Apple Device", "iOS/MacOS"}, {"54:9F:13", "Apple Device", "iOS/MacOS"},
    {"54:AF:97", "Apple Device", "iOS/MacOS"}, {"54:E4:BD", "Apple Device", "iOS/MacOS"},
    {"58:1F:AA", "Apple Device", "iOS/MacOS"}, {"58:40:4E", "Apple Device", "iOS/MacOS"},
    {"58:7F:57", "Apple Device", "iOS/MacOS"}, {"58:B0:35", "Apple Device", "iOS/MacOS"},
    {"5C:5F:67", "Apple Device", "iOS/MacOS"}, {"5C:8E:33", "Apple Device", "iOS/MacOS"},
    {"5C:96:9D", "Apple Device", "iOS/MacOS"}, {"5C:97:D3", "Apple Device", "iOS/MacOS"},
    {"5C:9A:D8", "Apple Device", "iOS/MacOS"}, {"5C:AD:CF", "Apple Device", "iOS/MacOS"},
    {"60:03:08", "Apple Device", "iOS/MacOS"}, {"60:08:8F", "Apple Device", "iOS/MacOS"},
    {"60:1D:71", "Apple Device", "iOS/MacOS"}, {"60:33:4B", "Apple Device", "iOS/MacOS"},
    {"60:35:6D", "Apple Device", "iOS/MacOS"}, {"60:69:44", "Apple Device", "iOS/MacOS"},
    {"60:7E:DD", "Apple Device", "iOS/MacOS"}, {"60:8C:4A", "Apple Device", "iOS/MacOS"},
    {"60:92:17", "Apple Device", "iOS/MacOS"}, {"60:D9:C7", "Apple Device", "iOS/MacOS"},
    {"64:20:0C", "Apple Device", "iOS/MacOS"}, {"64:4B:F0", "Apple Device", "iOS/MacOS"},
    {"64:5A:ED", "Apple Device", "iOS/MacOS"}, {"64:76:9B", "Apple Device", "iOS/MacOS"},
    {"64:9A:BE", "Apple Device", "iOS/MacOS"}, {"64:A3:CB", "Apple Device", "iOS/MacOS"},
    {"64:B0:A6", "Apple Device", "iOS/MacOS"}, {"64:B9:E8", "Apple Device", "iOS/MacOS"},
    {"64:ED:97", "Apple Device", "iOS/MacOS"}, {"68:09:27", "Apple Device", "iOS/MacOS"},
    {"68:17:29", "Apple Device", "iOS/MacOS"}, {"68:5B:35", "Apple Device", "iOS/MacOS"},
    {"68:5D:43", "Apple Device", "iOS/MacOS"}, {"68:96:9F", "Apple Device", "iOS/MacOS"},
    {"68:9C:70", "Apple Device", "iOS/MacOS"}, {"68:A8:6D", "Apple Device", "iOS/MacOS"},
    {"68:DB:CA", "Apple Device", "iOS/MacOS"}, {"68:FE:F7", "Apple Device", "iOS/MacOS"},
    {"6C:19:C0", "Apple Device", "iOS/MacOS"}, {"6C:40:08", "Apple Device", "iOS/MacOS"},
    {"6C:4A:0F", "Apple Device", "iOS/MacOS"}, {"6C:70:9F", "Apple Device", "iOS/MacOS"},
    {"6C:72:E7", "Apple Device", "iOS/MacOS"}, {"6C:8B:58", "Apple Device", "iOS/MacOS"},
    {"6C:AB:31", "Apple Device", "iOS/MacOS"}, {"6C:B7:8B", "Apple Device", "iOS/MacOS"},

    
    {"70:14:A6", "Samsung Device", "Android"}, {"CC:BA:CC", "Samsung Device", "Android"},
    {"48:43:77", "Samsung Device", "Android"}, {"FC:01:7C", "Samsung Device", "Android"},
    {"00:12:FB", "Samsung Device", "Android"}, {"00:15:99", "Samsung Device", "Android"},
    {"00:16:6B", "Samsung Device", "Android"}, {"00:17:C9", "Samsung Device", "Android"},
    {"00:1A:3F", "Samsung Device", "Android"}, {"00:1B:98", "Samsung Device", "Android"},
    {"00:1D:F6", "Samsung Device", "Android"}, {"00:1E:7C", "Samsung Device", "Android"},
    {"00:21:19", "Samsung Device", "Android"}, {"00:23:39", "Samsung Device", "Android"},
    {"00:24:54", "Samsung Device", "Android"}, {"00:25:38", "Samsung Device", "Android"},
    {"00:26:37", "Samsung Device", "Android"}, {"00:2A:07", "Samsung Device", "Android"},
    {"00:2F:B8", "Samsung Device", "Android"}, {"00:30:1A", "Samsung Device", "Android"},
    {"00:31:36", "Samsung Device", "Android"}, {"00:36:2C", "Samsung Device", "Android"},
    {"00:37:6D", "Samsung Device", "Android"}, {"00:50:06", "Samsung Device", "Android"},
    {"00:7E:3C", "Samsung Device", "Android"}, {"00:8E:36", "Samsung Device", "Android"},
    {"00:91:DF", "Samsung Device", "Android"}, {"00:A0:DE", "Samsung Device", "Android"},
    {"00:C0:24", "Samsung Device", "Android"}, {"00:D0:AF", "Samsung Device", "Android"},
    {"00:E0:23", "Samsung Device", "Android"}, {"00:E8:98", "Samsung Device", "Android"},
    {"04:A1:51", "Samsung Device", "Android"}, {"08:11:17", "Samsung Device", "Android"},
    {"08:21:32", "Samsung Device", "Android"}, {"0C:1D:AF", "Samsung Device", "Android"},
    {"0C:48:85", "Samsung Device", "Android"}, {"0C:96:E6", "Samsung Device", "Android"},
    {"0C:AC:8E", "Samsung Device", "Android"}, {"10:4A:F2", "Samsung Device", "Android"},
    {"10:68:3F", "Samsung Device", "Android"}, {"14:1F:BA", "Samsung Device", "Android"},
    {"14:35:7D", "Samsung Device", "Android"}, {"14:7F:62", "Samsung Device", "Android"},
    {"14:8C:25", "Samsung Device", "Android"}, {"14:A0:83", "Samsung Device", "Android"},
    {"14:B4:01", "Samsung Device", "Android"}, {"1C:62:B8", "Samsung Device", "Android"},
    {"1C:9A:3B", "Samsung Device", "Android"}, {"20:02:AF", "Samsung Device", "Android"},
    {"20:34:FB", "Samsung Device", "Android"}, {"24:2A:C1", "Samsung Device", "Android"},
    {"24:A0:74", "Samsung Device", "Android"}, {"28:1A:4B", "Samsung Device", "Android"},
    {"28:CC:01", "Samsung Device", "Android"}, {"2C:0E:3D", "Samsung Device", "Android"},
    {"2C:1F:23", "Samsung Device", "Android"}, {"2C:44:FD", "Samsung Device", "Android"},
    {"2C:BE:08", "Samsung Device", "Android"}, {"30:39:F2", "Samsung Device", "Android"},
    {"30:7A:6A", "Samsung Device", "Android"}, {"30:8A:97", "Samsung Device", "Android"},
    {"34:14:5E", "Samsung Device", "Android"}, {"34:80:B3", "Samsung Device", "Android"},
    {"38:2D:6F", "Samsung Device", "Android"}, {"3C:5A:37", "Samsung Device", "Android"},
    {"3C:8B:43", "Samsung Device", "Android"}, {"3C:E9:F7", "Samsung Device", "Android"},
    {"40:0E:85", "Samsung Device", "Android"}, {"40:43:08", "Samsung Device", "Android"},
    {"40:B8:9A", "Samsung Device", "Android"}, {"44:6C:3A", "Samsung Device", "Android"},
    {"48:5A:B6", "Samsung Device", "Android"}, {"4C:24:98", "Samsung Device", "Android"},
    {"4C:66:41", "Samsung Device", "Android"}, {"50:1E:2D", "Samsung Device", "Android"},
    {"50:32:75", "Samsung Device", "Android"}, {"50:56:67", "Samsung Device", "Android"},
    {"50:7A:55", "Samsung Device", "Android"}, {"54:88:0E", "Samsung Device", "Android"},
    {"58:03:5B", "Samsung Device", "Android"}, {"58:2A:F7", "Samsung Device", "Android"},
    {"5C:0A:5B", "Samsung Device", "Android"}, {"5C:C5:D4", "Samsung Device", "Android"},
    {"60:21:01", "Samsung Device", "Android"}, {"60:A4:D0", "Samsung Device", "Android"},
    {"64:16:66", "Samsung Device", "Android"}, {"68:3E:26", "Samsung Device", "Android"},
    {"68:94:23", "Samsung Device", "Android"}, {"6C:4D:73", "Samsung Device", "Android"},
    {"6C:C2:17", "Samsung Device", "Android"}, {"70:1C:E7", "Samsung Device", "Android"},
    {"70:3A:CB", "Samsung Device", "Android"}, {"70:72:3C", "Samsung Device", "Android"},
    {"74:DA:38", "Samsung Device", "Android"}, {"78:25:AD", "Samsung Device", "Android"},
    {"7C:1C:4E", "Samsung Device", "Android"}, {"7C:4F:87", "Samsung Device", "Android"},
    {"80:1F:02", "Samsung Device", "Android"}, {"84:25:3F", "Samsung Device", "Android"},
    {"88:32:9B", "Samsung Device", "Android"}, {"8C:77:12", "Samsung Device", "Android"},
    {"90:18:7C", "Samsung Device", "Android"}, {"90:7A:58", "Samsung Device", "Android"},
    {"94:35:0A", "Samsung Device", "Android"}, {"98:0C:A5", "Samsung Device", "Android"},
    {"9C:2A:70", "Samsung Device", "Android"}, {"A0:10:81", "Samsung Device", "Android"},
    {"A0:8D:16", "Samsung Device", "Android"}, {"A4:08:EA", "Samsung Device", "Android"},
    {"A8:F2:58", "Samsung Device", "Android"}, {"AC:5F:3E", "Samsung Device", "Android"},
    {"B0:09:DA", "Samsung Device", "Android"}, {"B0:48:1A", "Samsung Device", "Android"},
    {"B4:75:0E", "Samsung Device", "Android"}, {"B8:62:1F", "Samsung Device", "Android"},
    {"BC:47:60", "Samsung Device", "Android"}, {"C0:48:E6", "Samsung Device", "Android"},
    {"C4:43:8F", "Samsung Device", "Android"}, {"C8:69:CD", "Samsung Device", "Android"},
    {"CC:F3:A5", "Samsung Device", "Android"}, {"D0:03:4B", "Samsung Device", "Android"},
    {"D0:37:25", "Samsung Device", "Android"}, {"D4:87:D8", "Samsung Device", "Android"},
    {"D8:50:E6", "Samsung Device", "Android"}, {"DC:9F:53", "Samsung Device", "Android"},
    {"E0:9D:31", "Samsung Device", "Android"}, {"E4:D3:F1", "Samsung Device", "Android"},
    {"E8:50:8B", "Samsung Device", "Android"}, {"EC:9B:8B", "Samsung Device", "Android"},
    {"F0:5C:77", "Samsung Device", "Android"}, {"F4:9F:F3", "Samsung Device", "Android"},
    {"F8:77:B8", "Samsung Device", "Android"}, {"FC:8F:90", "Samsung Device", "Android"},

    
    {"F8:8A:5E", "Google Pixel", "Android"}, {"64:16:66", "Google Pixel", "Android"},
    {"5C:5F:67", "Google Pixel", "Android"}, {"7C:11:BE", "Google Pixel", "Android"},
    {"48:D6:D5", "Google Pixel", "Android"}, {"F0:72:82", "Google Pixel", "Android"},

    
    {"FC:34:97", "Microsoft Surface", "Windows"}, {"00:1D:D8", "Microsoft Device", "Windows"},
    {"00:0D:3A", "Microsoft Device", "Windows"}, {"28:18:78", "Microsoft Device", "Windows"},
    {"4C:0B:BE", "Microsoft Device", "Windows"}, {"50:1A:C5", "Microsoft Device", "Windows"},
    {"5C:E0:C5", "Microsoft Device", "Windows"}, {"60:67:20", "Microsoft Device", "Windows"},
    {"70:4F:57", "Microsoft Device", "Windows"}, {"8C:DE:52", "Microsoft Device", "Windows"},
    {"A4:DF:7F", "Microsoft Device", "Windows"}, {"B0:68:52", "Microsoft Device", "Windows"},
    {"BC:83:85", "Microsoft Device", "Windows"}, {"DC:71:96", "Microsoft Device", "Windows"},
    {"E4:AD:48", "Microsoft Device", "Windows"},

    
    {"D8:FC:93", "Dell Device", "Windows/Linux"}, {"00:1D:09", "Dell Device", "Windows/Linux"},
    {"04:69:F8", "Intel Device", "Linux/Windows"}, {"00:1A:6B", "Intel Device", "Linux/Windows"},
    {"A0:88:B4", "Intel Device", "Linux/Windows"}, {"F8:1E:DF", "TUXEDO/Asus", "Linux/Ubuntu"},
    {"04:7C:16", "Framework Laptop", "Linux/Win"}, {"F4:9E:AB", "Framework Laptop", "Linux/Win"},

    
    {"7C:DD:90", "Bitmain Antminer", "ASIC OS"}, {"00:1E:06", "Bitmain Antminer", "ASIC OS"},
    {"0C:82:6D", "Bitmain Antminer", "ASIC OS"}, {"D8:37:65", "Bitmain Antminer", "ASIC OS"},
    {"00:0E:2D", "Goldshell Miner", "ASIC OS"}, {"EC:71:DB", "Goldshell Miner", "ASIC OS"},
    {"00:09:F3", "Whatsminer (MicroBT)", "ASIC OS"}, {"00:25:90", "Supermicro (Miner)", "ASIC OS"},
    {"0C:CD:2E", "Supermicro (Miner)", "ASIC OS"}, {"3C:EC:EF", "Supermicro (Miner)", "ASIC OS"},
    {"00:0A:35", "Xilinx (Antminer BB)", "ASIC OS"}, 
    {"52:54:00", "Miner/VM (QEMU)", "Linux/ASIC"},  
    {"B8:27:EB", "Raspberry Pi", "Linux"}, {"D8:3A:DD", "Raspberry Pi", "Linux"},
    {"E4:5F:01", "Raspberry Pi", "Linux"}, {"DC:A6:32", "Raspberry Pi", "Linux"},
    {"28:CD:C1", "Raspberry Pi", "Linux"},

    
    {"08:8D:C0", "Huawei/Xiaomi", "Android/HarmonyOS"}, {"00:25:38", "Huawei/Xiaomi", "Android/HarmonyOS"},
    {"00:9A:CD", "Huawei/Xiaomi", "Android/HarmonyOS"}, {"04:02:1F", "Huawei/Xiaomi", "Android/HarmonyOS"}
};
static const int ouiDbSize = sizeof(ouiDatabase) / sizeof(ouiDatabase[0]);

static void IRAM_ATTR snifferCallback(void* buf, wifi_promiscuous_pkt_type_t type) {
    if (type != WIFI_PKT_DATA && type != WIFI_PKT_MGMT) return;
    if (s_raw_device_count >= MAX_DEVICES) return;
    
    const wifi_promiscuous_pkt_t *packet = (wifi_promiscuous_pkt_t *)buf;
    if (packet->rx_ctrl.sig_len < 24) return;

    const uint8_t *payload = packet->payload;
    bool protected_frame = (payload[1] & 0x40) != 0;
    uint8_t frame_type = (payload[0] & 0x0C) >> 2;
    
    uint8_t* macs[2] = { (uint8_t*)(payload + 4), (uint8_t*)(payload + 10) };
    
    for(int m=0; m<2; m++) {
        if(macs[m][0] == 0xFF && macs[m][1] == 0xFF && macs[m][2] == 0xFF) continue;
        if(macs[m][0] == 0x01 && macs[m][1] == 0x00 && macs[m][2] == 0x5E) continue;
        if(macs[m][0] == 0x00 && macs[m][1] == 0x00 && macs[m][2] == 0x00) continue;
        
        int existing_idx = -1;
        for(int i=0; i<s_raw_device_count; i++) {
            if(memcmp(s_raw_devices[i].mac, macs[m], 6) == 0) {
                existing_idx = i;
                break;
            }
        }

        if (existing_idx == -1) {
            existing_idx = s_raw_device_count;
            memcpy(s_raw_devices[existing_idx].mac, macs[m], 6);
            s_raw_devices[existing_idx].isEncrypted = protected_frame;
            s_raw_devices[existing_idx].hasIp = false;
            s_raw_devices[existing_idx].port = 0;
            s_raw_devices[existing_idx].hint = HINT_NONE;
            memset(s_raw_devices[existing_idx].ip, 0, 4);
            s_raw_device_count++;
            if (s_raw_device_count >= MAX_DEVICES) return;
        }

        
        
        if (frame_type == 0 && s_raw_devices[existing_idx].hint == HINT_NONE) {
            int len = packet->rx_ctrl.sig_len;
            for (int i = 24; i < len - 5; i++) {
                if (payload[i] == 0xDD && payload[i+2] == 0x00 && payload[i+3] == 0x17 && payload[i+4] == 0xF2) {
                    s_raw_devices[existing_idx].hint = HINT_APPLE;
                }
            }
        }

        
        if (!protected_frame && !s_raw_devices[existing_idx].hasIp && packet->rx_ctrl.sig_len >= 54) {
            int offset = 24;
            if (payload[0] & 0x80) offset += 2; 
            
            if (offset + 8 <= packet->rx_ctrl.sig_len &&
                payload[offset] == 0xAA && payload[offset+1] == 0xAA && payload[offset+2] == 0x03 &&
                payload[offset+6] == 0x08 && payload[offset+7] == 0x00) {
                
                int ip_hdr_offset = offset + 8;
                if (ip_hdr_offset + 20 <= packet->rx_ctrl.sig_len) {
                    memcpy(s_raw_devices[existing_idx].ip, payload + ip_hdr_offset + 12, 4);
                    uint8_t protocol = payload[ip_hdr_offset + 9];
                    s_raw_devices[existing_idx].hasIp = true;
                    
                    if (protocol == 6 || protocol == 17) { 
                        int transport_offset = ip_hdr_offset + 20;
                        if (transport_offset + 4 <= packet->rx_ctrl.sig_len) {
                            s_raw_devices[existing_idx].port = (payload[transport_offset] << 8) | payload[transport_offset + 1];
                        }
                    }
                }
            }
        }
    }
}

static void selectNetwork() {
    M5.Display.fillScreen(kitenConfig.bgColor);
    drawMainBorder(false);
    M5.Display.setTextSize(FP);
    M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
    M5.Display.setCursor(BORDER_PAD_X, BORDER_PAD_Y);
    M5.Display.print("Scanning APs...");

    int n = WiFi.scanNetworks();
    if (n == 0) {
        displayError("No networks found!", true);
        return;
    }

    std::vector<Option> opts;
    for (int i = 0; i < n; ++i) {
        String ssid = WiFi.SSID(i);
        int ch = WiFi.channel(i);
        opts.push_back({ssid + " (CH:" + String(ch) + ")", [i]() { s_selected_network_index = i; }});
    }
    addBackItem(opts);

    int sel = loopOptions(opts, MENU_TYPE_SUBMENU, "Select AP");
    if (sel == -1 || sel == (int)opts.size() - 1) {
        s_selected_network_index = -1;
    }
}

static void processFoundDevices() {
    s_final_devices.clear();
    for(int i=0; i<s_raw_device_count; i++) {
        FinalDevice fd;
        
        char macStr[18];
        snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X", 
                 s_raw_devices[i].mac[0], s_raw_devices[i].mac[1], s_raw_devices[i].mac[2], 
                 s_raw_devices[i].mac[3], s_raw_devices[i].mac[4], s_raw_devices[i].mac[5]);
        fd.macStr = String(macStr);
        
        String prefix = fd.macStr.substring(0, 8);
        fd.deviceName = "Unknown Device";
        fd.osName = "Unknown OS";
        fd.privacyInfo = "Standard MAC";

        char p = fd.macStr.charAt(1);
        bool is_private = (p == '2' || p == '6' || p == 'A' || p == 'E');

        if (!is_private) {
            for (int j = 0; j < ouiDbSize; j++) {
                if (prefix.equalsIgnoreCase(ouiDatabase[j].prefix)) {
                    fd.deviceName = ouiDatabase[j].device;
                    fd.osName = ouiDatabase[j].os;
                    break;
                }
            }
        } else {
            fd.privacyInfo = "Private/Random MAC";
        }

        
        if (s_raw_devices[i].hasIp && s_raw_devices[i].port != 0) {
            uint16_t port = s_raw_devices[i].port;
            if (port == 4028 || port == 4029) {
                fd.deviceName = "ASIC Miner (API)";
                fd.osName = "ASIC OS";
                fd.privacyInfo = "Identified via Port 4028";
            } else if (port == 3333 || port == 4444 || port == 8888 || port == 14444 || port == 25) {
                fd.deviceName = "Crypto Miner (Stratum)";
                fd.osName = "Mining OS";
                fd.privacyInfo = "Identified via Mining Port";
            } else if (port == 22 && fd.deviceName == "Unknown Device") {
                fd.deviceName = "SSH Host (Linux/IoT)";
                fd.osName = "Linux/Unix";
            }
        }

        
        if (s_raw_devices[i].hint == HINT_APPLE) {
            if (is_private) fd.deviceName = "Private Apple Device"; else fd.deviceName = "Apple Device";
            fd.osName = "iOS/MacOS";
            if (is_private) fd.privacyInfo = "Identified via Apple IE";
        } else if (is_private) {
            fd.deviceName = "Private Device";
            fd.osName = "iOS/Android/Win";
        }

        if (s_raw_devices[i].isEncrypted) {
            fd.ipStr = "Enc";
            fd.portStr = "Enc";
        } else if (s_raw_devices[i].hasIp) {
            fd.ipStr = String(s_raw_devices[i].ip[0]) + "." + String(s_raw_devices[i].ip[1]) + "." + String(s_raw_devices[i].ip[2]) + "." + String(s_raw_devices[i].ip[3]);
            fd.portStr = (s_raw_devices[i].port == 0) ? ": -" : ": " + String(s_raw_devices[i].port);
        } else {
            fd.ipStr = "N/A";
            fd.portStr = ": -";
        }

        if (fd.ipStr.length() > 14) {
            fd.ipStr = fd.ipStr.substring(0, 13) + "~";
        }

        s_final_devices.push_back(fd);
    }
}

static void startSniffing() {
    if (s_selected_network_index < 0) return;

    String ssid = WiFi.SSID(s_selected_network_index);
    int channel = WiFi.channel(s_selected_network_index);
    
    s_raw_device_count = 0; 

    M5.Display.fillScreen(kitenConfig.bgColor);
    drawMainBorder(false);
    M5.Display.setTextSize(FP);
    M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
    M5.Display.setCursor(BORDER_PAD_X, BORDER_PAD_Y);
    M5.Display.print("Sniffing: " + ssid);
    M5.Display.setTextColor(TFT_WHITE, kitenConfig.bgColor);
    M5.Display.setCursor(BORDER_PAD_X, BORDER_PAD_Y + 12);
    M5.Display.print("Press ESC to stop");

    WiFi.disconnect();
    delay(100);
    esp_wifi_set_promiscuous(false);
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous_rx_cb(snifferCallback);
    esp_wifi_set_promiscuous(true);

    unsigned long startTime = millis();
    unsigned long lastUpdate = millis();
    
    while (millis() - startTime < 20000) {
        M5.update();
        pollKeyboard();
        
        if (millis() - lastUpdate > 500) {
            lastUpdate = millis();
            M5.Display.fillRect(BORDER_PAD_X, BORDER_PAD_Y + 24, 120, 10, kitenConfig.bgColor);
            M5.Display.setCursor(BORDER_PAD_X, BORDER_PAD_Y + 24);
            M5.Display.print("Found: " + String(s_raw_device_count));
        }

        if (check(EscPress)) break;
        delay(10);
    }

    esp_wifi_set_promiscuous(false);
    processFoundDevices();

    if (s_final_devices.size() == 0) {
        displayInfo("No devices found!\nNetwork might be idle.", true);
    } else {
        displaySuccess("Found " + String(s_final_devices.size()) + " devices!", true);
    }
}

static void showDeviceDetails(int index) {
    M5.Display.fillScreen(kitenConfig.bgColor);
    drawMainBorder(false);
    
    M5.Display.setTextSize(FP);
    M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
    M5.Display.setCursor(BORDER_PAD_X, BORDER_PAD_Y);
    M5.Display.print("Device Details");
    M5.Display.drawFastHLine(BORDER_PAD_X, BORDER_PAD_Y + LH * FP + 2, SCREEN_WIDTH - 2 * BORDER_PAD_X, kitenConfig.secColor);
    
    int y = BORDER_PAD_Y + LH * FP + 6;
    M5.Display.setTextColor(TFT_WHITE, kitenConfig.bgColor);
    
    FinalDevice d = s_final_devices[index];
    
    M5.Display.setCursor(BORDER_PAD_X, y); y += LH * FP;
    M5.Display.print("Dev: " + d.deviceName);
    
    M5.Display.setCursor(BORDER_PAD_X, y); y += LH * FP;
    M5.Display.print("OS:  " + d.osName);
    
    M5.Display.setCursor(BORDER_PAD_X, y); y += LH * FP;
    M5.Display.print("IP:  " + d.ipStr);
    
    M5.Display.setCursor(BORDER_PAD_X, y); y += LH * FP;
    M5.Display.print("Port:" + d.portStr);
    
    M5.Display.setCursor(BORDER_PAD_X, y); y += LH * FP;
    M5.Display.print("MAC: " + d.macStr);

    M5.Display.setCursor(BORDER_PAD_X, y); y += LH * FP;
    M5.Display.setTextColor(kitenConfig.secColor, kitenConfig.bgColor);
    M5.Display.print(d.privacyInfo);

    M5.Display.setCursor(BORDER_PAD_X, SCREEN_HEIGHT - LH * FP - 4);
    M5.Display.print("ESC: Back");

    waitAllKeysReleased();
    while (true) {
        M5.update(); pollKeyboard();
        if (check(EscPress) || check(SelPress)) {
            waitAllKeysReleased();
            return;
        }
        delay(5);
    }
}

static void viewDevices() {
    if (s_final_devices.size() == 0) return;
    
    int selectedIndex = 0;
    int scrollOffset = 0;
    int marqueeOffset = 0;
    unsigned long lastMarqueeTime = 0;
    
    int rowHeight = 10; 
    int tableStartY = 35;
    int maxVisible = (SCREEN_HEIGHT - tableStartY - 10) / rowHeight;
    
    
    int x_ip = BORDER_PAD_X;            
    int x_port = BORDER_PAD_X + 98;     
    int x_dev = BORDER_PAD_X + 135;     
    int w_dev = SCREEN_WIDTH - x_dev - BORDER_PAD_X; 
    
    auto drawBackground = [&]() {
        M5.Display.fillScreen(kitenConfig.bgColor);
        drawMainBorder(false);
        M5.Display.setTextSize(FP);
        M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
        M5.Display.setCursor(BORDER_PAD_X, BORDER_PAD_Y);
        M5.Display.print("Dev: " + String(s_final_devices.size()));
        
        M5.Display.setCursor(x_ip, tableStartY - 10);
        M5.Display.setTextColor(kitenConfig.secColor, kitenConfig.bgColor);
        M5.Display.print("IP");
        M5.Display.setCursor(x_port, tableStartY - 10);
        M5.Display.print("Port");
        M5.Display.setCursor(x_dev, tableStartY - 10);
        M5.Display.print("Device [OS]");
    };
    
    auto drawList = [&]() {
        for(int i=0; i<maxVisible && (scrollOffset+i) < (int)s_final_devices.size(); i++) {
            int idx = scrollOffset + i;
            int y = tableStartY + i * rowHeight;
            
            FinalDevice d = s_final_devices[idx];
            String devText = d.deviceName + " [" + d.osName + "]";
            
            M5.Display.setTextWrap(false);
            
            if (idx == selectedIndex) {
                M5.Display.fillRect(BORDER_PAD_X, y, SCREEN_WIDTH - 2*BORDER_PAD_X, rowHeight-1, kitenConfig.priColor);
                M5.Display.setTextColor(TFT_BLACK, kitenConfig.priColor);
            } else {
                M5.Display.fillRect(BORDER_PAD_X, y, SCREEN_WIDTH - 2*BORDER_PAD_X, rowHeight-1, kitenConfig.bgColor);
                M5.Display.setTextColor(TFT_WHITE, kitenConfig.bgColor);
            }
            
            M5.Display.setCursor(x_ip, y);
            M5.Display.print(d.ipStr);
            
            M5.Display.setCursor(x_port, y);
            M5.Display.print(d.portStr);
            
            M5.Display.setClipRect(x_dev, y, w_dev, rowHeight-1);
            if (idx == selectedIndex) {
                int textW = M5.Display.textWidth(devText);
                if (textW > w_dev) {
                    M5.Display.setCursor(x_dev - marqueeOffset, y);
                    M5.Display.print(devText + "   " + devText);
                } else {
                    M5.Display.setCursor(x_dev, y);
                    M5.Display.print(devText);
                }
            } else {
                M5.Display.setCursor(x_dev, y);
                M5.Display.print(devText);
            }
            M5.Display.clearClipRect();
        }
        
        M5.Display.fillRect(SCREEN_WIDTH - 20, SCREEN_HEIGHT - 10, 20, 10, kitenConfig.bgColor);
        M5.Display.setTextColor(kitenConfig.secColor, kitenConfig.bgColor);
        if (scrollOffset > 0) {
            M5.Display.setCursor(SCREEN_WIDTH - 20, SCREEN_HEIGHT - 10);
            M5.Display.print("^");
        }
        if (scrollOffset + maxVisible < (int)s_final_devices.size()) {
            M5.Display.setCursor(SCREEN_WIDTH - 10, SCREEN_HEIGHT - 10);
            M5.Display.print("v");
        }
    };
    
    drawBackground();
    drawList();
    waitAllKeysReleased();
    
    while (true) {
        M5.update();
        pollKeyboard();
        
        bool needsRedraw = false;
        
        if (check(NextPress) || check(DownPress)) {
            if (selectedIndex < (int)s_final_devices.size() - 1) {
                selectedIndex++;
                if (selectedIndex - scrollOffset >= maxVisible) scrollOffset++;
                marqueeOffset = 0;
                needsRedraw = true;
            }
        }
        if (check(PrevPress) || check(UpPress)) {
            if (selectedIndex > 0) {
                selectedIndex--;
                if (selectedIndex < scrollOffset) scrollOffset--;
                marqueeOffset = 0;
                needsRedraw = true;
            }
        }
        if (check(SelPress)) {
            showDeviceDetails(selectedIndex);
            drawBackground(); 
            needsRedraw = true;
            waitAllKeysReleased();
        }
        if (check(EscPress)) {
            waitAllKeysReleased();
            return;
        }
        
        if (millis() - lastMarqueeTime > 150) {
            lastMarqueeTime = millis();
            marqueeOffset += 2;
            String devText = s_final_devices[selectedIndex].deviceName + " [" + s_final_devices[selectedIndex].osName + "]";
            int textW = M5.Display.textWidth(devText) + 3*6; 
            if (marqueeOffset > textW) {
                marqueeOffset = 0;
            }
            needsRedraw = true;
        }
        
        if (needsRedraw) {
            drawList();
        }
        delay(5);
    }
}

void wifiscannerMenuEntry() {
    int sel = 0;
    std::vector<Option> opts;
    
    for (;;) {
        opts.clear();
        opts.push_back({"Select Network", [](){ selectNetwork(); }});
        
        if (s_selected_network_index >= 0) {
            String ssid = WiFi.SSID(s_selected_network_index);
            opts.push_back({"Start Sniffing", [](){ startSniffing(); }});
            if (s_final_devices.size() > 0) {
                opts.push_back({"View Devices (" + String(s_final_devices.size()) + ")", [](){ viewDevices(); }});
            }
        }
        
        opts.push_back({"Back", [](){}});
        
        sel = loopOptions(opts, MENU_TYPE_REGULAR, "Wi-Fi Sniffer");
        if (sel == -1 || sel == (int)opts.size() - 1) return;
    }
}