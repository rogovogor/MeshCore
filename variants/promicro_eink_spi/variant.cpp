#include "variant.h"
#include "wiring_constants.h"
#include "wiring_digital.h"

// Arduino D# -> nRF52840 GPIO bit mapping
// D0=P0.08, D1=P0.06, D2=P0.17, D3=P0.20, D4=P0.22, D5=P0.24,
// D6=P1.00, D7=P0.11, D8=P1.04, D9=P1.06, D10=P0.09, D11=P0.10,
// D12=P1.11, D13=P1.13, D14=P1.15, D15=P0.02, D16=P0.29, D17=P0.31,
// D18=P1.01, D19=P1.02, D20=P1.05, D21=P0.13, D22=P0.15
// GxEPD2 requires these symbols; point to SPI1 (display bus) as a safe default.
const int MISO = PIN_SPI1_MISO;
const int MOSI = PIN_SPI1_MOSI;
const int SCK  = PIN_SPI1_SCK;

const uint32_t g_ADigitalPinMap[] = {
  8, 6, 17, 20, 22, 24, 32, 11, 36, 38,
  9, 10, 43, 45, 47, 2, 29, 31,
  33, 34, 37,
  13, 15
};

void initVariant()
{
}
