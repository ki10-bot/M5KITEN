

#include "ir_menu.h"
#include "config.h"
#include "settings.h"
#include "keyboard.h"
#include "ui.h"
#include "menu.h"
#include "sd_helper.h"
#include <Arduino.h>
#include <M5Unified.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <IRrecv.h>
#include <IRutils.h>
#include "WORLD_IR_CODES.h"

uint8_t kitenIrTxPin      = 44;
uint8_t kitenIrRxPin      = 45;
uint8_t kitenIrTxRepeats  = 1;

#define NA_REGION 1
#define EU_REGION 0

#define TVBG_NUM_ELEM(x) (sizeof(x) / sizeof(*(x)))

#define TVBG_DEBUG 0
#define TVBG_DEBUGP(x) if (TVBG_DEBUG == 1) { x; }

#define TVBG_NOPP __asm__ __volatile__("nop")
#define DELAY_CNT 25
#define MAX_WAIT_TIME 65535  

static uint8_t tvbg_bitsleft_r = 0;
static uint8_t tvbg_bits_r     = 0;
static uint8_t tvbg_code_ptr   = 0;
static volatile const IrCode *tvbg_powerCode = nullptr;

static uint8_t tvbg_read_bits(uint8_t count) {
    uint8_t tmp = 0;
    while (count--) {
        if (tvbg_bitsleft_r == 0) {
            tvbg_bits_r = tvbg_powerCode->codes[tvbg_code_ptr++];
            tvbg_bitsleft_r = 8;
        }
        tmp = (tmp << 1) | ((tvbg_bits_r >> --tvbg_bitsleft_r) & 1);
    }
    return tmp;
}

static void tvbg_delay_ten_us(uint16_t us) {
    uint8_t timer;
    while (us != 0) {
        for (timer = 0; timer <= DELAY_CNT; timer++) {
            TVBG_NOPP;
            TVBG_NOPP;
        }
        TVBG_NOPP;
        us--;
    }
}

void startTvBGone()
{
    
    uint8_t region = NA_REGION;
    std::vector<Option> regionOpts = {
        {"Region NA", [&]() { region = NA_REGION; }},
        {"Region EU", [&]() { region = EU_REGION; }},
        {"Back",      [](){}},
    };
    int rs = loopOptions(regionOpts, MENU_TYPE_REGULAR, "TV-B-Gone Region");
    if (rs == -1 || rs == (int)regionOpts.size() - 1) return;

    IRsend irsend(kitenIrTxPin);
    irsend.begin();
    pinMode(kitenIrTxPin, OUTPUT);
    digitalWrite(kitenIrTxPin, LOW);

    uint8_t num_codes = (region == NA_REGION) ? (uint8_t)TVBG_NUM_ELEM(NApowerCodes) : (uint8_t)TVBG_NUM_ELEM(EUpowerCodes);
    uint16_t rawData[300];

    drawMainBorder(true);
    M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
    M5.Display.setTextSize(FP);
    M5.Display.setTextDatum(top_left);
    M5.Display.drawString("TV-B-GONE", 4, 28);

    logClear();
    logPrint("Region: " + String(region == NA_REGION ? "NA" : "EU"));
    logPrint("Codes:  " + String(num_codes));
    logPrint("Sending...");
    logPrint("Sel: pause");
    logPrint("Back: stop");
    logRender();

    bool endingEarly = false;
    for (uint8_t i = 0; i < num_codes; i++) {
        tvbg_powerCode = (region == NA_REGION) ? NApowerCodes[i] : EUpowerCodes[i];

        const uint8_t freq          = tvbg_powerCode->timer_val;
        const uint8_t numpairs      = tvbg_powerCode->numpairs;
        const uint8_t bitcompress = tvbg_powerCode->bitcompression;

        tvbg_code_ptr = 0;
        for (uint8_t k = 0; k < numpairs; k++) {
            uint16_t ti = (tvbg_read_bits(bitcompress)) * 2;
            rawData[k * 2]       = tvbg_powerCode->times[ti] * 10;
            rawData[(k * 2) + 1] = tvbg_powerCode->times[ti + 1] * 10;
        }

        
        if (i % 5 == 0) {
            logClear();
            logPrint("Region: " + String(region == NA_REGION ? "NA" : "EU"));
            logPrint("Code " + String(i) + "/" + String(num_codes));
            logPrint("Sel: pause");
            logPrint("Back: stop");
            logRender();
        }

        
        for (uint8_t r = 0; r < kitenIrTxRepeats; r++) {
            irsend.sendRaw(rawData, numpairs * 2, freq);
        }
        tvbg_bitsleft_r = 0;

        
        tvbg_delay_ten_us(20500);

        M5.update();
        pollKeyboard();
        if (check(SelPress)) {
            waitAllKeysReleased();
            logClear();
            logPrint("** PAUSED **");
            logPrint("Sel: resume");
            logPrint("Back: stop");
            logRender();
            for (;;) {
                M5.update();
                pollKeyboard();
                if (check(SelPress)) { waitAllKeysReleased(); break; }
                if (check(EscPress)) { waitAllKeysReleased(); endingEarly = true; break; }
                delay(10);
            }
            if (endingEarly) break;
            logClear();
            logPrint("Resuming...");
            logRender();
        }
        if (check(EscPress)) {
            waitAllKeysReleased();
            endingEarly = true;
            break;
        }
    }

    digitalWrite(kitenIrTxPin, LOW);
    if (!endingEarly) {
        logClear();
        logPrint("All codes sent!");
        logRender();
        tvbg_delay_ten_us(MAX_WAIT_TIME);
        tvbg_delay_ten_us(MAX_WAIT_TIME);
    } else {
        displayInfo("TV-B-Gone stopped", true);
    }
}

