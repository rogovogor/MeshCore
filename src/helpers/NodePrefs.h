#pragma once
#include <stdint.h>
#include <string.h>
#include <helpers/ConfigSerializer.h>

#define ADVERT_LOC_NONE       0
#define ADVERT_LOC_SHARE      1
#define ADVERT_LOC_PREFS      2

#define LOOP_DETECT_OFF       0
#define LOOP_DETECT_MINIMAL   1
#define LOOP_DETECT_MODERATE  2
#define LOOP_DETECT_STRICT    3

#define TELEM_MODE_DENY         0
#define TELEM_MODE_ALLOW_FLAGS  1
#define TELEM_MODE_ALLOW_ALL    2

// One prefs object for every role (companion, repeater, room server, sensor) —
// upstream keeps two near-identical copies, we keep one. Persisted as JSON via
// ConfigSerializer (see DataStore::savePrefs / CommonCLI::savePrefs); the old
// binary layout is still *read* once, to migrate, by DataStore::loadPrefsInt().
//
// Serializer constraints, worth knowing before adding a field:
//   * keys are [A-Za-z_] only, max 15 chars — no digits (ConfigSerializer's
//     is_key_char), so `max_log`, never `log_64`;
//   * any single value, including a hex blob, must stay under 127 chars;
//   * group names mirror upstream's (radio/gps/repeat/room/power/bridge/comp)
//     so their future changes to those groups merge cleanly. Ours live in the
//     `ui` and `south` groups, which upstream will never touch.
class NodePrefs : public ConfigSerializer {
public:
  // === Common fields ===
  float    airtime_factor = 0;
  char     node_name[32];
  float    freq = 0;
  float    bw = 0;
  uint8_t  sf = 0;
  uint8_t  cr = 0;
  int8_t   tx_power_dbm = 0;
  uint8_t  multi_acks = 0;
  float    rx_delay_base = 0;
  uint8_t  rx_boosted_gain = 0;
  uint8_t  radio_fem_rxgain = 0;  // FEM LNA (boards that can switch it)
  uint8_t  cad_enabled = 0;       // hardware Channel Activity Detection before TX
  uint8_t  path_hash_mode = 0;
  uint8_t  extra_sf[4];           // LR2021 side-detector SFs ('set extra.sf'); not persisted upstream either
  uint8_t  gps_enabled = 0;
  uint32_t gps_interval = 0;
  uint8_t  advert_loc_policy = 0;

  // === Repeater / CommonCLI fields ===
  double   node_lat = 0, node_lon = 0;
  char     password[16];
  uint8_t  disable_fwd = 0;
  uint8_t  advert_interval = 0;       // minutes / 2
  uint8_t  flood_advert_interval = 0; // hours
  float    tx_delay_factor = 0;
  char     guest_password[16];
  float    direct_tx_delay_factor = 0;
  uint32_t guard = 0;
  uint8_t  allow_read_only = 0;
  uint8_t  flood_max = 0;
  uint8_t  flood_max_unscoped = 0;
  uint8_t  flood_max_advert = 0;
  uint8_t  interference_threshold = 0;
  uint8_t  agc_reset_interval = 0;    // secs / 4
  uint8_t  bridge_enabled = 0;
  uint16_t bridge_delay = 0;          // milliseconds
  uint8_t  bridge_pkt_src = 0;        // 0=logTx, 1=logRx
  uint32_t bridge_baud = 0;
  uint8_t  bridge_channel = 0;        // 1-14 (ESP-NOW only)
  char     bridge_secret[16];
  uint8_t  powersaving_enabled = 0;
  uint32_t discovery_mod_timestamp = 0;
  float    adc_multiplier = 0;
  char     owner_info[120];
  uint8_t  loop_detect = 0;

