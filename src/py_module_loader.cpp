

#include "py_interpreter.h"
#include "py_module_loader.h"
#include "config.h"
#include "sd_helper.h"
#include "wifi_menu.h"
#include "keyboard.h"
#include "ui.h"
#include <Arduino.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <SD.h>
#include <FS.h>

#define PYLIBS_DIR      "/pylibs"
#define PYLIBS_DIR_SL   "/pylibs/"
#define PYTHON_USER_DIR "/python"

#define PY_MODULE_MAX_BYTES  65536   

extern void pyConsolePrint(const String &s);

static String readCachedModule(const String &name)
{
    if (!kitenSdBegin()) return "";

    String path1 = String(PYLIBS_DIR_SL) + name + ".py";
    if (kitenSdExists(path1)) {
        return kitenSdReadFile(path1, PY_MODULE_MAX_BYTES);
    }

    String path2 = String(PYLIBS_DIR_SL) + name + "/__init__.py";
    if (kitenSdExists(path2)) {
        return kitenSdReadFile(path2, PY_MODULE_MAX_BYTES);
    }

    
    String path3 = String(PYTHON_USER_DIR) + "/" + name + ".py";
    if (kitenSdExists(path3)) {
        return kitenSdReadFile(path3, PY_MODULE_MAX_BYTES);
    }

    return "";
}

static bool cacheModule(const String &name, const String &src)
{
    if (!kitenSdBegin()) return false;
    
    if (!kitenSdExists(PYLIBS_DIR)) {
        SD.mkdir(PYLIBS_DIR);
    }
    String path = String(PYLIBS_DIR_SL) + name + ".py";
    return kitenSdWriteFile(path, src);
}

static String httpsGetBody(const String &url)
{
    WiFiClientSecure tls;
    tls.setInsecure();              
    tls.setTimeout(8000);

    HTTPClient http;
    if (!http.begin(tls, url)) return "";
    http.setTimeout(8000);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        http.end();
        return "";
    }
    String body = http.getString();
    http.end();
    if (body.length() == 0) return "";
    if (body.length() > PY_MODULE_MAX_BYTES) {
        body = body.substring(0, PY_MODULE_MAX_BYTES);
    }
    return body;
}

String pyLoadModule(const String &moduleName)
{
    String clean = moduleName;
    clean.replace(".", "/");    

    
    String cached = readCachedModule(clean);
    if (cached.length() > 0) return cached;

    
    if (clean == "math" || clean == "time" || clean == "sys" ||
        clean == "os"   || clean == "random" || clean == "json" ||
        clean == "re") {
        
        
        
        
        
        
        if (clean == "math") {
            
            
            return String(
                "PI = 3.141592653589793\n"
                "E = 2.718281828459045\n"
                "def sqrt(x): return _sqrt(x)\n"
                "def sin(x):  return _sin(x)\n"
                "def cos(x):  return _cos(x)\n"
                "def tan(x):  return _tan(x)\n"
                "def log(x):  return _log(x)\n"
                "def log2(x): return _log2(x)\n"
                "def log10(x):return _log10(x)\n"
                "def exp(x):  return _exp(x)\n"
                "def floor(x):return _floor(x)\n"
                "def ceil(x): return _ceil(x)\n"
                "def pow(a,b):return _pow(a,b)\n"
            );
        }
        
        
        
        if (clean == "random") {
            
            
            return String(
                "def randint(a, b): return _randint(a, b)\n"
                "def choice(seq):   return _choice(seq)\n"
                "def random():      return _random_float()\n"
            );
        }
        return String("# KITEN built-in " + clean + " module stub\n");
    }

    
    
    
    if (clean == "struct"    || clean == "socket"    || clean == "threading" ||
        clean == "multiprocessing" || clean == "subprocess" || clean == "ctypes" ||
        clean == "asyncio"   || clean == "pickle"    || clean == "shutil" ||
        clean == "pathlib"   || clean == "io"        || clean == "select" ||
        clean == "signal"    || clean == "mmap"      || clean == "errno" ||
        clean == "fcntl"     || clean == "grp"       || clean == "pwd" ||
        clean == "resource"  || clean == "syslog"    || clean == "termios" ||
        clean == "tty"       || clean == "pty"       || clean == "readline" ||
        clean == "curses"    || clean == "crypt"     || clean == "dbm" ||
        clean == "sqlite3"   || clean == "xml"       || clean == "html" ||
        clean == "http"      || clean == "urllib"    || clean == "email" ||
        clean == "ftplib"    || clean == "smtplib"   || clean == "telnetlib" ||
        clean == "ssl"       || clean == "hashlib"   || clean == "hmac" ||
        clean == "secrets"   || clean == "csv"       || clean == "configparser" ||
        clean == "toml"      || clean == "xmlrpc"    || clean == "ipaddress" ||
        clean == "unittest"  || clean == "argparse"  || clean == "logging" ||
        clean == "traceback" || clean == "linecache" || clean == "tokenize" ||
        clean == "dis"       || clean == "inspect"   || clean == "ast" ||
        clean == "code"      || clean == "codeop"    || clean == "compile" ||
        clean == "compileall"|| clean == "py_compile"|| clean == "zipimport" ||
        clean == "pkgutil"   || clean == "modulefinder"|| clean == "importlib" ||
        clean == "platform"  || clean == "locale"    || clean == "gettext" ||
        clean == "base64"    || clean == "binascii"  || clean == "quopri" ||
        clean == "uu"        || clean == "unicodedata") {
        pyConsolePrint("Module '" + clean + "' not available on ESP32.\n");
        return String("# stub: " + clean + " not available\n");
    }

    
    if (!wifiConnected) {
        pyConsolePrint("Module '" + clean + "' not on SD.\n"
                       "Connect WiFi to auto-install.\n");
        return "";
    }

    pyConsolePrint("Installing " + clean + "...\n");

    
    const char *urlTemplates[] = {
        "https://raw.githubusercontent.com/micropython/micropython-lib/master/python-stdlib/{m}/__init__.py",
        "https://raw.githubusercontent.com/micropython/micropython-lib/master/python-stdlib/{m}/{m}.py",
        "https://raw.githubusercontent.com/micropython/micropython-lib/master/micropython/{m}/{m}.py",
        "https://raw.githubusercontent.com/micropython/micropython-lib/master/python-stdlib/{m}.py",
        
        "https://raw.githubusercontent.com/micropython/micropython-lib/master/python-stdlib/{m}/{m}.py",
    };

    String cleanLeaf = clean;
    int slash = cleanLeaf.lastIndexOf('/');
    if (slash >= 0) cleanLeaf = cleanLeaf.substring(slash + 1);

    for (const char *tmpl : urlTemplates) {
        String url = tmpl;
        url.replace("{m}", clean);
        
        String body = httpsGetBody(url);
        if (body.length() == 0) continue;

        
        if (body.startsWith("404:")) continue;
        if (body.indexOf("Not Found") == 0) continue;

        
        if (cacheModule(clean, body)) {
            pyConsolePrint("Installed " + clean + " (" + String(body.length()) + " B)\n");
        } else {
            pyConsolePrint("Fetched " + clean + " but SD write failed\n");
        }
        return body;
    }

    pyConsolePrint("Module '" + clean + "' not found on GitHub.\n");
    return "";
}
