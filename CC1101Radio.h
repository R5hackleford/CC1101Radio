// CC1101Radio - flexible driver for the TI CC1101 sub-GHz transceiver.
//
// Two layers:
//   * High level: begin(), setFrequency(), setBitrate(), startReceive(), readPacket(), ...
//   * Raw access: readReg()/writeReg()/strobe()/readBurst()/writeBurst() plus the full
//     register map in the CC1101Reg namespace, for anything the helpers don't cover.
//
// Named CC1101Radio (not CC1101) so it can live in the same sketch as RadioLib.
// Supported bands: 300-348, 387-464 and 779-928 MHz (the CC1101 cannot do 2.4 GHz).
#pragma once

#include <Arduino.h>
#include <SPI.h>

namespace CC1101Reg {
// Configuration registers
enum : uint8_t {
  IOCFG2 = 0x00, IOCFG1 = 0x01, IOCFG0 = 0x02, FIFOTHR = 0x03,
  SYNC1 = 0x04, SYNC0 = 0x05, PKTLEN = 0x06, PKTCTRL1 = 0x07, PKTCTRL0 = 0x08,
  ADDR = 0x09, CHANNR = 0x0A, FSCTRL1 = 0x0B, FSCTRL0 = 0x0C,
  FREQ2 = 0x0D, FREQ1 = 0x0E, FREQ0 = 0x0F,
  MDMCFG4 = 0x10, MDMCFG3 = 0x11, MDMCFG2 = 0x12, MDMCFG1 = 0x13, MDMCFG0 = 0x14,
  DEVIATN = 0x15, MCSM2 = 0x16, MCSM1 = 0x17, MCSM0 = 0x18,
  FOCCFG = 0x19, BSCFG = 0x1A, AGCCTRL2 = 0x1B, AGCCTRL1 = 0x1C, AGCCTRL0 = 0x1D,
  WOREVT1 = 0x1E, WOREVT0 = 0x1F, WORCTRL = 0x20,
  FREND1 = 0x21, FREND0 = 0x22, FSCAL3 = 0x23, FSCAL2 = 0x24, FSCAL1 = 0x25, FSCAL0 = 0x26,
  RCCTRL1 = 0x27, RCCTRL0 = 0x28, FSTEST = 0x29, PTEST = 0x2A, AGCTEST = 0x2B,
  TEST2 = 0x2C, TEST1 = 0x2D, TEST0 = 0x2E,
  PATABLE = 0x3E, FIFO = 0x3F
};
// Command strobes
enum : uint8_t {
  SRES = 0x30, SFSTXON = 0x31, SXOFF = 0x32, SCAL = 0x33, SRX = 0x34, STX = 0x35,
  SIDLE = 0x36, SWOR = 0x38, SPWD = 0x39, SFRX = 0x3A, SFTX = 0x3B, SWORRST = 0x3C, SNOP = 0x3D
};
// Status registers (read with the burst bit set)
enum : uint8_t {
  PARTNUM = 0x30, VERSION = 0x31, FREQEST = 0x32, LQI = 0x33, RSSI = 0x34,
  MARCSTATE = 0x35, WORTIME1 = 0x36, WORTIME0 = 0x37, PKTSTATUS = 0x38,
  VCO_VC_DAC = 0x39, TXBYTES = 0x3A, RXBYTES = 0x3B
};
// MARCSTATE values
enum : uint8_t {
  MARC_SLEEP = 0x00, MARC_IDLE = 0x01, MARC_RX = 0x0D,
  MARC_RX_OVERFLOW = 0x11, MARC_TX = 0x13, MARC_TX_UNDERFLOW = 0x16
};
}  // namespace CC1101Reg

class CC1101Radio {
 public:
  enum Modulation : uint8_t {
    MOD_2FSK = 0, MOD_GFSK = 1, MOD_ASK_OOK = 3, MOD_4FSK = 4, MOD_MSK = 7
  };

