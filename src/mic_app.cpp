

#include "others_menu.h"
#include "config.h"
#include "settings.h"
#include "keyboard.h"
#include "ui.h"
#include "menu.h"
#include "sd_helper.h"
#include <Arduino.h>
#include <M5Unified.h>
#include "driver/i2s.h"
#include <math.h>
#include <SD.h>

#define KITEN_MIC_I2S_PORT      (I2S_NUM_0)
#define KITEN_MIC_DATA_PIN      (41)     
#define KITEN_MIC_CLK_PIN       (42)     
#define KITEN_MIC_SAMPLE_RATE   (16000)
#define KITEN_MIC_DOWNSHIFT     (4)      
#define KITEN_MIC_GAIN          (3.0f)

static bool kitenMicInit()
{
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM),
        .sample_rate = KITEN_MIC_SAMPLE_RATE * KITEN_MIC_DOWNSHIFT,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,
        .communication_format = I2S_COMM_FORMAT_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 256,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0,
    };
    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_PIN_NO_CHANGE,    
        .ws_io_num = KITEN_MIC_CLK_PIN,     
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = KITEN_MIC_DATA_PIN,  
    };
    esp_err_t err = i2s_driver_install(KITEN_MIC_I2S_PORT, &i2s_config, 0, NULL);
    if (err != ESP_OK) {
        Serial.printf("[MIC] i2s_driver_install failed: %s\n", esp_err_to_name(err));
        return false;
    }
    err = i2s_set_pin(KITEN_MIC_I2S_PORT, &pin_config);
    if (err != ESP_OK) {
        Serial.printf("[MIC] i2s_set_pin failed: %s\n", esp_err_to_name(err));
        i2s_driver_uninstall(KITEN_MIC_I2S_PORT);
        return false;
    }
    return true;
}

static void kitenMicDeinit()
{
    i2s_driver_uninstall(KITEN_MIC_I2S_PORT);
}

static bool kitenMicRead(int16_t *outSamples, size_t n)
{
    
    
    size_t rawN = n * KITEN_MIC_DOWNSHIFT;
    int16_t *raw = (int16_t *)malloc(rawN * sizeof(int16_t));
    if (!raw) return false;

    size_t bytesRead = 0;
    esp_err_t err = i2s_read(KITEN_MIC_I2S_PORT, raw, rawN * sizeof(int16_t),
                             &bytesRead, portMAX_DELAY);
    if (err != ESP_OK) {
        free(raw);
        return false;
    }
    size_t samplesRead = bytesRead / sizeof(int16_t);

    
    for (size_t i = 0; i < n; i++) {
        int32_t sum = 0;
        for (int j = 0; j < KITEN_MIC_DOWNSHIFT; j++) {
            size_t idx = i * KITEN_MIC_DOWNSHIFT + j;
            if (idx < samplesRead) sum += raw[idx];
        }
        int32_t avg = sum / KITEN_MIC_DOWNSHIFT;
        
        avg = (int32_t)(avg * KITEN_MIC_GAIN);
        if (avg > 32767)  avg = 32767;
        if (avg < -32768) avg = -32768;
        outSamples[i] = (int16_t)avg;
    }
    free(raw);
    return true;
}

#define SPECTRUM_N       (128)        
#define SPECTRUM_BINS    (32)         

void mic_test_impl()
{
    if (!kitenMicInit()) {
        displayError("Mic init failed", true);
        return;
    }

    M5.Display.fillScreen(kitenConfig.bgColor);
    drawMainBorder(false);

    
    M5.Display.setTextSize(FP);
    M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
    M5.Display.setCursor(4, 30);
    M5.Display.print("Mic Spectrum - ESC to exit");

    int16_t *samples = (int16_t *)malloc(SPECTRUM_N * sizeof(int16_t));
    if (!samples) {
        kitenMicDeinit();
        displayError("malloc failed", true);
        return;
    }

    
    int binIdx[SPECTRUM_BINS];
    for (int i = 0; i < SPECTRUM_BINS; i++) {
        
        binIdx[i] = i + 1;
    }

    waitAllKeysReleased();
    while (!check(EscPress)) {
        if (!kitenMicRead(samples, SPECTRUM_N)) continue;

        
        float mag[SPECTRUM_BINS];
        for (int b = 0; b < SPECTRUM_BINS; b++) {
            int k = binIdx[b];
            float re = 0, im = 0;
            for (int n = 0; n < SPECTRUM_N; n++) {
                float angle = 2.0f * M_PI * k * n / SPECTRUM_N;
                re += samples[n] * cosf(angle);
                im -= samples[n] * sinf(angle);
            }
            mag[b] = sqrtf(re * re + im * im) / SPECTRUM_N * 2.0f;
            
            mag[b] = log10f(mag[b] + 1) * 30.0f;
            if (mag[b] > 100.0f) mag[b] = 100.0f;
        }

        
        int barAreaY = 30 + LH + 4;       
        int barAreaH = SCREEN_HEIGHT - barAreaY - 4;
        int barW = (SCREEN_WIDTH - 4) / SPECTRUM_BINS;
        for (int b = 0; b < SPECTRUM_BINS; b++) {
            int barH = (int)(mag[b] * barAreaH / 100);
            if (barH < 1) barH = 1;
            int x = 2 + b * barW;
            int y = barAreaY + barAreaH - barH;
            
            M5.Display.fillRect(x, barAreaY, barW - 1, barAreaH - barH, kitenConfig.bgColor);
            
            uint16_t c = (mag[b] > 60) ? kitenConfig.secColor : kitenConfig.priColor;
            M5.Display.fillRect(x, y, barW - 1, barH, c);
        }

        delay(20);
    }

    free(samples);
    kitenMicDeinit();
    waitAllKeysReleased();
}