  // === Companion-only fields ===
  uint32_t ble_pin = 0;
  uint8_t  telemetry_mode_base = 0;
  uint8_t  telemetry_mode_loc = 0;
  uint8_t  telemetry_mode_env = 0;
  uint8_t  manual_add_contacts = 0;
  uint8_t  buzzer_quiet = 0;
  uint8_t  vibe_quiet = 0;
  uint8_t  autoadd_config = 0;
  uint8_t  client_repeat = 0;   // DEPRECATED, legacy binary only -> use isRepeatEn()
  uint8_t  autoadd_max_hops = 0;
  char     default_scope_name[31];
  uint8_t  default_scope_key[16];
  uint8_t  cyr2lat_channels = 0;
  uint8_t  cyr2lat_contacts = 0;
  uint8_t  ui_pm_clock_mode = 0;   // message popup mode: 0=Off, 1=PM/room, 2=All
  uint8_t  ui_clock_dim_mode = 0;
  uint8_t  ui_display_rotation = 0; // 0-3 (GxEPD2 rotation, 3=default landscape)
  uint8_t  ui_max_unread_idx = 0;  // 0=16, 1=32, 2=64 messages in unread buffer
  uint8_t  ui_max_log_idx = 0;     // 0=16, 1=32, 2=64 messages in history log
  uint32_t ui_charge_uptime_base = 0; // accumulated seconds across soft-reboots

private:
  class RadioPrefs : public ConfigSerializer {
    NodePrefs* _parent;
  protected:
    void structure() override {
      def("freq", _parent->freq);
      def("bw", _parent->bw);
      def("sf", _parent->sf);
      def("cr", _parent->cr);
      def("cad", _parent->cad_enabled);
      def("int_thr", _parent->interference_threshold);
      def("rxgain", _parent->rx_boosted_gain);
      // upstream maps this back onto rx_boosted_gain, which leaves the FEM
      // setting unpersisted; the separate field is what CommonCLI actually uses.
      def("fem_rxgain", _parent->radio_fem_rxgain);
      def("tx", _parent->tx_power_dbm);
      def("af", _parent->airtime_factor);
      def("rxdelay", _parent->rx_delay_base);
      def("f_txdelay", _parent->tx_delay_factor);
      def("d_txdelay", _parent->direct_tx_delay_factor);
      def("agc_int", _parent->agc_reset_interval);
      def("hash_mode", _parent->path_hash_mode);
      def("multi_ack", _parent->multi_acks);
    }
  public:
    RadioPrefs(NodePrefs* parent) : _parent(parent) { }
  };
  RadioPrefs radio;

  class BridgePrefs : public ConfigSerializer {
    NodePrefs* _parent;
  protected:
    void structure() override {
      def("en", _parent->bridge_enabled);
      def("delay", _parent->bridge_delay);
      def("src", _parent->bridge_pkt_src);
      def("baud", _parent->bridge_baud);
      def("ch", _parent->bridge_channel);
      def("secret", _parent->bridge_secret, sizeof(_parent->bridge_secret));
    }
  public:
    BridgePrefs(NodePrefs* parent) : _parent(parent) { }
  };
  BridgePrefs bridge;

  class GPSPrefs : public ConfigSerializer {
    NodePrefs* _parent;
  protected:
    void structure() override {
      def("en", _parent->gps_enabled);
      def("int", _parent->gps_interval);
      def("adv_loc", _parent->advert_loc_policy);
    }
  public:
    GPSPrefs(NodePrefs* parent) : _parent(parent) { }
  };
  GPSPrefs gps;

  class PowerPrefs : public ConfigSerializer {
    NodePrefs* _parent;
  protected:
    void structure() override {
      def("adc_mult", _parent->adc_multiplier);
      def("pwr_sav_en", _parent->powersaving_enabled);
    }
  public:
    PowerPrefs(NodePrefs* parent) : _parent(parent) { }
  };
  PowerPrefs power;

  class RepeatPrefs : public ConfigSerializer {
    NodePrefs* _parent;
  protected:
    void structure() override {
      def("disable", _parent->disable_fwd);
      def("f_max", _parent->flood_max);
      def("f_max_uns", _parent->flood_max_unscoped);
      def("f_max_adv", _parent->flood_max_advert);
      def("loop", _parent->loop_detect);
    }
  public:
    RepeatPrefs(NodePrefs* parent) : _parent(parent) { }
  };
  RepeatPrefs repeat;

