#include <Arduino.h>   // needed for PlatformIO
#include <Mesh.h>

#include "MyMesh.h"

#ifdef DISPLAY_CLASS
  #include "UITask.h"
  static UITask ui_task(board, display);
#endif

#ifdef ETHERNET_ENABLED
  #define ETHERNET_CLI_BANNER "MeshCore Repeater CLI"
  #include <helpers/nrf52/EthernetCLI.h>
#endif

StdRNG fast_rng;
SimpleMeshTables tables;

MyMesh the_mesh(board, radio_driver, *new ArduinoMillis(), fast_rng, rtc_clock, tables);

void halt() {
  while (1) ;
}

static char command[160];
#ifdef ETHERNET_ENABLED
static char ethernet_command[160];
#endif

// For power saving
unsigned long POWERSAVING_FIRSTSLEEP_SECS = 120; // The first sleep (if enabled) from boot

#if defined(PIN_USER_BTN) && defined(_SEEED_SENSECAP_SOLAR_H_)
static unsigned long userBtnDownAt = 0;
#define USER_BTN_HOLD_OFF_MILLIS 1500
#endif

#ifdef AUTO_REBOOT_MS
static unsigned long startup_millis = 0;
#endif

#if defined(ESP32) && defined(WITH_ESP32_POWER_SAVING)
// Repeater automatic power saving: the same mechanism as the companion —
// automatic light sleep via esp_pm_configure() plus a loop yield to the idle
// task, instead of a manual board.sleep(). Needs a framework built with
// CONFIG_PM_ENABLE; on a stock one esp_pm_configure() reports
// ESP_ERR_NOT_SUPPORTED and the node runs as if powersaving were off.
#if defined(WITH_ESP32_PM_REQUIRED)
  // A *_ps environment: refuse to build a "power saving" image on a core that
  // cannot sleep. Install the PS core once: python tools/ps_framework.py
  #include "sdkconfig.h"
  #if !CONFIG_PM_ENABLE
    #error "WITH_ESP32_PM_REQUIRED: framework built without CONFIG_PM_ENABLE - run: python tools/ps_framework.py"
  #endif
#endif
#include "esp_pm.h"
#include "esp_sleep.h"
#include "driver/uart.h"

#define REPEATER_PM_CPU_MAX_MHZ   80
#define REPEATER_PM_CPU_MIN_MHZ   40

// How long the loop yields when there is nothing urgent to do; this is what
// actually hands the CPU to light sleep (or to DFS down to 40 MHz).
#ifndef REPEATER_PM_YIELD_MS
  #define REPEATER_PM_YIELD_MS    50
#endif

// Keep-awake windows: first boot, CLI serial traffic, user button.
#define REPEATER_PM_SERIAL_HOLD_MS  60000
#define REPEATER_PM_BTN_HOLD_MS     30000

// Light sleep is allowed only when Serial is a real UART (no native USB CDC) and
// there is no bridge, Ethernet or WiFi link for the CPU to service. Heltec v3 is
// UART0 through a CP2102, so it sleeps; everything else gets DFS (frequency
// scaling) but no sleep.
#if (defined(ARDUINO_USB_CDC_ON_BOOT) && ARDUINO_USB_CDC_ON_BOOT) \
    || defined(WITH_BRIDGE) || defined(ETHERNET_ENABLED) || defined(WIFI_SSID)
  #define REPEATER_PM_LIGHT_SLEEP   0
#else
  #define REPEATER_PM_LIGHT_SLEEP   1
#endif

static esp_pm_lock_handle_t repeater_pm_lock = NULL;
static bool repeater_pm_lock_held = false;
static uint8_t repeater_pm_applied = 0xFF;   // 0 = off, 1 = on

static bool repeater_pm_boot_active = false;
static uint32_t repeater_pm_boot_until = 0;
static bool repeater_pm_serial_active = false;
static uint32_t repeater_pm_serial_until = 0;
static bool repeater_pm_btn_active = false;
static uint32_t repeater_pm_btn_until = 0;

static bool repeater_pm_wantsAwake() {
  return repeater_pm_boot_active || repeater_pm_serial_active || repeater_pm_btn_active;
}

