#ifndef CST820_H
#define CST820_H

#include <Arduino.h>
#include <Wire.h>

// CST820 I2C address (you already confirmed 0x15 works)
static constexpr uint8_t CST820_ADDR = 0x15;

class CST820 {
public:
  CST820(uint8_t sda, uint8_t scl, uint8_t rst, uint8_t irq, TwoWire &w = Wire)
  : _sda(sda), _scl(scl), _rst(rst), _irq(irq), _wire(&w) {}

  // Assumes YOU call Wire.begin(SDA,SCL) in setup().
  // This begin() only does reset + optional clock.
  bool begin(uint32_t i2cClockHz = 400000, bool doReset = true) {
    if (doReset) {
      pinMode(_rst, OUTPUT);
      digitalWrite(_rst, LOW);
      delay(10);
      digitalWrite(_rst, HIGH);
      delay(80);
    }

    // IRQ/INT line: often active-low, and often needs pullup.
    pinMode(_irq, INPUT_PULLUP);

    // If Wire is already begun, setClock is safe.
    _wire->setClock(i2cClockHz);

    // Quick probe
    return (readChipID() != 0xFF);
  }

  uint8_t readChipID() {
    uint8_t id = 0xFF;
    if (!readReg(0xA7, &id, 1)) return 0xFF;
    return id;
  }

  // Optional: INT sanity check. While touching, many boards pull INT low.
  // If this never changes when you touch, wiring/pin or mode is suspect.
  int intLevel() const {
    return digitalRead(_irq);
  }

  bool getTouch(uint16_t *x, uint16_t *y, uint8_t *gesture = nullptr) {
    // ---- Method A (COMMON CST8xx layout) ----
    // 0x02: number of touch points
    // 0x03..: XH, XL, YH, YL ...
    uint8_t touches = 0;
    if (readReg(0x02, &touches, 1)) {
      touches &= 0x0F;
      if (touches == 0) return false;

      uint8_t buf[6] = {0};
      // Read starting at 0x03: [XH, XL, YH, YL, ?, ?] (varies)
      if (!readReg(0x03, buf, sizeof(buf))) return false;

      // Decode: top nibble often carries event/flags, low 4 bits are coord MSBs
      uint16_t rx = ((buf[0] & 0x0F) << 8) | buf[1];
      uint16_t ry = ((buf[2] & 0x0F) << 8) | buf[3];

      if (gesture) {
        // Some variants expose gesture elsewhere; keep 0 unless you map it later.
        *gesture = 0;
      }

      *x = rx;
      *y = ry;
      return true;
    }

    // ---- Method B (YOUR ORIGINAL READ 0x00 LEN 7) ----
    // If your board really uses this layout, uncomment this whole block
    // and comment out Method A above.
    /*
    uint8_t buf7[7] = {0};
    if (!readReg(0x00, buf7, 7)) return false;

    uint8_t t = buf7[2] & 0x0F;
    if (t == 0) return false;

    if (gesture) *gesture = buf7[1];

    *x = ((buf7[3] & 0x0F) << 8) | buf7[4];
    *y = ((buf7[5] & 0x0F) << 8) | buf7[6];
    return true;
    */

    return false;
  }

private:
  bool readReg(uint8_t reg, uint8_t *out, uint8_t len) {
    _wire->beginTransmission((uint8_t)CST820_ADDR);
    _wire->write(reg);
    if (_wire->endTransmission(false) != 0) return false;

    // Avoid overload noise by being explicit types
    uint8_t got = _wire->requestFrom((uint8_t)CST820_ADDR, (uint8_t)len);
    if (got != len) return false;

    for (uint8_t i = 0; i < len; i++) out[i] = _wire->read();
    return true;
  }

  uint8_t _sda, _scl, _rst, _irq;
  TwoWire *_wire;
};

#endif