#pragma once
#include <helpers/CommonCLI.h>

class MyMesh;
class DataStore;

class CompanionCLICallbacks : public CommonCLICallbacks {
  MyMesh&    _mesh;
  DataStore& _store;
public:
  CompanionCLICallbacks(MyMesh& mesh, DataStore& store) : _mesh(mesh), _store(store) {}

  void savePrefs() override;
  const char* getFirmwareVer() override;
  const char* getBuildDate() override;
  const char* getRole() override;
  bool formatFileSystem() override;
  void sendSelfAdvertisement(int delay_millis, bool flood) override;
  void updateAdvertTimer() override {}
  void updateFloodAdvertTimer() override {}
  void setLoggingOn(bool enable) override {}
  void eraseLogFile() override {}
  void dumpLogFile() override {}
  void setTxPower(int8_t power_dbm) override;
  void formatNeighborsReply(char* reply) override;
  void formatStatsReply(char* reply) override;
  void formatRadioStatsReply(char* reply) override;
  void formatPacketStatsReply(char* reply) override;
  mesh::LocalIdentity& getSelfId() override;
  void saveIdentity(const mesh::LocalIdentity& new_id) override;
  void clearStats() override;
  void applyTempRadioParams(float freq, float bw, uint8_t sf, uint8_t cr, int timeout_mins) override;
  bool setRxBoostedGain(bool enable) override;
  void formatTimesyncReply(char* reply) override;
};