// Apply the powersaving_enabled pref to esp_pm_configure(). Called at setup and
// again whenever the pref changes, so `powersaving on|off` works without a reboot.
static void repeater_pm_configure(uint8_t on) {
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

  if (on) {
    pm_config.max_freq_mhz = REPEATER_PM_CPU_MAX_MHZ;
    pm_config.min_freq_mhz = REPEATER_PM_CPU_MIN_MHZ;
    pm_config.light_sleep_enable = (REPEATER_PM_LIGHT_SLEEP != 0);
  } else {
    pm_config.max_freq_mhz = REPEATER_PM_CPU_MAX_MHZ;
    pm_config.min_freq_mhz = REPEATER_PM_CPU_MAX_MHZ;   // min == max disables DFS
    pm_config.light_sleep_enable = false;
  }

  esp_err_t err = esp_pm_configure(&pm_config);
  repeater_pm_applied = on;
  if (err == ESP_OK) {
    Serial.printf("Repeater PM: %d-%d MHz, light sleep %s, %s\n",
                  (int)pm_config.max_freq_mhz, (int)pm_config.min_freq_mhz,
                  pm_config.light_sleep_enable ? "on" : "off",
                  on ? "on" : "off");
  } else {
    Serial.printf("Repeater PM: unavailable (%d) - framework without CONFIG_PM_ENABLE\n",
                  (int)err);
  }
}

static void repeater_pm_setup() {
  // NO_LIGHT_SLEEP lock: held while a keep-awake window is open.
  if (esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP, 0, "repeater", &repeater_pm_lock) != ESP_OK) {
    repeater_pm_lock = NULL;
    Serial.println("Repeater PM: no lock available");
  }

  // UART0 RX wakes the CPU so the CLI can be typed while asleep. The first few
  // characters that woke the chip are lost — the 60 s window after the last
  // received byte covers that.
  esp_err_t err = uart_set_wakeup_threshold(UART_NUM_0, 3);
  if (err != ESP_OK) {
    Serial.printf("Repeater PM: uart wakeup threshold error (%d)\n", (int)err);
  }
  err = esp_sleep_enable_uart_wakeup(UART_NUM_0);
  if (err != ESP_OK) {
    Serial.printf("Repeater PM: uart wakeup enable error (%d)\n", (int)err);
  }

  #if defined(PIN_USER_BTN) && (PIN_USER_BTN >= 0) && !defined(DISPLAY_CLASS)
    // With a display, MomentaryButton::begin() already sets INPUT_PULLUP.
    pinMode(PIN_USER_BTN, INPUT_PULLUP);
  #endif

  // Boot keep-awake window, so the CLI works right after flashing.
  repeater_pm_boot_active = true;
  repeater_pm_boot_until = millis() + POWERSAVING_FIRSTSLEEP_SECS * 1000;

  repeater_pm_configure(the_mesh.getNodePrefs()->powersaving_enabled ? 1 : 0);
}

// Service the keep-awake windows and the power pref. Runs every loop iteration.
static void repeater_pm_service() {
  #if defined(PIN_USER_BTN) && (PIN_USER_BTN >= 0)
    if (digitalRead(PIN_USER_BTN) == LOW) {
      repeater_pm_btn_active = true;
      repeater_pm_btn_until = millis() + REPEATER_PM_BTN_HOLD_MS;
    }
  #endif

  // Flags + deadlines, cleared via unsigned subtraction so the millis() wrap is
  // handled correctly (the arithmetic stays right across the ~49.7-day wrap).
  if (repeater_pm_boot_active && ((int32_t)(millis() - repeater_pm_boot_until) >= 0)) {
    repeater_pm_boot_active = false;
  }
  if (repeater_pm_serial_active && ((int32_t)(millis() - repeater_pm_serial_until) >= 0)) {
    repeater_pm_serial_active = false;
  }
  if (repeater_pm_btn_active && ((int32_t)(millis() - repeater_pm_btn_until) >= 0)) {
    repeater_pm_btn_active = false;
  }

  // Apply powersaving on|off without a reboot.
  const uint8_t want = the_mesh.getNodePrefs()->powersaving_enabled ? 1 : 0;
  if (want != repeater_pm_applied) {
    repeater_pm_configure(want);
  }

  // Hold/release the lock to match the windows.
  if (repeater_pm_lock != NULL) {
    const bool hold = repeater_pm_wantsAwake();
    if (hold != repeater_pm_lock_held) {
      if (hold) {
        esp_pm_lock_acquire(repeater_pm_lock);
      } else {
        esp_pm_lock_release(repeater_pm_lock);
      }
      repeater_pm_lock_held = hold;
    }
  }
}
#endif

