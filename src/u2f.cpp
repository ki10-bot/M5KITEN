

#include "others_menu.h"
#include "config.h"
#include "settings.h"
#include "ui.h"
#include <Arduino.h>
#include <M5Unified.h>

void u2f_setup_impl()
{
    displayInfo(
        "USB U2F Security Key\n"
        "\n"
        "KITEN's u2f.cpp is\n"
        "1,714 lines and uses\n"
        "mbedTLS ECDSA + AES +\n"
        "CTR_DRBG. Requires\n"
        "build flags:\n"
        "  CONFIG_MBEDTLS_ECDH_C\n"
        "  CONFIG_MBEDTLS_AES_C\n"
        "  CONFIG_MBEDTLS_CTR_DRBG\n"
        "\n"
        "Coming in a future\n"
        "KITEN build.",
        true);
}
