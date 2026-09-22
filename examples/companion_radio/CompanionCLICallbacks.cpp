#include "CompanionCLICallbacks.h"
#include "MyMesh.h"
#include "DataStore.h"
#include <target.h>

void CompanionCLICallbacks::savePrefs() {
  _mesh.deferSavePrefs();
}

const char* CompanionCLICallbacks::getFirmwareVer() {
  return FIRMWARE_VERSION;
}

const char* CompanionCLICallbacks::getBuildDate() {
  return FIRMWARE_BUILD_DATE;
}

const char* CompanionCLICallbacks::getRole() {
  return "companion";
}

bool CompanionCLICallbacks::formatFileSystem() {
  return _store.formatFileSystem();
}

void CompanionCLICallbacks::sendSelfAdvertisement(int delay_millis, bool flood) {
  _mesh.advert();
}

void CompanionCLICallbacks::setTxPower(int8_t power_dbm) {
  radio_driver.setTxPower(power_dbm);
}

void CompanionCLICallbacks::formatNeighborsReply(char* reply) {
  strcpy(reply, "(not supported)");
}

void CompanionCLICallbacks::formatStatsReply(char* reply) {
  sprintf(reply, "{\"battery_mv\":%u,\"uptime_secs\":%lu}",
    board.getBattMilliVolts(), (unsigned long)(millis() / 1000));
}

void CompanionCLICallbacks::formatRadioStatsReply(char* reply) {
  sprintf(reply,
    "{\"noise_floor\":%d,\"last_rssi\":%d,\"last_snr\":%.2f,\"tx_air_secs\":%u,\"rx_air_secs\":%u}",
    (int16_t)radio_driver.getNoiseFloor(),
    (int16_t)radio_driver.getLastRSSI(),
    radio_driver.getLastSNR(),
    (uint32_t)(_mesh.getTotalAirTime() / 1000),
    (uint32_t)(_mesh.getReceiveAirTime() / 1000));
}

void CompanionCLICallbacks::formatPacketStatsReply(char* reply) {
  sprintf(reply,
    "{\"recv\":%u,\"sent\":%u,\"flood_tx\":%u,\"direct_tx\":%u,\"flood_rx\":%u,\"direct_rx\":%u,\"recv_errors\":%u}",
    radio_driver.getPacketsRecv(),
    radio_driver.getPacketsSent(),
    _mesh.getNumSentFlood(),
    _mesh.getNumSentDirect(),
    _mesh.getNumRecvFlood(),
    _mesh.getNumRecvDirect(),
    radio_driver.getPacketsRecvErrors());
}

mesh::LocalIdentity& CompanionCLICallbacks::getSelfId() {
  return _mesh.self_id;
}

void CompanionCLICallbacks::saveIdentity(const mesh::LocalIdentity& new_id) {
  _store.saveMainIdentity(new_id);
}

void CompanionCLICallbacks::clearStats() {
  radio_driver.resetStats();
  _mesh.resetStats();
}

void CompanionCLICallbacks::applyTempRadioParams(float freq, float bw, uint8_t sf, uint8_t cr, int timeout_mins) {
  radio_driver.setParams(freq, bw, sf, cr);
}

bool CompanionCLICallbacks::setRxBoostedGain(bool enable) {
  radio_driver.setRxBoostedGainMode(enable ? 1 : 0);
  return true;
}

void CompanionCLICallbacks::formatTimesyncReply(char* reply) {
  _mesh._ts.buildReply(reply, _mesh.getRTCClock());
}
