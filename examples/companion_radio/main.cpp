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
  // How long the idle path yields to the FreeRTOS idle task: this is what actually
  // lets the chip sleep, and how long depends on the profile.
  #define ESP32_PM_IDLE_YIELD_MS            10
  #define ESP32_PM_IDLE_YIELD_AGGRESSIVE_MS 20
  // How long one press of the user button keeps the CPU awake — enough to read the
  // console and type a CLI command.
  #define ESP32_PM_USB_HOLD_MS   30000

  // Boards whose Serial runs over the ESP32-S3's own USB peripheral cannot sleep
  // while a host is attached: that link is serviced by the CPU, so automatic light
  // sleep drops it — not just the log, the port itself. But the transport here is
  // BLE, so the port is only logs and CLI, while the node spends its life on a
  // battery with nothing plugged in. So light sleep stays enabled, and two things
  // keep the console usable:
  //   * the link is held awake automatically while a USB host is detected;
  //   * a press of the user button holds the CPU awake for a window — the fallback
  //     for when detection cannot tell, and the way in when the cable appeared
  //     while the node was asleep (USB cannot wake the CPU, but BLE and LoRa
  //     traffic wakes it constantly, so the press is noticed almost at once).
  // WITH_ESP32_POWER_SAVING_USB_SERIAL_SAFE restores the conservative rule: no
  // light sleep at all on such boards, matching the fork's PS 17.1.5 choice.
  #if defined(ARDUINO_USB_CDC_ON_BOOT) && ARDUINO_USB_CDC_ON_BOOT
    #define ESP32_PM_NATIVE_USB    1
  #else
    #define ESP32_PM_NATIVE_USB    0
  #endif

  // WITH_WIFI_SWITCHING is not in the list: there the transport is chosen at runtime,
  // so light sleep stays configured and esp32_pm_wantsAwake() holds the lock while
  // the active transport is WiFi or USB.
  #if defined(WIFI_SSID) || defined(ENABLE_USB_INTERFACE)
    // The transport itself is WiFi or USB: sleeping breaks the link, not just the log.
    #define ESP32_PM_LIGHT_SLEEP   0
    #define ESP32_PM_USB_GUARD     0
  #elif ESP32_PM_NATIVE_USB && !defined(WITH_ESP32_POWER_SAVING_USB_SERIAL_SAFE)
    #define ESP32_PM_LIGHT_SLEEP   1
    #define ESP32_PM_USB_GUARD     1
  #else
    #define ESP32_PM_LIGHT_SLEEP   1
    #define ESP32_PM_USB_GUARD     0
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
// (not with WITH_WIFI_SWITCHING: there MyMesh owns its own BLE and WiFi interfaces,
// and a second SerialBLEInterface would initialise the BLE stack twice)
#if defined(BLE_PIN_CODE) && !defined(WITH_WIFI_SWITCHING)
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

#if defined(ESP32) && defined(WITH_ESP32_POWER_SAVING)
// Power saving is the feature; the profile says how far to take it. The lock below
// is held whenever the CPU must stay awake instead, and three things ask for that:
// a USB host on the line (that link is serviced by the CPU), the app connected in
// the CONSERVATIVE profile, and a button-granted window for reading the console.
static esp_pm_lock_handle_t esp32_pm_lock = NULL;
static bool esp32_pm_lock_held = false;
static uint32_t esp32_pm_hold_until = 0;
static bool esp32_pm_window_active = false;
static uint16_t esp32_pm_yield_ms = ESP32_PM_IDLE_YIELD_MS;
static uint8_t esp32_pm_applied_mode = 0xFF;

#if ESP32_PM_USB_GUARD
static bool esp32_usbHostPresent() {
  #if defined(ARDUINO_USB_MODE) && ARDUINO_USB_MODE
    return Serial.isConnected();   // HWCDC: hardware USB-Serial-JTAG link
  #else
    return (bool)Serial;           // USBCDC over TinyUSB: host has the port open
  #endif
}
#endif

static uint16_t esp32_pm_yieldFor(uint8_t mode) {
  return (mode == POWER_MODE_AGGRESSIVE) ? ESP32_PM_IDLE_YIELD_AGGRESSIVE_MS
                                         : ESP32_PM_IDLE_YIELD_MS;
}

static bool esp32_pm_wantsAwake() {
  if (ESP32_PM_LIGHT_SLEEP == 0) return false;   // this transport forbids sleeping
  #if defined(WITH_WIFI_SWITCHING)
    if (the_mesh.commsForbidsSleep()) return true;
  #endif
  #if ESP32_PM_USB_GUARD
    if (esp32_usbHostPresent()) return true;
  #endif
  if (the_mesh.getNodePrefs()->power_mode == POWER_MODE_CONSERVATIVE
      && interface_manager.isConnected()) {
    return true;
  }
  return esp32_pm_window_active;
}