static bool sendIrProtocol(const String &protocol, uint64_t address,
                           uint64_t command, uint16_t bits)
{
    IRsend irsend(kitenIrTxPin);
    irsend.begin();

    String p = protocol;
    p.toUpperCase();

    if (p == "NEC")           { irsend.sendNEC(command, bits); return true; }
    if (p == "SONY" || p == "SIRC")    { irsend.sendSony(command, bits); return true; }
    if (p == "RC5")           { irsend.sendRC5(command, bits); return true; }
    if (p == "RC6")           { irsend.sendRC6(command, bits); return true; }
    if (p == "SAMSUNG")       { irsend.sendSAMSUNG(command, bits); return true; }
    if (p == "LG")            { irsend.sendLG(command, bits); return true; }
    if (p == "SHARP")         { irsend.sendSharp(command, bits); return true; }
    if (p == "JVC")           { irsend.sendJVC(command, bits, 0); return true; }
    if (p == "PANASONIC")     { irsend.sendPanasonic(command, command); return true; }
    if (p == "WHYNTER")       { irsend.sendWhynter(command, command); return true; }
    if (p == "COOLIX")        { irsend.sendCOOLIX(command, bits, 0); return true; }
    return false;
}

static void sendIrFile(const String &path)
{
    String content = kitenSdReadFile(path, 30000);
    if (content.length() == 0) {
        displayError("Failed to read\n" + path, true);
        return;
    }

    String protocol;
    String addressStr;
    String commandStr;
    String rawStr;
    uint16_t frequency = 38000;
    uint16_t bits = 32;

    
    int idx = 0;
    while (idx < (int)content.length()) {
        int nl = content.indexOf('\n', idx);
        String line = (nl < 0) ? content.substring(idx) : content.substring(idx, nl);
        line.trim();
        int colon = line.indexOf(':');
        if (colon > 0) {
            String key = line.substring(0, colon);
            String val = line.substring(colon + 1);
            key.trim();
            val.trim();
            key.toLowerCase();
            if (key == "protocol")      protocol = val;
            else if (key == "address")  addressStr = val;
            else if (key == "command")  commandStr = val;
            else if (key == "frequency") frequency = (uint16_t)val.toInt();
            else if (key == "bits")     bits = (uint16_t)val.toInt();
            else if (key == "raw")      rawStr = val;
        }
        if (nl < 0) break;
        idx = nl + 1;
    }

    if (protocol.length() > 0) {
        
        uint64_t address = 0;
        uint64_t command = 0;
        
        if (addressStr.length() > 0) {
            addressStr.replace(" ", "");
            address = strtoull(addressStr.c_str(), nullptr, 16);
        }
        if (commandStr.length() > 0) {
            commandStr.replace(" ", "");
            command = strtoull(commandStr.c_str(), nullptr, 16);
        }
        if (sendIrProtocol(protocol, address, command, bits)) {
            displayInfo("Sent " + protocol + "\nA:" + String(address, HEX) + " C:" + String(command, HEX), true);
        } else {
            displayError("Unknown protocol:\n" + protocol, true);
        }
    } else if (rawStr.length() > 0) {
        
        std::vector<uint16_t> rawTimings;
        int ri = 0;
        while (ri < (int)rawStr.length()) {
            int comma = rawStr.indexOf(',', ri);
            String numStr = (comma < 0) ? rawStr.substring(ri) : rawStr.substring(ri, comma);
            numStr.trim();
            if (numStr.length() > 0) rawTimings.push_back((uint16_t)numStr.toInt());
            if (comma < 0) break;
            ri = comma + 1;
        }
        if (rawTimings.size() > 0) {
            IRsend irsend(kitenIrTxPin);
            irsend.begin();
            irsend.sendRaw(rawTimings.data(), rawTimings.size(), frequency);
            displayInfo("Sent raw " + String(rawTimings.size()) + " timings", true);
        } else {
            displayError("Empty raw data", true);
        }
    } else {
        displayError("No protocol or raw\nin .ir file", true);
    }
}

