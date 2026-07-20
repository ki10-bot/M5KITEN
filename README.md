# M5KITEN

M5KITEN is a multi-tool security firmware for the M5Stack Cardputer (ESP32-S3 based development board with integrated keyboard and display). It provides a comprehensive suite of penetration testing and network analysis tools in a portable, pocket-sized form factor.

---

## Table of Contents

- [Features](#features)
- [Hardware Requirements](#hardware-requirements)
- [Installation](#installation)
- [Usage](#usage)
- [Project Structure](#project-structure)
- [Modifying the Firmware](#modifying-the-firmware)
- [Creating New Modules](#creating-new-modules)
- [Troubleshooting](#troubleshooting)
- [License](#license)

---

## Features

### WiFi Tools
- **WiFi Scanner**: Scan for nearby access points, display SSID, signal strength (RSSI), encryption type, and channel information
- **Deauth Attack**: Send deauthentication frames to disconnect clients from target networks (for authorized testing only)
- **Evil Portal**: Create captive portal pages for phishing simulations and security awareness training
- **DDoS Menu**: Execute various denial-of-service attack patterns against target systems (for authorized testing)
- **Connect Manager**: Manage saved WiFi profiles and connection settings

### Bluetooth Low Energy (BLE)
- **BLE Scanner**: Discover nearby BLE devices, display names, addresses, and RSSI values
- **BLE Jammer**: Transmit advertising packets to interfere with BLE scanning operations

### Radio Frequency (RF) & NFC
- **RFID Cloner**: Read and clone 13.56 MHz RFID tags using PN532 module
- **iButton Reader**: Read Dallas iButton keys (DS1990A format) via OneWire protocol
- **NRF24L01 Tools**: Interface with 2.4 GHz NRF24 radio modules for wireless communication testing
- **IR Remote**: Send and receive infrared codes using the built-in IR transmitter/receiver, includes extensive database of world IR codes

### USB Tools (BadUSB/HID)
- **BadUSB**: Execute Ducky Script payloads for automated keyboard input simulation
- **USB Keyboard**: Interactive keyboard passthrough mode for manual HID input
- **USB Clicker**: Auto-clicker tool for automated mouse click simulation
- **U2F Security Key**: Universal 2nd Factor authentication token implementation (stub)

### Utilities
- **QR Code Generator**: Generate QR codes including Brazilian PIX payment codes, custom text QR codes, save and manage multiple codes
- **IP Address Generator**: Generate random or pattern-based IP addresses for network scanning preparation
- **Email OSINT**: Open-source intelligence gathering via email address lookup services
- **Python Interpreter**: Execute MicroPython scripts directly on the device
- **Scripts Manager**: Load and execute custom scripts from SD card storage
- **File Editor**: View and edit text files on the SD card
- **Clock**: Real-time clock display with NTP synchronization capability
- **Microphone**: Audio spectrum analyzer and WAV recorder using the SPM1423 PDM microphone
- **FM Radio**: Receive FM broadcast signals using RDA5807M module (requires hardware add-on)

### System
- **Settings Menu**: Configure brightness, colors, sound, rotation, startup behavior
- **Config Menu**: Advanced configuration options for power management and feature flags

---

## Hardware Requirements

### Required Hardware
| Component | Specification |
|-----------|---------------|
| Board | M5Stack Cardputer (StampS3 variant) |
| MCU | ESP32-S3 (Xtensa LX7 dual-core) |
| Display | ST7789 TFT, 240 x 135 pixels, SPI interface |
| Keyboard | Integrated QWERTY keyboard |
| Storage | MicroSD card slot (recommended 8GB+) |

### Optional Hardware Add-ons
| Module | Purpose |
|--------|---------|
| PN532 RFID/NFC Module | RFID cloning features |
| RDA5807M FM Module | FM radio reception |
| NRF24L01+ Module | 2.4 GHz RF communication |
| IR Transmitter | Extended infrared range |

---

## Installation

### Prerequisites

1. **PlatformIO CLI** (version 6.x or later)
   ```bash
   pip install platformio
   ```

2. **VS Code** with PlatformIO extension (recommended) or any text editor

3. **USB-C cable** for flashing and serial communication

### Build Steps

1. Clone or download this repository:
   ```bash
   git clone https://github.com/yourusername/M5KITEN.git
   cd M5KITEN
   ```

2. Install dependencies automatically via PlatformIO:
   ```bash
   pio run
   ```

3. Connect the M5Stack Cardputer via USB-C

4. Flash the firmware:
   ```bash
   pio run -t upload
   ```

5. Monitor serial output (optional, for debugging):
   ```bash
   pio device monitor -b 115200
   ```

### PlatformIO Configuration

The `platformio.ini` file is pre-configured for the M5Stack StampS3 board:

```ini
[env:m5stack-stamps3]
platform = espressif32
board = m5stack-stamps3
framework = arduino
monitor_speed = 115200
```

---

## Usage

### Navigation Controls

The Cardputer keyboard serves as the primary input method:

| Key | Function |
|-----|----------|
| Up Arrow / `,` (comma) | Navigate up in menus |
| Down Arrow / `.` (period) | Navigate down in menus |
| Left Arrow / `;` (semicolon) | Previous item / Page up |
| Right Arrow / `'` (apostrophe) | Next item / Page down |
| Enter / Select | Select menu option |
| ESC / Back | Go back one level |
| Tab | Special functions (context-dependent) |
| Space | Confirm actions |
| Backspace | Delete characters |

### Main Menu Interface

Upon booting, the firmware displays an icon carousel showing all available modules:

1. Use arrow keys to navigate between icons
2. Press Enter to select a module
3. Each module opens its own submenu with specific options
4. Press ESC at any time to return to the previous level

### Common Operations

#### Text Input
When a function requires text input (such as entering an SSID or filename), a keyboard input dialog appears. Type your input using the integrated keyboard, then press Enter to confirm or ESC to cancel.

#### Status Bar
The top of the screen displays:
- Current time (if NTP synchronized)
- Battery status indicator
- WiFi connectivity status (when connected)

#### Modal Messages
Information, warnings, errors, and success messages appear as colored banners:
- Red: Error messages
- Yellow/Orange: Warning messages
- Blue: Informational messages
- Green: Success confirmations

Press any key to dismiss modal messages when they are blocking.

---

## Project Structure

```
M5KITEN/
|-- include/                  # Header files (.h)
|   |-- app.h                 # Application entry points: initHardware(), appLoop(), runMainMenu()
|   |-- config.h              # Constants: SCREEN_WIDTH, SCREEN_HEIGHT, colors, font sizes
|   |-- settings.h            # Runtime configuration struct (kitenConfig), NVS persistence
|   |-- ui.h                  # Drawing functions: borders, messages, icons, animations
|   |-- keyboard.h            # keyboard() function for text input, key state checking
|   |-- menu.h                # Option struct, loopOptions(), navigation helpers
|   |-- wifi_menu.h           # WiFi module declarations
|   |-- ble_menu.h            # BLE module declarations
|   |-- rfid_menu.h           # RFID module declarations
|   |-- badusb_menu.h         # BadUSB module declarations
|   |-- others_menu.h         # Others category (QR, Mic, iButton, etc.)
|   |-- python_menu.h         # Python interpreter declarations
|   |-- scripts_menu.h        # Scripts manager declarations
|   |-- config_menu.h         # Settings/config declarations
|   `-- [other module headers]
|
|-- src/                      # Implementation files (.cpp)
|   |-- main.cpp              # Arduino setup()/loop() entry point
|   |-- app.cpp               # Main menu logic, module dispatch (**KEY FILE**)
|   |-- ui.cpp                # All drawing code, icon rendering (**KEY FILE**)
|   |-- menu.cpp              # loopOptions() implementation, input handling
|   |-- keyboard.cpp          # Keyboard polling, text input buffer
|   |-- settings.cpp          # Configuration load/save to NVS
|   |-- wifi_menu.cpp         # WiFi scanner, deauth, evil portal
|   |-- wifi_apps.cpp         # WiFi attack implementations
|   |-- wifi_deauth.cpp       # Deauth attack logic
|   |-- ble_menu.cpp          # BLE scanner interface
|   |-- ble_jammer.cpp        # BLE jammer implementation
|   |-- rfid_menu.cpp         # RFID menu navigation
|   |-- rfid_cloner.cpp       # RFID read/write logic
|   |-- badusb_menu.cpp       # BadUSB script selection
|   |-- badusb.cpp            # Ducky script execution engine
|   |-- others_menu.cpp       # QR codes, microphone, iButton, U2F
|   |-- ipgen_menu.cpp        # IP address generator
|   |-- email_osint.cpp       # Email OSINT lookups
|   |-- python_menu.cpp       # Python interpreter UI
|   |-- py_interpreter.cpp    # MicroPython execution
|   |-- scripts_menu.cpp      # SD card script manager
|   |-- file_editor.cpp       # Text file viewer/editor
|   |-- clock_app.cpp         # Clock display
|   |-- mic_app.cpp           # Microphone spectrum/recorder
|   |-- ir_menu.cpp           # IR remote control
|   |-- nrf24_menu.cpp        # NRF24 radio tools
|   |-- fm_menu.cpp           # FM radio receiver
|   |-- connect_menu.cpp      # Connection manager
|   |-- config_menu.cpp       # Settings configuration UI
|   |-- sd_helper.cpp         # SD card utility functions
|   `-- [other module sources]
|
|-- lib/                      # External libraries (empty by default)
|-- platformio.ini            # Build configuration
`-- README.md                 # This file
```

---

## Modifying the Firmware

This section explains how to customize and extend the firmware. All modifications follow established patterns used throughout the codebase.

### Understanding the Architecture

The firmware uses a cooperative multitasking model on the ESP32-S3. The main loop continuously polls for keyboard input and updates the display. Critical constraint: never block execution for more than 50 milliseconds without calling `pollKeyboard()` and `M5.update()`, otherwise the keyboard becomes unresponsive.

Execution flow:
```
Arduino setup()
  -> initHardware()           // Initialize display, keyboard, config
  -> kitenConfig.begin()      // Load settings from NVS
  -> initUI()                // Apply brightness, rotation settings

Arduino loop()
  -> appLoop()               // Main application loop
    -> pollKeyboard()        // Check key states
    -> runMainMenu()         // Show icon carousel
      -> loopOptions()       // Render menu, handle selection
        -> User selects item -> Call module's *MenuEntry() function
```

### Adding a New Top-Level Module

To add a new module to the main menu, you must modify four specific locations:

#### Step 1: Create Header File

Create `include/your_module_menu.h`:

```cpp
#pragma once
#include <Arduino.h>

void yourModuleMenuEntry();
```

#### Step 2: Create Implementation File

Create `src/your_module_menu.cpp`. This file must include the framework headers in the correct order:

```cpp
#include "your_module_menu.h"   // Your header first
#include "config.h"              // Screen dimensions, colors
#include "settings.h"            // kitenConfig global instance
#include "keyboard.h"            // Text input function
#include "ui.h"                  // Drawing functions
#include "menu.h"                // loopOptions(), Option struct
#include <Arduino.h>
#include <M5Unified.h>           // M5.Display, M5.update()
#include <vector>

void yourModuleMenuEntry()
{
    std::vector<Option> opts;
    for (;;) {
        opts.clear();  // Clear vector each iteration
        
        opts.push_back({"Your Action", [](){
            displayInfo("Action executed!", true);
        }});
        
        opts.push_back({"Back", [](){}});  // Always last
        
        int sel = loopOptions(opts, MENU_TYPE_REGULAR, "YourModule");
        if (sel == -1 || sel == (int)opts.size() - 1) return;
    }
}
```

#### Step 3: Register in App

Edit `src/app.cpp`:

1. Add include near the top (after existing includes):
```cpp
#include "your_module_menu.h"
```

2. Add menu entry inside `runMainMenu()` function's options vector:
```cpp
std::vector<Option> opts = {
    {"Clock",   [](){ clockMenuEntry();    }},
    {"WiFi",    [](){ wifiMenuEntry();     }},
    // ... existing entries ...
    {"YourModule", [](){ yourModuleMenuEntry(); }},  // ADD THIS
    {"Config",  [](){ configMenuEntry();   }},
};
```

#### Step 4: Add Icon

Edit `src/ui.cpp`:

1. Define icon drawing function before `drawOthersIcon()` (around line 1097):
```cpp
static void drawYourModuleIcon(float scale, uint16_t color, uint16_t bg)
{
    int cx = g_iconCenterX;  // Screen center X (120)
    int cy = g_iconCenterY;  // Screen center Y (~75)
    
    // Draw your icon using LovyanGFX primitives:
    M5.Display.fillCircle(cx, cy, (int)(scale * 10), color);
    M5.Display.fillCircle(cx, cy + 2, (int)(scale * 6), bg);
    M5.Display.setTextColor(color, bg);
    M5.Display.setTextSize(2);  // Medium font
    M5.Display.setTextDatum(top_center);
    M5.Display.drawString("Y", cx, cy);
    M5.Display.setTextDatum(top_left);
}
```

2. Add dispatch case inside `drawMainMenu()` function's if-else chain (before the final else):
```cpp
} else if (lbl.equalsIgnoreCase("YourModule")) {
    drawYourModuleIcon(1.0f, iconColor, kitenConfig.bgColor);
} else {
    // Default fallback icon
}
```

**Important:** The string in `lbl.equalsIgnoreCase()` must match the string in `app.cpp`'s menu entry exactly (case-insensitive).

---

## Creating New Modules

### Complete Module Template

Below is a production-ready template for new modules. Copy and modify as needed.

#### Header (`include/new_module_menu.h`)
```cpp
#pragma once
#include <Arduino.h>

void newModuleMenuEntry();
```

#### Implementation (`src/new_module_menu.cpp`)

```cpp
#include "new_module_menu.h"
#include "config.h"
#include "settings.h"
#include "keyboard.h"
#include "ui.h"
#include "menu.h"
#include <Arduino.h>
#include <M5Unified.h>
#include <vector>

// Module-specific constants
#define NEW_MODULE_MAX_VALUE  999
#define NEW_MODULE_DEFAULT    10

// Private module state (static = local to this file)
static int s_setting = NEW_MODULE_DEFAULT;

// Internal helper: Format result string
static String formatResult(int value)
{
    return "Result: " + String(value);
}

// Internal helper: Show information display
static void showInfoDisplay(const String &title, const String &body)
{
    M5.Display.fillScreen(kitenConfig.bgColor);
    drawMainBorder(false);
    
    M5.Display.setTextSize(FM);  // Medium font
    M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
    M5.Display.setCursor(BORDER_PAD_X, BORDER_PAD_Y);
    M5.Display.print(title);
    
    // Separator line
    M5.Display.drawFastHLine(BORDER_PAD_X, BORDER_PAD_Y + 18,
                             SCREEN_WIDTH - 2 * BORDER_PAD_X, kitenConfig.secColor);
    
    M5.Display.setTextSize(FP);  // Small font
    M5.Display.setTextColor(TFT_WHITE, kitenConfig.bgColor);
    M5.Display.setCursor(BORDER_PAD_X, BORDER_PAD_Y + 22);
    M5.Display.print(body);
}

// Action handler: Main operation
static void doMainAction()
{
    showInfoDisplay("Processing", "Working...");
    
    for (int i = 0; i < s_setting; i++) {
        // Poll every 20 iterations to stay responsive
        if (i % 20 == 0) {
            markActivity();      // Prevent screensaver activation
            M5.update();
            pollKeyboard();
            
            // Allow user to abort
            if (check(EscPress)) {
                displayWarning("Cancelled");
                return;
            }
        }
        delay(10);  // Keep delays short
    }
    
    displaySuccess("Completed " + String(s_setting) + " iterations");
}

// Action handler: Change setting
static void changeSetting()
{
    String input = keyboard(
        String(s_setting),
        4,
        "Value (1-" + String(NEW_MODULE_MAX_VALUE) + "):"
    );
    
    if (input == "\x1B" || input.length() == 0) {
        return;  // User cancelled
    }
    
    long val = input.toInt();
    if (val < 1) val = 1;
    if (val > NEW_MODULE_MAX_VALUE) val = NEW_MODULE_MAX_VALUE;
    
    s_setting = (int)val;
    displaySuccess("Set to " + String(s_setting));
}

// Action handler: About information
static void showAbout()
{
    displayInfo(
        "NewModule v1.0\n"
        "\n"
        "Description:\n"
        "- Feature one\n"
        "- Feature two\n"
        "\n"
        "Setting: " + String(s_setting),
        true  // Blocking wait
    );
}

// Entry point (called from app.cpp)
void newModuleMenuEntry()
{
    std::vector<Option> opts;
    
    for (;;) {  // Menu loop until user exits
        opts.clear();
        
        // Define menu options
        opts.push_back({"Run Action", [](){ doMainAction(); }});
        opts.push_back({"Setting: " + String(s_setting), [](){ changeSetting(); }});
        opts.push_back({"About", [](){ showAbout(); }});
        opts.push_back({"Back", [](){}});  // Must be last
        
        // Render menu and wait for selection
        int sel = loopOptions(opts, MENU_TYPE_REGULAR, "NewModule");
        
        // Exit conditions: ESC pressed or Back selected
        if (sel == -1 || sel == (int)opts.size() - 1) {
            return;
        }
    }
}
```

### Key APIs Reference

#### Drawing Functions (`ui.h`)

| Function | Description |
|----------|-------------|
| `drawMainBorder()` | Clear screen and draw status bar |
| `drawMainBorderWithTitle(title)` | Draw border with custom title |
| `displayError(text)` | Show red error banner |
| `displayWarning(text)` | Show yellow warning banner |
| `displayInfo(text)` | Show blue info banner |
| `displaySuccess(text)` | Show green success banner |
| `logPrint(text)` | Append to scrollable log buffer |
| `logRender()` | Redraw log area |
| `animEnter()` | Fade-in transition effect |
| `animExit()` | Fade-out transition effect |
| `uiBeep(freq, dur)` | Play beep sound |

#### Input Functions (`keyboard.h`, `menu.h`)

| Function | Description |
|----------|-------------|
| `keyboard(default, maxLen, prompt)` | Open text input dialog |
| `check(keyPress)` | Check if key was pressed (edge-triggered) |
| `pollKeyboard()` | Update keyboard state (call every loop) |
| `waitAllKeysReleased()` | Block until all keys released |
| `loopOptions(opts, type, title)` | Run interactive menu |

#### Key Press Enum Values

| Constant | Trigger |
|----------|---------|
| `PrevPress` | Up arrow |
| `NextPress` | Down arrow |
| `SelPress` | Enter/Select |
| `EscPress` | ESC/Back |
| `LeftPress` | Left arrow |
| `RightPress` | Right arrow |

#### Screen Constants (`config.h`)

| Constant | Value | Description |
|----------|-------|-------------|
| `SCREEN_WIDTH` | 240 | Display width in pixels |
| `SCREEN_HEIGHT` | 135 | Display height in pixels |
| `FP` | 1 | Small font (6x8 px) |
| `FM` | 2 | Medium font (12x16 px) |
| `FG` | 3 | Large font (18x24 px) |
| `BORDER_PAD_X` | 10 | Horizontal margin |
| `BORDER_PAD_Y` | 28 | Content start Y |

### Common Patterns

#### Pattern 1: Scrollable List Display

For displaying lists longer than the screen height:

```cpp
static std::vector<String> s_items;
static int s_scrollOffset = 0;

static void renderList()
{
    M5.Display.fillScreen(kitenConfig.bgColor);
    drawMainBorder(false);
    
    M5.Display.setTextSize(FM);
    M5.Display.setTextColor(kitenConfig.priColor, kitenConfig.bgColor);
    M5.Display.setCursor(BORDER_PAD_X, BORDER_PAD_Y);
    M5.Display.print("Results");
    
    int startY = BORDER_PAD_Y + 22;
    int lineHeight = 10;
    int maxVisible = (SCREEN_HEIGHT - startY - 12) / lineHeight;
    
    M5.Display.setTextSize(FP);
    M5.Display.setTextColor(TFT_WHITE, kitenConfig.bgColor);
    
    for (int i = 0; i < maxVisible && (s_scrollOffset + i) < (int)s_items.size(); i++) {
        M5.Display.setCursor(BORDER_PAD_X, startY + i * lineHeight);
        M5.Display.print(s_items[s_scrollOffset + i]);
    }
    
    // Scroll indicators
    if (s_scrollOffset > 0) {
        M5.Display.setCursor(SCREEN_WIDTH - 20, SCREEN_HEIGHT - 12);
        M5.Display.print("^");
    }
    if (s_scrollOffset + maxVisible < (int)s_items.size()) {
        M5.Display.setCursor(SCREEN_WIDTH - 10, SCREEN_HEIGHT - 12);
        M5.Display.print("v");
    }
}
```

#### Pattern 2: Non-blocking Long Operations

Replace blocking delays with polled loops:

```cpp
unsigned long startTime = millis();
while (millis() - startTime < 5000) {  // 5 second timeout
    M5.update();
    pollKeyboard();
    
    if (check(EscPress)) {
        displayWarning("Cancelled");
        return;
    }
    
    // Update progress display here
    delay(10);  // Short delay, then poll again
}
```

#### Pattern 3: Persistent Data Storage

Use Preferences library (NVS) to save data across reboots:

```cpp
#include <Preferences.h>

void saveSetting(int value) {
    Preferences prefs;
    prefs.begin("mymodule");  // Namespace
    prefs.putInt("setting", value);
    prefs.end();
}

int loadSetting(int defaultValue) {
    Preferences prefs;
    prefs.begin("mymodule");
    int value = prefs.getInt("setting", defaultValue);
    prefs.end();
    return value;
}
```

---

## Troubleshooting

### Compilation Errors

| Error Message | Cause | Solution |
|--------------|-------|----------|
| `'functionName' was not declared` | Missing `#include` | Add the corresponding header include |
| `redefinition of 'void functionName()'` | Duplicate source file | Remove duplicate from project |
| `'kitenConfig' was not declared` | Missing settings include | Add `#include "settings.h"` |
| `'struct Option' has no member` | Outdated menu.h | Update to latest version |

### Runtime Issues

| Symptom | Likely Cause | Solution |
|---------|--------------|----------|
| Unresponsive keyboard | Blocking `delay()` > 100ms | Add `pollKeyboard()` calls |
| Screen corruption | Out-of-bounds drawing coordinates | Validate all coordinates before drawing |
| Random crashes | Stack overflow from large locals | Move large buffers to static/global scope |
| Memory exhaustion | Too many Strings in memory | Reduce vector sizes, reuse buffers |

### Debugging Techniques

1. **Serial output:** Use `Serial.println()` for debug messages, view via `pio device monitor`
2. **Heap monitoring:** Call `ESP.getFreeHeap()` to track memory usage
3. **Visual debugging:** Use `displayInfo()` to show variable states on screen

### Testing Checklist

After modifying or creating a module, verify:

- [ ] Module appears in main menu with correct icon
- [ ] Selecting module opens submenu correctly
- [ ] All menu options execute without crashes
- [ ] Settings persist during session (if applicable)
- [ ] ESC key exits at all levels properly
- [ ] Long operations can be cancelled with ESC
- [ ] No visual artifacts after extended use
- [ ] Keyboard remains responsive during operations
- [ ] Power-save/dim-screen activates correctly

---

## License

This project is provided for educational and authorized security testing purposes only. Users are responsible for complying with all applicable laws and regulations regarding the use of these tools. The authors assume no liability for misuse.

---

## Acknowledgments

Based on KITEN firmware architecture by KI10sus, originally ported from Bruce Devices firmware. Built using the M5Unified library and PlatformIO framework.
