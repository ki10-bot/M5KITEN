#include "email_osint.h"
#include "config.h"
#include "settings.h"
#include "keyboard.h"
#include "ui.h"
#include "menu.h"
#include <Arduino.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <vector>
#include <String>

static bool parseUrl(const String &url, String &host, int &port, String &path) {
    if (!url.startsWith("https://")) return false;
    int s = 8;
    int slash = url.indexOf('/', s);
    if (slash == -1) slash = url.length();
    String hp = url.substring(s, slash);
    int colon = hp.indexOf(':');
    if (colon != -1) {
        host = hp.substring(0, colon);
        port = hp.substring(colon + 1).toInt();
    } else {
        host = hp;
        port = 443;
    }
    path = (slash < url.length()) ? url.substring(slash) : "/";
    return true;
}

static WiFiClientSecure sslClient;

static bool secureGet(const String &url, String &response, int timeout = 8000) {
    sslClient.stop();
    String host, path;
    int port;
    if (!parseUrl(url, host, port, path)) return false;
    sslClient.setInsecure();
    sslClient.setTimeout(timeout);
    if (!sslClient.connect(host.c_str(), port)) return false;
    sslClient.println("GET " + path + " HTTP/1.1");
    sslClient.println("Host: " + host);
    sslClient.println("Connection: close");
    sslClient.println("User-Agent: Mozilla/5.0 (compatible; EmailOSINT/1.0)");
    sslClient.println();
    unsigned long start = millis();
    while (!sslClient.available() && millis() - start < timeout) delay(10);
    response = "";
    while (sslClient.available()) {
        response += (char)sslClient.read();
        if (millis() - start > timeout) break;
    }
    sslClient.stop();
    return response.length() > 0;
}

static bool securePost(const String &url, const String &data, const String &contentType, String &response, int timeout = 8000) {
    sslClient.stop();
    String host, path;
    int port;
    if (!parseUrl(url, host, port, path)) return false;
    sslClient.setInsecure();
    sslClient.setTimeout(timeout);
    if (!sslClient.connect(host.c_str(), port)) return false;
    sslClient.println("POST " + path + " HTTP/1.1");
    sslClient.println("Host: " + host);
    sslClient.println("Connection: close");
    sslClient.println("Content-Type: " + contentType);
    sslClient.println("Content-Length: " + String(data.length()));
    sslClient.println("User-Agent: Mozilla/5.0 (compatible; EmailOSINT/1.0)");
    sslClient.println();
    sslClient.println(data);
    unsigned long start = millis();
    while (!sslClient.available() && millis() - start < timeout) delay(10);
    response = "";
    while (sslClient.available()) {
        response += (char)sslClient.read();
        if (millis() - start > timeout) break;
    }
    sslClient.stop();
    return response.length() > 0;
}

static bool checkGoogle(const String &email) {
    String postUrl = "https://accounts.google.com/_/signup/validateemail?hl=en";
    String data = "email=" + email + "&hl=en";
    String resp;
    if (!securePost(postUrl, data, "application/x-www-form-urlencoded", resp)) return false;
    if (resp.indexOf("\"valid\":false") >= 0) return true;
    if (resp.indexOf("\"valid\":true") >= 0) return false;
    return false;
}

static bool checkReddit(const String &email) {
    String url = "https://www.reddit.com/api/check_email.json";
    String data = "email=" + email;
    String resp;
    if (!securePost(url, data, "application/x-www-form-urlencoded", resp)) return false;
    resp.toLowerCase();
    if (resp.indexOf("true") >= 0) return true;
    if (resp.indexOf("false") >= 0) return false;
    return false;
}

static bool checkSnapchat(const String &email) {
    String url = "https://accounts.snapchat.com/accounts/check_email";
    String data = "email=" + email;
    String resp;
    if (!securePost(url, data, "application/x-www-form-urlencoded", resp)) return false;
    resp.toLowerCase();
    if (resp.indexOf("\"hasaccount\":true") >= 0) return true;
    if (resp.indexOf("\"hasaccount\":false") >= 0) return false;
    return false;
}

static bool checkPinterest(const String &email) {
    String url = "https://www.pinterest.com/resource/EmailExistsResource/get/";
    String data = "email=" + email;
    String resp;
    if (!securePost(url, data, "application/x-www-form-urlencoded", resp)) return false;
    resp.toLowerCase();
    if (resp.indexOf("\"email_exists\": true") >= 0) return true;
    if (resp.indexOf("\"email_exists\": false") >= 0) return false;
    return false;
}

static bool checkSpotify(const String &email) {
    String url = "https://www.spotify.com/api/signup/validate?email=" + email;
    String resp;
    if (!secureGet(url, resp)) return false;
    resp.toLowerCase();
    if (resp.indexOf("\"valid\":true") >= 0) return true;
    if (resp.indexOf("\"valid\":false") >= 0) return false;
    return false;
}

