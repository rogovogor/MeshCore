#include "variant.h"
#include "wiring_constants.h"
#include "wiring_digital.h"
#include "nrf.h"

const uint32_t g_ADigitalPinMap[] =
{
    // D0 - D9 (left side, top to bottom)
    6,  // D0 is P0.06 (D0, TX)
    8,  // D1 is P0.08 (D1, RX)
    17, // D2 is P0.17 (D2, SCK)
    20, // D3 is P0.20 (D3, MISO)
    22, // D4 is P0.22 (D4, MOSI)
    24, // D5 is P0.24 (D5, CS)
    32, // D6 is P1.00 (D6, SDA)
    11, // D7 is P0.11 (D7, SCL)
    36, // D8 is P1.04 (D8)
    38, // D9 is P1.06 (D9)

    // D10 - D17 (right side, bottom to top)
    9,  // D10 is P0.09 (D10, NFC1)
    10, // D11 is P0.10 (D11, NFC2)
    43, // D12 is P1.11 (D12)
    45, // D13 is P1.13 (D13, SDA1)
    47, // D14 is P1.15 (D14, SCL1)
    2,  // D15 is P0.02 (D15, A0)
    29, // D16 is P0.29 (D16, A1)
    31, // D17 is P0.31 (D17, A2)

    // D18 - D20 (extra center pins, left to right)
    33, // D18 is P1.01 (D18, SCK1)
    34, // D19 is P1.02 (D19, MISO1)
    39, // D20 is P1.07 (D20, MOSI1)

    // EXT_VCC
    13, // 'D21' is P0.13 (EXT_VCC) [active high]

    // LED
    15, // 'D22' is P0.15 (LED)
};

void initVariant()
{
    // enable EXT_VCC by default
    pinMode(EXT_VCC, OUTPUT);
    digitalWrite(EXT_VCC, HIGH);

    // enable LED by default
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);
}

