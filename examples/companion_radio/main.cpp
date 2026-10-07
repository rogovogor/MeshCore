#include <Arduino.h>   // needed for PlatformIO
#include <Mesh.h>
#include "MyMesh.h"

// ESP32 companion power saving. See agent_docs/power.md.
//
// The mechanism is automatic light sleep, not a manual sleep call: the BLE
// controller keeps the connection and wakes the CPU for a connection event,
// while the application yields so the FreeRTOS idle task can hand the CPU over.
// ESP32Board::sleep() is deliberately NOT used for a companion — it wakes only
// on the radio DIO pin and a timer, so BLE connection events would be missed and
// the app would see the node drop off.
//
// This needs a framework built with CONFIG_PM_ENABLE and FreeRTOS tickless idle.
// Stock arduino-esp32 2.0.17 (platformio/espressif32@6.11.0) ships both
// disabled, so esp_pm_configure() returns ESP_ERR_NOT_SUPPORTED there: the code
// below then only reports the failure and the node behaves exactly as before.
#if defined(ESP32) && defined(WITH_ESP32_POWER_SAVING)
  #include "esp_pm.h"
  #include "esp_bt.h"

  #define ESP32_PM_CPU_MAX_MHZ   80
  #define ESP32_PM_CPU_MIN_MHZ   40
  // Native USB-CDC (and the USB companion transport) does not survive automatic
  // light sleep, and WiFi keeps its own reason to stay awake — for those builds
  // only the frequency scaling is enabled.
  #if (defined(ARDUINO_USB_CDC_ON_BOOT) && ARDUINO_USB_CDC_ON_BOOT) \
      || defined(ENABLE_USB_INTERFACE) || defined(WIFI_SSID) || defined(WITH_WIFI_SWITCHING)
    #define ESP32_PM_LIGHT_SLEEP   0
  #else
    #define ESP32_PM_LIGHT_SLEEP   1
  #endif
#endif

// Believe it or not, this std C function is busted on some platforms!
static uint32_t _atoi(const char* sp) {
  uint32_t n = 0;
  while (*sp && *sp >= '0' && *sp <= '9') {
    n *= 10;
    n += (*sp++ - '0');
  }
  return n;
}

// interface manager
#include <helpers/MultiSerialInterface.h>
MultiSerialInterface interface_manager;

// include bluetooth interface
#if defined(BLE_PIN_CODE)
  #ifdef ESP32
    // include esp32 bluetooth interface
    #include <helpers/esp32/SerialBLEInterface.h>
    SerialBLEInterface bluetooth_interface;
  #elif defined(NRF52_PLATFORM)
    // include nrf52 bluetooth interface
    #include <helpers/nrf52/SerialBLEInterface.h>
    SerialBLEInterface bluetooth_interface;
  #else
    #error "SerialBLEInterface is not defined for this platform"
  #endif
#endif

// include wifi interface
#ifdef WIFI_SSID
  #ifndef TCP_PORT
    #define TCP_PORT 5000
  #endif
  #ifdef ESP32
    // include esp32 wifi interface
    #include <helpers/esp32/SerialWifiInterface.h>
    SerialWifiInterface wifi_interface;
  #else
    #error "SerialWifiInterface is not defined for this platform"
  #endif
#endif

// include usb interface
#if defined(ENABLE_USB_INTERFACE)
  #include <helpers/ArduinoSerialInterface.h>
  ArduinoSerialInterface usb_serial_interface;
#endif

// include ethernet interface
#if defined(ETHERNET_ENABLED)
  #include <helpers/ethernet/EthernetInterface.h>
  ETHERNET_CLASS ethernet_interface;
#endif

// include hardware serial interface
#if defined(SERIAL_RX)
  #include <helpers/ArduinoSerialInterface.h>
  ArduinoSerialInterface hardware_serial_interface;
  HardwareSerial companion_serial(1);
#endif

// platform file system
#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  #include <InternalFileSystem.h>
  #if defined(QSPIFLASH)
    #include <CustomLFS_QSPIFlash.h>
    DataStore store(InternalFS, QSPIFlash, rtc_clock);
  #else
    #if defined(EXTRAFS)
      #include <CustomLFS.h>
      CustomLFS ExtraFS(0xD4000, 0x19000, 128);
      DataStore store(InternalFS, ExtraFS, rtc_clock);
    #else
      DataStore store(InternalFS, rtc_clock);
    #endif
  #endif