void setup() {
  Serial.begin(115200);
  delay(1000);

  board.begin();

#ifdef HAS_EXTERNAL_WATCHDOG
  external_watchdog.begin();
#endif

#if defined(MESH_DEBUG) && defined(NRF52_PLATFORM)
  // give some extra time for serial to settle so
  // boot debug messages can be seen on terminal
  delay(5000);
#endif

#ifdef DISPLAY_CLASS
  if (display.begin()) {
    display.startFrame();
    display.setCursor(0, 0);
    display.print("Please wait...");
    display.endFrame();
  }
#endif

  if (!radio_init()) {
    MESH_DEBUG_PRINTLN("Radio init failed!");
    halt();
  }

  fast_rng.begin(radio_driver.getRngSeed());

  FILESYSTEM* fs;
#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  InternalFS.begin();
  fs = &InternalFS;
  IdentityStore store(InternalFS, "");
#elif defined(ESP32)
  SPIFFS.begin(true);
  fs = &SPIFFS;
  IdentityStore store(SPIFFS, "/identity");
#elif defined(RP2040_PLATFORM)
  LittleFS.begin();
  fs = &LittleFS;
  IdentityStore store(LittleFS, "/identity");
  store.begin();
#else
  #error "need to define filesystem"
#endif
  if (!store.load("_main", the_mesh.self_id)) {
    MESH_DEBUG_PRINTLN("Generating new keypair");
    the_mesh.self_id = radio_new_identity();   // create new random identity
    int count = 0;
    while (count < 10 && (the_mesh.self_id.pub_key[0] == 0x00 || the_mesh.self_id.pub_key[0] == 0xFF)) {  // reserved id hashes
      the_mesh.self_id = radio_new_identity(); count++;
    }
    store.save("_main", the_mesh.self_id);
  }

  Serial.print("Repeater ID: ");
  mesh::Utils::printHex(Serial, the_mesh.self_id.pub_key, PUB_KEY_SIZE); Serial.println();

  command[0] = 0;
#ifdef ETHERNET_ENABLED
  ethernet_command[0] = 0;
#endif

  sensors.begin();

  the_mesh.begin(fs);

#ifdef DISPLAY_CLASS
  ui_task.begin(the_mesh.getNodePrefs(), FIRMWARE_BUILD_DATE, FIRMWARE_VERSION);
#endif

#ifdef ETHERNET_ENABLED
  ethernet_start_task();
#endif

  // send out initial zero hop Advertisement to the mesh
#if ENABLE_ADVERT_ON_BOOT == 1
  the_mesh.sendSelfAdvertisement(16000, false);
#endif

  board.onBootComplete();

#ifdef AUTO_REBOOT_MS
  startup_millis = millis();
#endif

#if defined(ESP32) && defined(WITH_ESP32_POWER_SAVING)
  repeater_pm_setup();
#endif
}