  class RoomPrefs : public ConfigSerializer {
    NodePrefs* _parent;
  protected:
    void structure() override {
      def("rd_only", _parent->allow_read_only);
    }
  public:
    RoomPrefs(NodePrefs* parent) : _parent(parent) { }
  };
  RoomPrefs room;

  class CompanionPrefs : public ConfigSerializer {
    NodePrefs* _parent;
  protected:
    void structure() override {
      def("auto_max", _parent->autoadd_max_hops);
      def("defs_nm", _parent->default_scope_name, sizeof(_parent->default_scope_name));
      def("defs_key", (void *) _parent->default_scope_key, sizeof(_parent->default_scope_key));
      def("pin", _parent->ble_pin);
      def("buzz_q", _parent->buzzer_quiet);
      def("vibe_q", _parent->vibe_quiet);
      def("auto_add", _parent->autoadd_config);
      def("man_add", _parent->manual_add_contacts);
      def("tel_base", _parent->telemetry_mode_base);
      def("tel_loc", _parent->telemetry_mode_loc);
      def("tel_env", _parent->telemetry_mode_env);
    }
  public:
    CompanionPrefs(NodePrefs* parent) : _parent(parent) { }
  };
  CompanionPrefs companion;

  // UI settings of this fork. Upstream has no equivalent, so this whole group
  // is ours and will never collide on merge.
  class UIPrefs : public ConfigSerializer {
    NodePrefs* _parent;
  protected:
    void structure() override {
      def("rot", _parent->ui_display_rotation);
      def("pm_mode", _parent->ui_pm_clock_mode);
      def("dim_mode", _parent->ui_clock_dim_mode);
      def("max_unread", _parent->ui_max_unread_idx);
      def("max_log", _parent->ui_max_log_idx);
      def("chg_uptime", _parent->ui_charge_uptime_base);
    }
  public:
    UIPrefs(NodePrefs* parent) : _parent(parent) { }
  };
  UIPrefs ui;

  // Non-UI fork settings. Also ours alone.
  class SouthPrefs : public ConfigSerializer {
    NodePrefs* _parent;
  protected:
    void structure() override {
      def("cyr_ch", _parent->cyr2lat_channels);
      def("cyr_ct", _parent->cyr2lat_contacts);
      // upstream reads this from the legacy binary but never writes it back
      def("disc_mod", _parent->discovery_mod_timestamp);
    }
  public:
    SouthPrefs(NodePrefs* parent) : _parent(parent) { }
  };
  SouthPrefs south;

protected:
  void structure() override {
    def("name", node_name, sizeof(node_name));
    def("pass", password, sizeof(password));
    def("guest", guest_password, sizeof(guest_password));
    def("owner", owner_info, sizeof(owner_info));
    def("adv_int", advert_interval);
    def("f_adv_int", flood_advert_interval);
    def("lat", node_lat);
    def("lon", node_lon);
    def("radio", radio);
    def("bridge", bridge);
    def("gps", gps);
    def("repeat", repeat);
    def("room", room);
    def("power", power);
    def("comp", companion);
    def("ui", ui);
    def("south", south);
  }

public:
  NodePrefs() : ConfigSerializer(),
      radio(this), bridge(this), gps(this), power(this), repeat(this),
      room(this), companion(this), ui(this), south(this) {
    node_name[0] = 0;
    password[0] = 0;
    guest_password[0] = 0;
    bridge_secret[0] = 0;
    owner_info[0] = 0;
    default_scope_name[0] = 0;
    memset(default_scope_key, 0, sizeof(default_scope_key));
    memset(extra_sf, 0, sizeof(extra_sf));
  }

  // Companion "act as repeater" toggle. Upstream renamed the old client_repeat
  // field into repeat.disable_fwd (inverted); we already had disable_fwd for the
  // repeater role, so both roles share it and client_repeat survives only as the
  // legacy binary field the migration reads.
  bool isRepeatEn() const { return disable_fwd == 0; }
  void setRepeatEn(bool en) { disable_fwd = en ? 0 : 1; }
};