void otherIRcodes()
{
    std::vector<Option> opts = {
        {"Type Protocol", []() {
            
            std::vector<String> protos = {
                "NEC", "SONY", "RC5", "RC6", "SAMSUNG", "LG",
                "SHARP", "JVC", "PANASONIC", "WHYNTER", "COOLIX"
            };
            std::vector<Option> popts;
            String chosenProto;
            for (const String &p : protos) {
                popts.push_back({p, [&chosenProto, p]() { chosenProto = p; }});
            }
            popts.push_back({"Back", [](){}});
            int ps = loopOptions(popts, MENU_TYPE_REGULAR, "Pick Protocol");
            if (ps == -1 || ps == (int)popts.size() - 1) return;

            
            String addrStr = keyboard("", 8, "Address (HEX)");
            if (addrStr == "\x1B" || addrStr.length() == 0) return;
            uint64_t address = strtoull(addrStr.c_str(), nullptr, 16);

            
            String cmdStr = keyboard("", 8, "Command (HEX)");
            if (cmdStr == "\x1B" || cmdStr.length() == 0) return;
            uint64_t command = strtoull(cmdStr.c_str(), nullptr, 16);

            
            String bitsStr = keyboard("32", 3, "Bits");
            if (bitsStr == "\x1B" || bitsStr.length() == 0) return;
            uint16_t bits = (uint16_t)bitsStr.toInt();
            if (bits == 0) bits = 32;

            if (sendIrProtocol(chosenProto, address, command, bits)) {
                displayInfo("Sent " + chosenProto + "\nA:" + String((uint32_t)address, HEX) + " C:" + String((uint32_t)command, HEX), true);
            } else {
                displayError("Unknown protocol", true);
            }
        }},
        {"Load from SD", []() {
            if (!kitenSdBegin()) {
                displayError("SD card not found", true);
                return;
            }
            auto files = kitenSdListFiles("/", ".ir");
            if (files.empty()) {
                displayError("No .ir files\nfound on SD", true);
                return;
            }
            std::vector<Option> fopts;
            for (const String &f : files) {
                fopts.push_back({f, [f]() { sendIrFile(f); }});
            }
            fopts.push_back({"Back", [](){}});
            loopOptions(fopts, MENU_TYPE_REGULAR, "Pick .ir file");
        }},
        {"Back", [](){}},
    };
    loopOptions(opts, MENU_TYPE_REGULAR, "Custom IR");
}

