#pragma once

#include <RadioLib.h>

#define LR1110_IRQ_HAS_PREAMBLE                     0b0000000100  //  4     4     valid LoRa header received
#define LR1110_IRQ_HEADER_VALID                     0b0000010000  //  4     4     valid LoRa header received

class CustomLR1110 : public LR1110 {
  public:
    CustomLR1110(Module *mod) : LR1110(mod) { }

    RadioLibTime_t getTimeOnAir(size_t len) override {
      uint32_t symbolLength_us = ((uint32_t)(1000 * 10) << this->spreadingFactor) / (this->bandwidthKhz * 10) ;
      uint8_t sfCoeff1_x4 = 17; // (4.25 * 4)
      uint8_t sfCoeff2 = 8;
      if(this->spreadingFactor == 5 || this->spreadingFactor == 6) {
        sfCoeff1_x4 = 25; // 6.25 * 4
        sfCoeff2 = 0;
      }
      uint8_t sfDivisor = 4*this->spreadingFactor;
      if(symbolLength_us >= 16000) {
        sfDivisor = 4*(this->spreadingFactor - 2);
      }
      const int8_t bitsPerCrc = 16;
      const int8_t N_symbol_header = this->headerType == RADIOLIB_SX126X_LORA_HEADER_EXPLICIT ? 20 : 0;

      // numerator of equation in section 6.1.4 of SX1268 datasheet v1.1 (might not actually be bitcount, but it has len * 8)
      int16_t bitCount = (int16_t) 8 * len + this->crcTypeLoRa * bitsPerCrc - 4 * this->spreadingFactor  + sfCoeff2 + N_symbol_header;
      if(bitCount < 0) {
        bitCount = 0;
      }
      // add (sfDivisor) - 1 to the numerator to give integer CEIL(...)
      uint16_t nPreCodedSymbols = (bitCount + (sfDivisor - 1)) / (sfDivisor);

      // preamble can be 65k, therefore nSymbol_x4 needs to be 32 bit
      uint32_t nSymbol_x4 = (this->preambleLengthLoRa + 8) * 4 + sfCoeff1_x4 + nPreCodedSymbols * (this->codingRate + 4) * 4;

      return((symbolLength_us * nSymbol_x4) / 4);
    }

    bool isReceiving() {
      uint16_t irq = getIrqStatus();
      bool detected = ((irq & LR1110_IRQ_HEADER_VALID) || (irq & LR1110_IRQ_HAS_PREAMBLE));
      return detected;
    }
};