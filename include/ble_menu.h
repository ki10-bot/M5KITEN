#pragma once
#include <Arduino.h>

void bleMenuEntry();

void bleScan();

void bleIbeacon(const char *DeviceName = "KITEN",
                const char *BEACON_UUID = "e4c159a0-8c82-11e6-bdf4-0800200c9a66",
                int ManufacturerId = 0x004C);

void bleSpamMenu();

void ninebotMenu();