void irRead()
{
    IRrecv irrecv(kitenIrRxPin);
    decode_results results;

    drawMainBorder(true);
    M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
    M5.Display.setTextSize(FP);
    M5.Display.setTextDatum(top_left);
    M5.Display.drawString("IR READ", 4, 28);

    logClear();
    logPrint("Listening on GPIO " + String(kitenIrRxPin));
    logPrint("Point a remote at");
    logPrint("the IR receiver.");
    logPrint("");
    logPrint("Back: exit");
    logRender();

    irrecv.enableIRIn();

    int codeCount = 0;
    for (;;) {
        M5.update();
        pollKeyboard();
        if (check(EscPress)) {
            waitAllKeysReleased();
            break;
        }
        if (irrecv.decode(&results)) {
            codeCount++;
            String proto = typeToString(results.decode_type, results.repeat);
            String addrStr = uint64ToString(results.address, HEX);
            String cmdStr  = uint64ToString(results.value, HEX);

            logClear();
            logPrint("Code #" + String(codeCount));
            logPrint("Proto: " + proto);
            logPrint("Addr: 0x" + addrStr);
            logPrint("Val:  0x" + cmdStr);
            logPrint("Bits: " + String(results.bits));
            if (results.overflow) logPrint("** overflow **");
            logPrint("");
            logPrint("Back: exit");
            logRender();
            irrecv.resume();
        }
        delay(10);
    }
    displayInfo("IR Read stopped\nCaptured: " + String(codeCount) + " codes", true);
}

void startIrJammer()
{
    IRsend irsend(kitenIrTxPin);
    irsend.begin();

    drawMainBorder(true);
    M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
    M5.Display.setTextSize(FP);
    M5.Display.setTextDatum(top_left);
    M5.Display.drawString("IR JAMMER", 4, 28);

    logClear();
    logPrint("Jamming on GPIO " + String(kitenIrTxPin));
    logPrint("38 kHz modulation");
    logPrint("Range: ~2-3 meters");
    logPrint("");
    logPrint("Back: stop");
    logRender();

    uint16_t rawData[200];
    unsigned long lastDraw = millis();
    unsigned long count = 0;
    for (;;) {
        M5.update();
        pollKeyboard();
        if (check(EscPress)) {
            waitAllKeysReleased();
            break;
        }
        
        int len = 20 + (rand() % 60);  
        for (int i = 0; i < len && i < 200; i++) {
            
            rawData[i] = 200 + (rand() % 1800);
        }
        irsend.sendRaw(rawData, len, 38);
        count++;
        if (millis() - lastDraw > 500) {
            logClear();
            logPrint("Jamming on GPIO " + String(kitenIrTxPin));
            logPrint("38 kHz modulation");
            logPrint("Range: ~2-3 meters");
            logPrint("Bursts: " + String(count));
            logPrint("");
            logPrint("Back: stop");
            logRender();
            lastDraw = millis();
        }
        delay(5 + (rand() % 20));  
    }
    displayInfo("IR Jammer stopped", true);
}

void irConfigMenu()
{
    std::vector<Option> opts = {
        {"IR TX Pin (GPIO " + String(kitenIrTxPin) + ")", []() {
            String s = keyboard(String(kitenIrTxPin), 3, "IR TX GPIO");
            if (s == "\x1B" || s.length() == 0) return;
            int pin = s.toInt();
            if (pin >= 0 && pin <= 48) kitenIrTxPin = (uint8_t)pin;
        }},
        {"IR RX Pin (GPIO " + String(kitenIrRxPin) + ")", []() {
            String s = keyboard(String(kitenIrRxPin), 3, "IR RX GPIO");
            if (s == "\x1B" || s.length() == 0) return;
            int pin = s.toInt();
            if (pin >= 0 && pin <= 48) kitenIrRxPin = (uint8_t)pin;
        }},
        {"IR TX Repeats (" + String(kitenIrTxRepeats) + ")", []() {
            String s = keyboard(String(kitenIrTxRepeats), 2, "Repeats 1..10");
            if (s == "\x1B" || s.length() == 0) return;
            int r = s.toInt();
            if (r >= 1 && r <= 10) kitenIrTxRepeats = (uint8_t)r;
        }},
        {"Back", [](){}},
    };
    loopOptions(opts, MENU_TYPE_REGULAR, "IR Config");
}

void irMenuEntry()
{
    std::vector<Option> opts = {
        {"TV-B-Gone",  []() { startTvBGone();   }},
        {"Custom IR",  []() { otherIRcodes();   }},
        {"IR Read",    []() { irRead();         }},
        {"IR Jammer",  []() { startIrJammer();  }},
        {"Config",     []() { irConfigMenu();   }},
    };
    String txt = "Infrared";
    txt += " Tx:" + String(kitenIrTxPin) + " Rx:" + String(kitenIrRxPin) +
           " Rpts:" + String(kitenIrTxRepeats);
    loopOptions(opts, MENU_TYPE_SUBMENU, txt.c_str());
}
