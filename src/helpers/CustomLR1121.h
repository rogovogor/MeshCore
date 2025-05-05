#pragma once

#include <RadioLib.h>

#define LR1121_IRQ_HAS_PREAMBLE                     0b0000000100  //  4     4     valid LoRa header received
#define LR1121_IRQ_HEADER_VALID                     0b0000010000  //  4     4     valid LoRa header received

class CustomLR1121 : public LR1121 {
  public:
    CustomLR1121(Module *mod) : LR1121(mod) { }

    bool isReceiving() {
      uint16_t irq = getIrqStatus();
      bool hasPreamble = ((irq & LR1121_IRQ_HEADER_VALID) && (irq & LR1121_IRQ_HAS_PREAMBLE));
      return hasPreamble;
    }
};