static bool checkGitHub(const String &email) {
    String url = "https://github.com/signup_check/email";
    String data = "value=" + email;
    String resp;
    if (!securePost(url, data, "application/x-www-form-urlencoded", resp)) return false;
    resp.toLowerCase();
    if (resp.indexOf("email is already taken") >= 0) return true;
    if (resp.indexOf("email is available") >= 0) return false;
    return false;
}

static bool checkAdobe(const String &email) {
    String url = "https://auth.services.adobe.com/signup/v2/users/email?email=" + email;
    String resp;
    if (!secureGet(url, resp)) return false;
    resp.toLowerCase();
    if (resp.indexOf("\"available\":false") >= 0) return true;
    if (resp.indexOf("\"available\":true") >= 0) return false;
    return false;
}

static bool checkOutlook(const String &email) {
    String url = "https://signup.live.com/API/CheckAvailableSigninNames?signinName=" + email;
    String resp;
    if (!secureGet(url, resp)) return false;
    resp.toLowerCase();
    if (resp.indexOf("\"isavailable\":false") >= 0) return true;
    if (resp.indexOf("\"isavailable\":true") >= 0) return false;
    return false;
}

static bool checkDiscord(const String &email) {
    String url = "https://discord.com/api/v9/auth/register/email";
    String json = "{\"email\":\"" + email + "\"}";
    String resp;
    if (!securePost(url, json, "application/json", resp)) return false;
    resp.toLowerCase();
    if (resp.indexOf("email_already_registered") >= 0) return true;
    if (resp.startsWith("HTTP/1.1 200") || resp.indexOf("\"captcha_key\"")>=0) return false;
    return false;
}

static bool checkSteam(const String &email) {
    String url = "https://store.steampowered.com/join/checkemailavail/";
    String data = "email=" + email + "&count=1";
    String resp;
    if (!securePost(url, data, "application/x-www-form-urlencoded", resp)) return false;
    resp.toLowerCase();
    if (resp.indexOf("\"available\":false") >= 0) return true;
    if (resp.indexOf("\"available\":true") >= 0) return false;
    return false;
}

static bool checkTikTok(const String &email) {
    String url = "https://www.tiktok.com/passport/web/account/check/email/?email=" + email;
    String resp;
    if (!secureGet(url, resp)) return false;
    resp.toLowerCase();
    if (resp.indexOf("\"is_registered\":true") >= 0) return true;
    if (resp.indexOf("\"is_registered\":false") >= 0) return false;
    return false;
}

static bool checkAirbnb(const String &email) {
    String url = "https://www.airbnb.com/api/v2/email_exists";
    String data = "email=" + email;
    String resp;
    if (!securePost(url, data, "application/x-www-form-urlencoded", resp)) return false;
    resp.toLowerCase();
    if (resp.indexOf("true") >= 0) return true;
    if (resp.indexOf("false") >= 0) return false;
    return false;
}

static bool checkUdemy(const String &email) {
    String url = "https://www.udemy.com/api-2.0/checkout/email_status/";
    String data = "email=" + email;
    String resp;
    if (!securePost(url, data, "application/x-www-form-urlencoded", resp)) return false;
    resp.toLowerCase();
    if (resp.indexOf("registered") >= 0) return true;
    if (resp.indexOf("not_registered") >= 0) return false;
    return false;
}

static bool checkCoursera(const String &email) {
    String url = "https://www.coursera.org/api/login/v3";
    String json = "{\"email\":\"" + email + "\"}";
    String resp;
    if (!securePost(url, json, "application/json", resp)) return false;
    resp.toLowerCase();
    if (resp.indexOf("exists") >= 0) return true;
    if (resp.indexOf("does_not_exist") >= 0) return false;
    return false;
}

static bool checkWordpress(const String &email) {
    String url = "https://public-api.wordpress.com/rest/v1.1/users/email/exists?email=" + email;
    String resp;
    if (!secureGet(url, resp)) return false;
    resp.toLowerCase();
    if (resp.indexOf("true") >= 0) return true;
    if (resp.indexOf("false") >= 0) return false;
    return false;
}

static bool checkPatreon(const String &email) {
    String url = "https://www.patreon.com/api/auth/check_email";
    String json = "{\"email\":\"" + email + "\"}";
    String resp;
    if (!securePost(url, json, "application/json", resp)) return false;
    resp.toLowerCase();
    if (resp.indexOf("taken") >= 0) return true;
    if (resp.indexOf("available") >= 0) return false;
    return false;
}

static bool checkSoundCloud(const String &email) {
    String url = "https://api.soundcloud.com/users/email_available";
    String data = "user[email]=" + email;
    String resp;
    if (!securePost(url, data, "application/x-www-form-urlencoded", resp)) return false;
    resp.toLowerCase();
    if (resp.indexOf("false") >= 0) return true;
    if (resp.indexOf("true") >= 0) return false;
    return false;
}

static bool checkHulu(const String &email) {
    String url = "https://www.hulu.com/api/account/email";
    String json = "{\"email\":\"" + email + "\"}";
    String resp;
    if (!securePost(url, json, "application/json", resp)) return false;
    resp.toLowerCase();
    if (resp.indexOf("already") >= 0 || resp.indexOf("taken") >= 0) return true;
    if (resp.indexOf("available") >= 0) return false;
    return false;
}