#elif defined(RP2040_PLATFORM)
  #include <LittleFS.h>
  DataStore store(LittleFS, rtc_clock);
#elif defined(ESP32)
  #include <SPIFFS.h>
  DataStore store(SPIFFS, rtc_clock);
#endif

/* GLOBAL OBJECTS */
#ifdef DISPLAY_CLASS
  #include "UITask.h"
  UITask ui_task(&board, &interface_manager);
#endif

StdRNG fast_rng;
SimpleMeshTables tables;
MyMesh the_mesh(radio_driver, fast_rng, rtc_clock, tables, store
   #ifdef DISPLAY_CLASS
      , &ui_task
   #endif
);

/* END GLOBAL OBJECTS */

void halt() {
  while (1) ;
}

/* WIFI RECONNECT TRACKERS */
#if defined(ESP32) && defined(WIFI_SSID)
  bool wifi_needs_reconnect = false;
  unsigned long last_wifi_reconnect_attempt = 0;
#endif

void setup() {
  Serial.begin(115200);
  board.begin();

#ifdef HAS_EXTERNAL_WATCHDOG
  external_watchdog.begin();
#endif

#ifdef DISPLAY_CLASS
  DisplayDriver* disp = NULL;
  if (display.begin()) {
    disp = &display;
    disp->startFrame();
  #ifdef ST7789
    disp->setTextSize(2);
  #endif
    disp->drawTextCentered(disp->width() / 2, 28, "Loading...");
    disp->endFrame();
  }
#endif

  if (!radio_init()) { halt(); }

  fast_rng.begin(radio_driver.getRngSeed());

#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  InternalFS.begin();
  #if defined(QSPIFLASH)
    if (!QSPIFlash.begin()) {
      // debug output might not be available at this point, might be too early. maybe should fall back to InternalFS here?
      MESH_DEBUG_PRINTLN("CustomLFS_QSPIFlash: failed to initialize");
    } else {
      MESH_DEBUG_PRINTLN("CustomLFS_QSPIFlash: initialized successfully");
    }
  #else
  #if defined(EXTRAFS)
      ExtraFS.begin();
  #endif
  #endif
  store.begin();
  the_mesh.begin(
    #ifdef DISPLAY_CLASS
        disp != NULL
    #else
        false
    #endif
  );
#elif defined(RP2040_PLATFORM)
  LittleFS.begin();
  store.begin();
  the_mesh.begin(
    #ifdef DISPLAY_CLASS
        disp != NULL
    #else
        false
    #endif
  );
#elif defined(ESP32)
  SPIFFS.begin(true);
  store.begin();
  the_mesh.begin(
    #ifdef DISPLAY_CLASS
        disp != NULL
    #else
        false
    #endif
  );
#else
  #error "need to define filesystem"
#endif

// add bluetooth interface
#if defined(BLE_PIN_CODE)
  bluetooth_interface.begin(BLE_NAME_PREFIX, the_mesh.getNodePrefs()->node_name, the_mesh.getBLEPin());
  interface_manager.addInterface(InterfaceType::Bluetooth, &bluetooth_interface);
#endif

// add wifi interface
#ifdef WIFI_SSID
  board.setInhibitSleep(true);   // prevent sleep when WiFi is active
  WiFi.setAutoReconnect(true);

  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info){
      if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
          WIFI_DEBUG_PRINTLN("WiFi disconnected. Flagging for reconnect...");
          wifi_needs_reconnect = true;
      } else if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
          WIFI_DEBUG_PRINTLN("WiFi connected successfully!");
          wifi_needs_reconnect = false;
      }
  });

  WiFi.begin(WIFI_SSID, WIFI_PWD);
  wifi_interface.begin(TCP_PORT);
  interface_manager.addInterface(InterfaceType::WiFi, &wifi_interface);
#endif

// add usb interface
#if defined(ENABLE_USB_INTERFACE)
  usb_serial_interface.begin(Serial);
  interface_manager.addInterface(InterfaceType::USB, &usb_serial_interface);
#endif

// add ethernet interface
#if defined(ETHERNET_ENABLED)
  ethernet_interface.begin();
  interface_manager.addInterface(InterfaceType::Ethernet, &ethernet_interface);
#endif

