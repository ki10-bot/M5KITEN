

#include "sd_helper.h"
#include <SPI.h>
#include <SD.h>

static bool g_sdMounted = false;

bool kitenSdBegin()
{
    if (g_sdMounted) return true;
    SPI.begin(KITEN_SD_SPI_SCK_PIN, KITEN_SD_SPI_MISO_PIN,
              KITEN_SD_SPI_MOSI_PIN, KITEN_SD_SPI_CS_PIN);
    if (!SD.begin(KITEN_SD_SPI_CS_PIN, SPI, 25000000)) {
        g_sdMounted = false;
        return false;
    }
    g_sdMounted = true;
    return true;
}

bool kitenSdMounted() { return g_sdMounted; }

std::vector<String> kitenSdListFiles(const String &dirPath, const String &exts)
{
    std::vector<String> result;
    if (!kitenSdBegin()) return result;

    
    std::vector<String> extList;
    int idx = 0;
    while (idx < (int)exts.length()) {
        int comma = exts.indexOf(',', idx);
        String e = (comma < 0) ? exts.substring(idx) : exts.substring(idx, comma);
        e.toLowerCase();
        if (e.length() > 0) extList.push_back(e);
        if (comma < 0) break;
        idx = comma + 1;
    }

    File root = SD.open(dirPath);
    if (!root) return result;
    File entry = root.openNextFile();
    while (entry) {
        if (!entry.isDirectory()) {
            String name = entry.name();
            String lower = name;
            lower.toLowerCase();
            bool match = extList.empty();
            for (const String &e : extList) {
                if (lower.endsWith(e)) { match = true; break; }
            }
            if (match) {
                String full = dirPath;
                if (!full.endsWith("/")) full += "/";
                full += name;
                result.push_back(full);
            }
        }
        entry = root.openNextFile();
    }
    root.close();
    return result;
}

String kitenSdReadFile(const String &path, size_t maxBytes)
{
    if (!kitenSdBegin()) return "";
    File f = SD.open(path, FILE_READ);
    if (!f) return "";
    String content;
    size_t sz = (size_t)f.size();
    content.reserve(sz > maxBytes ? maxBytes : sz);
    while (f.available() && content.length() < maxBytes) {
        
        char buf[256];
        int n = f.read((uint8_t *)buf, sizeof(buf));
        if (n <= 0) break;
        for (int i = 0; i < n && content.length() < maxBytes; i++) {
            content += buf[i];
        }
    }
    f.close();
    return content;
}

bool kitenSdWriteFile(const String &path, const String &content)
{
    if (!kitenSdBegin()) return false;
    
    
    if (SD.exists(path)) SD.remove(path);
    File f = SD.open(path, FILE_WRITE);
    if (!f) return false;
    size_t written = f.write((const uint8_t *)content.c_str(), content.length());
    f.close();
    return written == content.length();
}

bool kitenSdAppendFile(const String &path, const String &content)
{
    if (!kitenSdBegin()) return false;
    File f = SD.open(path, FILE_APPEND);
    if (!f) return false;
    size_t written = f.write((const uint8_t *)content.c_str(), content.length());
    f.close();
    return written == content.length();
}

bool kitenSdExists(const String &path)
{
    if (!kitenSdBegin()) return false;
    return SD.exists(path);
}

bool kitenSdRemove(const String &path)
{
    if (!kitenSdBegin()) return false;
    return SD.remove(path);
}