static void writeWavHeader(File &f, uint32_t sampleRate, uint16_t bitsPerSample,
                            uint16_t numChannels, uint32_t numSamples)
{
    uint32_t dataSize = numSamples * numChannels * (bitsPerSample / 8);
    uint32_t chunkSize = 36 + dataSize;
    uint16_t blockAlign = numChannels * (bitsPerSample / 8);
    uint32_t byteRate = sampleRate * blockAlign;

    uint8_t hdr[44] = {0};
    memcpy(hdr +  0, "RIFF", 4);
    memcpy(hdr +  4, &chunkSize, 4);
    memcpy(hdr +  8, "WAVE", 4);
    memcpy(hdr + 12, "fmt ", 4);
    uint32_t fmtSize = 16;
    memcpy(hdr + 16, &fmtSize, 4);
    uint16_t audioFmt = 1;
    memcpy(hdr + 20, &audioFmt, 2);
    memcpy(hdr + 22, &numChannels, 2);
    memcpy(hdr + 24, &sampleRate, 4);
    memcpy(hdr + 28, &byteRate, 4);
    memcpy(hdr + 32, &blockAlign, 2);
    memcpy(hdr + 34, &bitsPerSample, 2);
    memcpy(hdr + 36, "data", 4);
    memcpy(hdr + 40, &dataSize, 4);
    f.write(hdr, 44);
}

void mic_record_app_impl()
{
    
    String durStr = keyboard("5", 4, "Duration (sec, 1-30):");
    if (durStr == "\x1B" || durStr.length() == 0) return;
    int duration = durStr.toInt();
    if (duration < 1) duration = 1;
    if (duration > 30) duration = 30;

    if (!kitenSdBegin()) {
        displayError("SD card required", true);
        return;
    }

    if (!kitenMicInit()) {
        displayError("Mic init failed", true);
        return;
    }

    
    char filename[32];
    int idx = 0;
    do {
        snprintf(filename, sizeof(filename), "/rec_%03d.wav", idx);
        idx++;
    } while (SD.exists(filename));

    File f = SD.open(filename, FILE_WRITE);
    if (!f) {
        kitenMicDeinit();
        displayError("Cannot create file", true);
        return;
    }

    
    writeWavHeader(f, KITEN_MIC_SAMPLE_RATE, 16, 1, 0);

    
    M5.Display.fillScreen(kitenConfig.bgColor);
    drawMainBorder(false);
    M5.Display.setTextSize(FM);
    M5.Display.setTextColor(TFT_RED, kitenConfig.bgColor);
    M5.Display.setCursor(4, 30);
    M5.Display.print("REC");
    M5.Display.setTextSize(FP);
    M5.Display.setTextColor(TFT_WHITE, kitenConfig.bgColor);
    M5.Display.setCursor(40, 33);
    M5.Display.print(filename);
    M5.Display.setCursor(4, 50);
    M5.Display.print("0 / ");
    M5.Display.print(duration);
    M5.Display.print(" s");

    const int CHUNK = 256;
    int16_t *samples = (int16_t *)malloc(CHUNK * sizeof(int16_t));
    uint32_t totalSamples = 0;
    uint32_t targetSamples = (uint32_t)duration * KITEN_MIC_SAMPLE_RATE;
    unsigned long startT = millis();
    unsigned long lastUi = 0;

    while (totalSamples < targetSamples && !check(EscPress)) {
        if (!kitenMicRead(samples, CHUNK)) continue;
        
        uint8_t buf[CHUNK * 2];
        for (int i = 0; i < CHUNK; i++) {
            buf[i * 2]     = samples[i] & 0xFF;
            buf[i * 2 + 1] = (samples[i] >> 8) & 0xFF;
        }
        f.write(buf, CHUNK * 2);
        totalSamples += CHUNK;

        if (millis() - lastUi > 250) {
            int sec = (int)(totalSamples / KITEN_MIC_SAMPLE_RATE);
            M5.Display.fillRect(4, 50, 100, LH, kitenConfig.bgColor);
            M5.Display.setCursor(4, 50);
            M5.Display.printf("%d / %d s", sec, duration);
            
            int barW = (int)((float)totalSamples / targetSamples * (SCREEN_WIDTH - 20));
            M5.Display.fillRect(10, 70, SCREEN_WIDTH - 20, 8, kitenConfig.bgColor);
            M5.Display.fillRect(10, 70, barW, 8, kitenConfig.priColor);
            lastUi = millis();
        }
    }
    free(samples);
    f.close();
    kitenMicDeinit();

    
    f = SD.open(filename, FILE_WRITE);
    if (f) {
        f.seek(0);
        writeWavHeader(f, KITEN_MIC_SAMPLE_RATE, 16, 1, totalSamples);
        f.close();
    }

    unsigned long elapsed = (millis() - startT) / 1000;
    char msg[64];
    snprintf(msg, sizeof(msg), "Saved %s\n%d s, %lu samples", filename, (int)elapsed, (unsigned long)totalSamples);
    displaySuccess(msg, true);
    waitAllKeysReleased();
}