// add hardware serial interface
#if defined(SERIAL_RX)
  companion_serial.setPins(SERIAL_RX, SERIAL_TX);
  companion_serial.begin(115200);
  hardware_serial_interface.begin(companion_serial);
  interface_manager.addInterface(InterfaceType::HardwareSerial, &hardware_serial_interface);
#endif

  the_mesh.startInterface(interface_manager);
  sensors.begin();

#if ENV_INCLUDE_GPS == 1
  the_mesh.applyGpsPrefs();
#endif

#ifdef DISPLAY_CLASS
  ui_task.begin(disp, &sensors, the_mesh.getNodePrefs());  // still want to pass this in as dependency, as prefs might be moved
#endif

  board.onBootComplete();

#if defined(ESP32) && defined(WITH_ESP32_POWER_SAVING)
  #if defined(BLE_PIN_CODE)
    // Controller-side modem sleep: the radio sleeps between connection events.
    // On ESP32-S3/C3 this only exists if the framework was built with
    // CONFIG_BT_CTRL_MODEM_SLEEP, hence the report rather than an abort.
    esp_err_t err_bt = esp_bt_sleep_enable();
    if (err_bt != ESP_OK) {
      Serial.printf("ESP32 PM: BLE sleep not available (%d)\n", (int)err_bt);
    }
  #endif

  // The per-target type is what IDF 4.4 expects; the generic one is only
  // declared for IDF 5 targets (ESP32-C6 here).
  #if CONFIG_IDF_TARGET_ESP32C3
    esp_pm_config_esp32c3_t pm_config;
  #elif CONFIG_IDF_TARGET_ESP32S3
    esp_pm_config_esp32s3_t pm_config;
  #elif CONFIG_IDF_TARGET_ESP32
    esp_pm_config_esp32_t pm_config;
  #elif CONFIG_IDF_TARGET_ESP32C6
    esp_pm_config_t pm_config;
  #else
    #error "No esp_pm_config_t for this target"
  #endif

  pm_config.max_freq_mhz = ESP32_PM_CPU_MAX_MHZ;
  pm_config.min_freq_mhz = ESP32_PM_CPU_MIN_MHZ;
  pm_config.light_sleep_enable = ESP32_PM_LIGHT_SLEEP != 0;

  esp_err_t err_pm = esp_pm_configure(&pm_config);
  if (err_pm == ESP_OK) {
    Serial.printf("ESP32 PM: %d-%d MHz, light sleep %s\n",
                  (int)ESP32_PM_CPU_MAX_MHZ, (int)ESP32_PM_CPU_MIN_MHZ,
                  ESP32_PM_LIGHT_SLEEP ? "on" : "off");
  } else {
    Serial.printf("ESP32 PM: unavailable (%d) - framework built without CONFIG_PM_ENABLE\n",
                  (int)err_pm);
  }
#endif
}

void loop() {
  the_mesh.loop();
  interface_manager.loop();
  sensors.loop();
#if defined(DISPLAY_CLASS) && defined(WITH_ASYNC_EINK)
  if (!the_mesh.isRadioTxBusy()) display.service();
#endif
#ifdef DISPLAY_CLASS
  ui_task.loop();
#endif
  rtc_clock.tick();
#ifdef HAS_EXTERNAL_WATCHDOG
  external_watchdog.loop();
#endif

  if (!the_mesh.hasPendingWork()) {
#if defined(NRF52_PLATFORM)
    board.sleep(0); // nrf ignores seconds param, sleeps whenever possible
#elif defined(ESP32) && defined(WITH_ESP32_POWER_SAVING) && ESP32_PM_LIGHT_SLEEP
    // Walking the loop as fast as it can keeps the CPU out of light sleep, and
    // every wake-up costs a TCXO start on top. Yielding to the idle task is what
    // actually lets the chip sleep; while a received frame is still queued the
    // loop keeps running instead so the frame is handed over promptly.
    if (!serial_interface.isReadBusy() && !serial_interface.isWriteBusy()) {
      vTaskDelay(pdMS_TO_TICKS(10));
    }
#endif
  }

#if defined(ESP32) && defined(WIFI_SSID)
  // Safely attempt to reconnect every 10 seconds if flagged
  if (wifi_needs_reconnect && (millis() - last_wifi_reconnect_attempt > 10000)) {
    WIFI_DEBUG_PRINTLN("Attempting manual WiFi reconnect...");
    WiFi.disconnect();
    WiFi.reconnect();
    last_wifi_reconnect_attempt = millis();
  }
#endif
}