void loop() {
#if defined(ESP32) && defined(WITH_ESP32_POWER_SAVING)
  // Note CLI bytes as soon as they arrive — before the read loop below consumes
  // them — so the keep-awake window is armed by the received bytes themselves.
  if (Serial.available()) {
    repeater_pm_serial_active = true;
    repeater_pm_serial_until = millis() + REPEATER_PM_SERIAL_HOLD_MS;
  }
#endif

  // Handle Serial CLI
  int len = strlen(command);
  while (Serial.available() && len < sizeof(command)-1) {
    char c = Serial.read();
    if (c != '\n') {
      command[len++] = c;
      command[len] = 0;
      Serial.print(c);
    }
    if (c == '\r') break;
  }
  if (len == sizeof(command)-1) {  // command buffer full
    command[sizeof(command)-1] = '\r';
  }

  if (len > 0 && command[len - 1] == '\r') {  // received complete line
    Serial.print('\n');
    command[len - 1] = 0;  // replace newline with C string null terminator
    char reply[160];
    reply[0] = 0;
#ifdef ETHERNET_ENABLED
    if (!ethernet_handle_command(command, reply)) {
      the_mesh.handleCommand(0, command, reply);
    }
#else
    the_mesh.handleCommand(0, command, reply);  // NOTE: there is no sender_timestamp via serial!
#endif
    if (reply[0]) {
      Serial.print("  -> "); Serial.println(reply);
    }

    command[0] = 0;  // reset command buffer
  }

#ifdef ETHERNET_ENABLED
  ethernet_loop_maintain();
  if (ethernet_read_line(ethernet_command, sizeof(ethernet_command))) {
    char reply[160];
    reply[0] = 0;
    if (!ethernet_handle_command(ethernet_command, reply)) {
      the_mesh.handleCommand(0, ethernet_command, reply);
    }
    ethernet_send_reply(reply);
    ethernet_command[0] = 0;
  }
#endif

#if defined(PIN_USER_BTN) && defined(_SEEED_SENSECAP_SOLAR_H_) && !defined(DISPLAY_CLASS)
  // Hold the user button to power off the SenseCAP Solar repeater.
  int btnState = digitalRead(PIN_USER_BTN);
  if (btnState == LOW) {
    if (userBtnDownAt == 0) {
      userBtnDownAt = millis();
    } else if ((unsigned long)(millis() - userBtnDownAt) >= USER_BTN_HOLD_OFF_MILLIS) {
      Serial.println("Powering off...");
      board.powerOff();  // does not return
    }
  } else {
    userBtnDownAt = 0;
  }
#endif

  the_mesh.loop();
#ifdef AUTO_REBOOT_MS
  // Optional scheduled reboot, for unattended repeaters that should not be
  // left running indefinitely between site visits.
  if ((unsigned long)(millis() - startup_millis) >= (unsigned long)AUTO_REBOOT_MS) {
    Serial.println("Scheduled reboot: uptime limit reached");
    Serial.flush();
    board.reboot();
  }
#endif
  sensors.loop();
#ifdef DISPLAY_CLASS
  ui_task.loop();
#endif
  rtc_clock.tick();

#ifdef HAS_EXTERNAL_WATCHDOG
  external_watchdog.loop();
#endif

#if defined(ESP32) && defined(WITH_ESP32_POWER_SAVING)
  repeater_pm_service();
  radio_driver.recoverMissedDioInterrupt();
#endif

  if (the_mesh.getNodePrefs()->powersaving_enabled && !the_mesh.hasPendingWork()) {
#if defined(NRF52_PLATFORM)
    board.sleep(0); // nrf ignores seconds param, sleeps whenever possible
#elif defined(ESP32) && defined(WITH_ESP32_POWER_SAVING)
    // Automatic light sleep: the yield below lets the idle task sleep the CPU.
    // The old manual board.sleep(30) path is deliberately not used here.
#else
    if (the_mesh.millisHasNowPassed(POWERSAVING_FIRSTSLEEP_SECS * 1000)) { // To check if it is time to sleep
      board.sleep(30); // Sleep. Wake up after a while or when receiving a LoRa packet
    }
#endif
  }

#if defined(ESP32) && defined(WITH_ESP32_POWER_SAVING)
  // Yield to the FreeRTOS idle task when nothing is due right now: this is what
  // actually hands the CPU over to light sleep (or DFS down to 40 MHz). hasDueWork
  // ignores packets merely parked for a delayed retransmit, so the repeater can
  // sleep through the retransmit delay instead of spinning for it.
  if (the_mesh.getNodePrefs()->powersaving_enabled && !the_mesh.hasDueWork()) {
    vTaskDelay(pdMS_TO_TICKS(REPEATER_PM_YIELD_MS));
  }
#endif
}
