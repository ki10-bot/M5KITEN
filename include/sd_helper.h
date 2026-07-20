

#ifndef __KITEN_SD_HELPER_H__
#define __KITEN_SD_HELPER_H__

#include <Arduino.h>
#include <vector>
#include <FS.h>

#define KITEN_SD_SPI_SCK_PIN  (40)
#define KITEN_SD_SPI_MISO_PIN (39)
#define KITEN_SD_SPI_MOSI_PIN (14)
#define KITEN_SD_SPI_CS_PIN   (12)

bool kitenSdBegin();

bool kitenSdMounted();

std::vector<String> kitenSdListFiles(const String &dirPath = "/",
                                     const String &exts = "");

String kitenSdReadFile(const String &path, size_t maxBytes = 50000);

bool kitenSdWriteFile(const String &path, const String &content);

bool kitenSdAppendFile(const String &path, const String &content);

bool kitenSdExists(const String &path);

bool kitenSdRemove(const String &path);

#endif
