#pragma once

#define RADIOLIB_STATIC_ONLY 1
#include <RadioLib.h>
#include <helpers/RadioLibWrappers.h>
#include <helpers/nrf52/PromicroBoard.h>
#include <helpers/CustomSX1262Wrapper.h>
#include <helpers/CustomLLCC68Wrapper.h>
#include <helpers/AutoDiscoverRTCClock.h>
#include <helpers/SensorManager.h>
#ifdef DISPLAY_CLASS
  #include <helpers/ui/SSD1306Display.h>
#endif

#define NUM_SENSOR_SETTINGS 3
#include <helpers/sensors/EnvironmentSensorManager.h>

extern PromicroBoard board;
extern WRAPPER_CLASS radio_driver;
extern AutoDiscoverRTCClock rtc_clock;

#ifdef DISPLAY_CLASS
  extern DISPLAY_CLASS display;
#endif

bool radio_init();
uint32_t radio_get_rng_seed();
void radio_set_params(float freq, float bw, uint8_t sf, uint8_t cr);
void radio_set_tx_power(uint8_t dbm);
mesh::LocalIdentity radio_new_identity();

extern EnvironmentSensorManager sensors;