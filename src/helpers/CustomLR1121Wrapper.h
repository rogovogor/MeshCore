#pragma once

#include "CustomLR1121.h"
#include "RadioLibWrappers.h"

class CustomLR1121Wrapper : public RadioLibWrapper {
public:
  CustomLR1121Wrapper(CustomLR1121& radio, mesh::MainBoard& board) : RadioLibWrapper(radio, board) { }
  bool isReceiving() override {
    if (((CustomLR1121 *)_radio)->isReceiving()) return true;

    idle();  // put lr1121 into standby
    // do some basic CAD (blocks for ~12780 micros (on SF 10)!)
    bool activity = (((CustomLR1121 *)_radio)->scanChannel() == RADIOLIB_LORA_DETECTED);
    if (activity) {
      startRecv();
    } else {
      idle();
    }
    return activity;
  }

  void onSendFinished() override {
    RadioLibWrapper::onSendFinished();
    _radio->setPreambleLength(8); // overcomes weird issues with small and big pkts
  }

  float getLastRSSI() const override { return ((CustomLR1121 *)_radio)->getRSSI(); }
  float getLastSNR() const override { return ((CustomLR1121 *)_radio)->getSNR(); }
};