static void esp32_servicePmHold() {
  #if defined(PIN_USER_BTN) && (PIN_USER_BTN >= 0)
    // Active low with a pull-up. One press buys a window with the CPU awake, so the
    // console can be read even when host detection says nothing is attached.
    if (digitalRead(PIN_USER_BTN) == LOW) {
      esp32_pm_window_active = true;
      esp32_pm_hold_until = millis() + ESP32_PM_USB_HOLD_MS;
    }
  #endif

  // The window is a flag, not a signed difference: millis() wraps every ~49.7 days,
  // and a naive (int32_t)(hold_until - millis()) > 0 turns positive again ~24.8 days
  // after the wrap, holding the lock with no sleep. Clear the flag once the deadline
  // has passed; the unsigned subtraction is correct across the millis() wrap.
  if (esp32_pm_window_active && ((int32_t)(millis() - esp32_pm_hold_until) >= 0)) {
    esp32_pm_window_active = false;
  }

  if (esp32_pm_lock == NULL) return;
  const bool hold = esp32_pm_wantsAwake();
  if (hold == esp32_pm_lock_held) return;
  if (hold) {
    esp_pm_lock_acquire(esp32_pm_lock);
  } else {
    esp_pm_lock_release(esp32_pm_lock);
  }
  esp32_pm_lock_held = hold;
}

// Apply the profile: at startup, and again whenever the setting changes, because
// the CLI can flip it while the node is running.
static void esp32_applyPowerMode(uint8_t mode) {
  esp32_pm_yield_ms = esp32_pm_yieldFor(mode);

  // IDF 4.4 wants a per-target config struct; IDF 5 targets (C6) take the generic one.
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
  pm_config.light_sleep_enable = (ESP32_PM_LIGHT_SLEEP != 0) && (mode != POWER_MODE_OFF);

  esp_err_t err_pm = esp_pm_configure(&pm_config);
  esp32_pm_applied_mode = mode;
  if (err_pm == ESP_OK) {
    Serial.printf("ESP32 PM: %d-%d MHz, light sleep %s, mode %u\n",
                  (int)ESP32_PM_CPU_MAX_MHZ, (int)ESP32_PM_CPU_MIN_MHZ,
                  pm_config.light_sleep_enable ? "on" : "off", (unsigned)mode);
  } else {
    Serial.printf("ESP32 PM: unavailable (%d) - framework built without CONFIG_PM_ENABLE\n",
                  (int)err_pm);
  }
}

static void esp32_servicePowerPrefs() {
  const uint8_t mode = the_mesh.getNodePrefs()->power_mode;
  if (mode != esp32_pm_applied_mode) {
    esp32_applyPowerMode(mode);
  }
}

static void esp32_setupPower() {
  #if defined(PIN_USER_BTN) && (PIN_USER_BTN >= 0)
    pinMode(PIN_USER_BTN, INPUT_PULLUP);
  #endif
  // Take the lock BEFORE light sleep is switched on, so a USB port is never dropped
  // in the first moments.
  if (esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP, 0, "power", &esp32_pm_lock) != ESP_OK) {
    esp32_pm_lock = NULL;
    Serial.println("ESP32 PM: no lock available");
  } else if (esp32_pm_wantsAwake()) {
    esp_pm_lock_acquire(esp32_pm_lock);
    esp32_pm_lock_held = true;
  }
  esp32_applyPowerMode(the_mesh.getNodePrefs()->power_mode);
}
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
#if defined(BLE_PIN_CODE) && !defined(WITH_WIFI_SWITCHING)
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

#ifdef WITH_WIFI_SWITCHING
  // MyMesh picks the transport itself: it loads /wifi_prefs, brings up its own BLE,
  // starts WiFi if that was the saved mode, and switches between them at runtime.
  // Going through interface_manager instead would leave the uni build with no
  // transport at all — nothing registers there when BLE_PIN_CODE is not set.
  the_mesh.initCommsFromPrefs();
#else
  the_mesh.startInterface(interface_manager);
#endif
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

  esp32_setupPower();
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

  #if defined(ESP32) && defined(WITH_ESP32_POWER_SAVING)
    // Cheap: a couple of register reads and bool compares. Runs while the node is
    // busy too, so both the lock state and the profile always match the settings.
    esp32_servicePowerPrefs();
    esp32_servicePmHold();
  #endif

  if (!the_mesh.hasPendingWork()) {
#if defined(NRF52_PLATFORM)
    // nrf ignores the seconds param and sleeps whenever possible; the profile only
    // says whether to stay awake while the app is connected, or not to sleep at all.
    // AUTO is what this platform always did.
    const uint8_t power_mode = the_mesh.getNodePrefs()->power_mode;
    if (power_mode != POWER_MODE_OFF
        && (power_mode != POWER_MODE_CONSERVATIVE || !interface_manager.isConnected())) {
      board.sleep(0);
    }
#elif defined(ESP32) && defined(WITH_ESP32_POWER_SAVING) && ESP32_PM_LIGHT_SLEEP
    // Walking the loop as fast as it can keeps the CPU out of light sleep, and
    // every wake-up costs a TCXO start on top. Yielding to the idle task is what
    // actually lets the chip sleep; while a received frame is still queued the
    // loop keeps running instead so the frame is handed over promptly.
    // With WiFi or USB active the lock keeps the chip awake anyway, so the yield
    // would only add latency to a link interface_manager does not even see.
    if (!interface_manager.isReadBusy() && !interface_manager.isWriteBusy()
  #if defined(WITH_WIFI_SWITCHING)
        && !the_mesh.commsForbidsSleep()
  #endif
       ) {
      vTaskDelay(pdMS_TO_TICKS(esp32_pm_yield_ms));
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