  // Return codes (negative = error). readPacket() returns the payload length when >= 0.
  static constexpr int16_t ERR_NONE = 0;
  static constexpr int16_t ERR_CHIP_NOT_FOUND = -1;
  static constexpr int16_t ERR_BAD_ARG = -2;
  static constexpr int16_t ERR_TIMEOUT = -3;
  static constexpr int16_t ERR_OVERFLOW = -4;
  static constexpr int16_t ERR_CRC = -5;
  static constexpr int16_t ERR_BUF_SMALL = -6;
  static constexpr int16_t ERR_LENGTH = -7;

  static constexpr uint8_t MAX_PAYLOAD = 61;  // 64-byte FIFO minus length byte and 2 status bytes

  struct PacketInfo {
    float rssi;  // dBm
    uint8_t lqi;
    bool crcOk;
  };

  // misoPin lets the driver wait for the chip-ready signal after CS goes low. Pass the same
  // pin you gave SPI.begin() (-1 skips the wait).
  CC1101Radio(uint8_t csPin, SPIClass& spi = SPI, int8_t misoPin = -1, uint32_t spiHz = 4000000);

  // Call SPI.begin(sck, miso, mosi) first. Resets the chip, loads sane defaults and applies the
  // settings below. Returns ERR_NONE on success.
  int16_t begin(float freqMHz = 433.92f, float bitrateKbps = 4.8f, float devKHz = 47.6f,
                float rxBwKHz = 135.0f, Modulation mod = MOD_GFSK);
  void reset();

  // ---- Configuration (each returns false if the argument is out of range) ----
  // setFrequency() drops the chip to IDLE; call startReceive()/transmit() afterwards.
  bool setFrequency(float mhz);
  bool setBitrate(float kbps);          // 0.6 .. 500 kbps
  bool setDeviation(float kHz);         // ~1.5 .. 381 kHz (FSK modes)
  bool setRxBandwidth(float kHz);       // 58 .. 812 kHz
  void setModulation(Modulation m);
  void setSyncWord(uint8_t hi, uint8_t lo);
  void setChannel(uint8_t channel);
  void setCrc(bool enabled);
  void setWhitening(bool enabled);
  int8_t setOutputPowerDbm(int8_t dbm); // snaps to nearest PA step, returns the dBm chosen
  void setPaRaw(uint8_t paValue);       // raw PATABLE byte

  float frequency() const { return _freqMHz; }
  float bitrate() const { return _bitrateBps / 1000.0f; }

  // ---- State control ----
  void idle();
  void sleep();
  void calibrate();
  bool startReceive();  // IDLE -> flush RX FIFO -> RX
  int16_t transmit(const uint8_t* data, uint8_t len, uint32_t timeoutMs = 0);  // leaves chip in IDLE

  // ---- Receive ----
  float getRSSI();                  // live RSSI in dBm (chip must be in RX)
  int16_t available();              // bytes in RX FIFO, or ERR_OVERFLOW
  // Returns payload length (>0), 0 if no complete packet yet, or a negative error. Restarts RX.
  int16_t readPacket(uint8_t* buf, uint8_t maxLen, PacketInfo* info = nullptr);

  // ---- Info ----
  uint8_t partNumber();
  uint8_t version();
  uint8_t marcState();

  // ---- Raw access ----
  uint8_t readReg(uint8_t reg);
  void writeReg(uint8_t reg, uint8_t value);
  uint8_t readStatus(uint8_t reg);
  uint8_t strobe(uint8_t cmd);
  void readBurst(uint8_t reg, uint8_t* buf, uint8_t len);
  void writeBurst(uint8_t reg, const uint8_t* buf, uint8_t len);

 private:
  void select();
  void deselect();
  uint8_t readStatusStable(uint8_t reg);
  bool waitMarc(uint8_t state, uint32_t timeoutUs);
  void applyPa();
  static float rssiToDbm(uint8_t raw);

  uint8_t _cs;
  int8_t _miso;
  SPIClass* _spi;
  SPISettings _settings;

  float _freqMHz = 433.92f;
  double _bitrateBps = 4800.0;
  Modulation _mod = MOD_GFSK;
  uint8_t _pa = 0x60;
};