static bool checkNetflix(const String &email) {
    String url = "https://www.netflix.com/api/shakti/mre/account/email/" + email;
    String resp;
    if (!secureGet(url, resp)) return false;
    resp.toLowerCase();
    if (resp.indexOf("\"exists\":true") >= 0) return true;
    if (resp.indexOf("\"exists\":false") >= 0) return false;
    return false;
}

void emailOsintMenuEntry() {
    String email = keyboard("", 64, "E-Mail-Adresse:");
    if (email == "\x1B" || email.length() == 0) return;

    M5.Display.fillScreen(kitenConfig.bgColor);
    drawMainBorder(false);
    M5.Display.setTextSize(FM);
    M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
    M5.Display.setCursor(BORDER_PAD_X, BORDER_PAD_Y);
    M5.Display.print("OSINT: " + email);
    M5.Display.drawFastHLine(BORDER_PAD_X, BORDER_PAD_Y + LH*FM + 2,
                             SCREEN_WIDTH - 2*BORDER_PAD_X, kitenConfig.secColor);

    int y = BORDER_PAD_Y + LH*FM + 6;
    M5.Display.setTextSize(FP);
    M5.Display.setTextColor(TFT_WHITE, kitenConfig.bgColor);

    if (WiFi.status() != WL_CONNECTED) {
        displayError("Kein WiFi!", true);
        return;
    }

    struct CheckItem {
        String name;
        bool (*func)(const String&);
    };
    std::vector<CheckItem> checks = {
        {"Google", checkGoogle},
        {"Reddit", checkReddit},
        {"Snapchat", checkSnapchat},
        {"Pinterest", checkPinterest},
        {"Spotify", checkSpotify},
        {"GitHub", checkGitHub},
        {"Adobe", checkAdobe},
        {"Outlook", checkOutlook},
        {"Discord", checkDiscord},
        {"Steam", checkSteam},
        {"TikTok", checkTikTok},
        {"Airbnb", checkAirbnb},
        {"Udemy", checkUdemy},
        {"Coursera", checkCoursera},
        {"WordPress", checkWordpress},
        {"Patreon", checkPatreon},
        {"SoundCloud", checkSoundCloud},
        {"Hulu", checkHulu},
        {"Netflix", checkNetflix}
    };

    std::vector<String> registered;
    std::vector<String> not_registered;
    std::vector<String> unclear;

    for (auto &c : checks) {
        M5.Display.setCursor(BORDER_PAD_X, y);
        M5.Display.print("Pruefe " + c.name + "...");
        M5.update(); pollKeyboard(); if (check(EscPress)) break;
        bool res = c.func(email);
        String status = "?";
        if (res) { registered.push_back(c.name); status = "[+]"; }
        else if (res == false) { not_registered.push_back(c.name); status = "[-]"; }
        else { unclear.push_back(c.name); status = "[?]"; }
        M5.Display.setCursor(BORDER_PAD_X + 130, y);
        M5.Display.print(status);
        y += LH+2;
        if (y > SCREEN_HEIGHT - 20) {
            y = BORDER_PAD_Y + LH*FM + 6;
            M5.Display.fillRect(BORDER_PAD_X, BORDER_PAD_Y + LH*FM + 6, SCREEN_WIDTH-2*BORDER_PAD_X, SCREEN_HEIGHT-BORDER_PAD_Y-LH*FM-6, kitenConfig.bgColor);
        }
    }

    M5.Display.fillScreen(kitenConfig.bgColor);
    drawMainBorder(false);
    y = BORDER_PAD_Y + LH*FM + 4;
    M5.Display.setTextSize(FM);
    M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
    M5.Display.setCursor(BORDER_PAD_X, y);
    M5.Display.print("Ergebnisse");
    y += LH*FM + 4;
    M5.Display.drawFastHLine(BORDER_PAD_X, y, SCREEN_WIDTH-2*BORDER_PAD_X, kitenConfig.secColor);
    y += 4;
    M5.Display.setTextSize(FP);
    if (!registered.empty()) {
        M5.Display.setTextColor(TFT_GREEN, kitenConfig.bgColor);
        for (auto &s : registered) {
            M5.Display.setCursor(BORDER_PAD_X, y); M5.Display.print("[+] " + s); y += LH+2;
        }
    }
    if (!not_registered.empty()) {
        M5.Display.setTextColor(TFT_RED, kitenConfig.bgColor);
        for (auto &s : not_registered) {
            M5.Display.setCursor(BORDER_PAD_X, y); M5.Display.print("[-] " + s); y += LH+2;
        }
    }
    if (!unclear.empty()) {
        M5.Display.setTextColor(TFT_YELLOW, kitenConfig.bgColor);
        for (auto &s : unclear) {
            M5.Display.setCursor(BORDER_PAD_X, y); M5.Display.print("[?] " + s); y += LH+2;
        }
    }
    displayInfo("Fertig. Taste druecken", true);
}