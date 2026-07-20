#pragma once
#include <Arduino.h>
#include <WiFi.h>

void wifiMenuEntry();

void wifiDisconnect();

bool wifiConnectMenu();

bool startWiFiAP();

void displayAPInfo();

String checkMAC();

void wifiAtkMenu();

void targetAtkMenu(const String &tssid, const String &mac, uint8_t channel);

void targetAtk(const String &tssid, const String &mac, uint8_t channel);

void captureHandshake(const String &tssid, const String &mac, uint8_t channel);

void karmaSetup();

void clonePortalOnly(const String &tssid, const String &mac, uint8_t channel);

void deauthCloneAttack(const String &tssid, const String &mac, uint8_t channel, bool verifyMode);

void evilPortal();

void evilPortal(const String &tssid, uint8_t channel, bool deauth, bool verify);

void snifferSetup();

void sniffer_setup();

void wifiMACMenu();
bool validateMACFormat(const String &mac);
bool setCustomMAC(const String &mac);
String generateRandomMAC();
void applyConfiguredMAC();

void addEvilWifiMenu();
void removeEvilWifiMenu();

void listenTcpPort();             
void clientTCP();                 
void socks4Proxy(uint16_t port);  
void telnet_setup();              
void ssh_setup(const String &host = "");  
void scanHosts();                 
void netcutMenu();                
void responder();                 
void wifi_recover_menu();         
void brucegotchi_start();         
void wg_setup();                  

extern bool showHiddenNetworks;

extern bool wifiConnected;
extern String wifiIP;
