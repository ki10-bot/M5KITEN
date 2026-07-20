

#include "others_menu.h"
#include "config.h"
#include "settings.h"
#include "ui.h"
#include <Arduino.h>
#include <M5Unified.h>

void setup_ibutton_impl()
{
    displayInfo(
        "iButton\n"
        "\n"
        "Reads / writes DS1990A\n"
        "iButton keys over\n"
        "1-Wire.\n"
        "\n"
        "Hardware required:\n"
        "  - iButton probe /\n"
        "    coin-cell holder\n"
        "  - 4.7k pull-up to VCC\n"
        "  - Wired to a GPIO\n"
        "\n"
        "Add OneWire lib\n"
        "(bmorcelli fork) to\n"
        "platformio.ini to\n"
        "enable.",
        true);
}
