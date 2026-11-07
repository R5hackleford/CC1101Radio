#include "CC1101Radio.h"

#include <math.h>

using namespace CC1101Reg;

namespace {
constexpr double XOSC_HZ = 26000000.0;  // CC1101 modules use a 26 MHz crystal

// SPI header bits
constexpr uint8_t BIT_READ = 0x80;
constexpr uint8_t BIT_BURST = 0x40;

// Output power steps and matching PATABLE bytes (TI datasheet, "optimum PATABLE settings").
constexpr int8_t PA_DBM[8] = {-30, -20, -15, -10, 0, 5, 7, 10};
constexpr uint8_t PA_315[8] = {0x12, 0x0D, 0x1C, 0x34, 0x51, 0x85, 0xCB, 0xC2};
constexpr uint8_t PA_433[8] = {0x12, 0x0E, 0x1D, 0x34, 0x60, 0x84, 0xC8, 0xC0};
constexpr uint8_t PA_868[8] = {0x03, 0x0F, 0x1E, 0x27, 0x50, 0x81, 0xCB, 0xC2};
constexpr uint8_t PA_915[8] = {0x03, 0x0E, 0x1E, 0x27, 0x8E, 0x84, 0xCC, 0xC3};

// Register defaults based on TI SmartRF Studio output (433 MHz, GFSK, 4.8 kbps).
// Frequency, bitrate, deviation, bandwidth, modulation and PA are applied afterwards.
struct RegVal { uint8_t reg; uint8_t val; };
constexpr RegVal DEFAULTS[] = {
    {IOCFG0, 0x06},    // GDO0: asserts on sync word, deasserts at end of packet
    {FIFOTHR, 0x47},
    {SYNC1, 0xD3},     {SYNC0, 0x91},
    {PKTLEN, 0x3D},    // max packet length 61
    {PKTCTRL1, 0x04},  // append RSSI/LQI/CRC-OK status bytes, no address check
    {PKTCTRL0, 0x05},  // variable length, CRC on, no whitening
    {FSCTRL1, 0x06},   {FSCTRL0, 0x00},
    {MDMCFG1, 0x22},   {MDMCFG0, 0xF8},
    {MCSM1, 0x00},     // CCA always, go to IDLE after RX and after TX
    {MCSM0, 0x18},     // auto-calibrate on IDLE -> RX/TX
    {FOCCFG, 0x16},    {BSCFG, 0x6C},
    {AGCCTRL2, 0x43},  {AGCCTRL1, 0x40}, {AGCCTRL0, 0x91},
    {WORCTRL, 0xFB},
    {FREND1, 0x56},
    {FSCAL3, 0xE9},    {FSCAL2, 0x2A},   {FSCAL1, 0x00}, {FSCAL0, 0x1F},
    {FSTEST, 0x59},    {TEST2, 0x81},    {TEST1, 0x35},  {TEST0, 0x09},
};
}  // namespace

CC1101Radio::CC1101Radio(uint8_t csPin, SPIClass& spi, int8_t misoPin, uint32_t spiHz)
    : _cs(csPin), _miso(misoPin), _spi(&spi), _settings(spiHz, MSBFIRST, SPI_MODE0) {}

// ---------------------------------------------------------------- low-level SPI

void CC1101Radio::select() {
  _spi->beginTransaction(_settings);
  digitalWrite(_cs, LOW);
  if (_miso >= 0) {  // wait for the chip's crystal-ready signal (MISO low)
    uint32_t t = micros();
    while (digitalRead(_miso) && (micros() - t) < 2000) {
    }
  }
}

void CC1101Radio::deselect() {
  digitalWrite(_cs, HIGH);
  _spi->endTransaction();
}

uint8_t CC1101Radio::readReg(uint8_t reg) {
  select();
  _spi->transfer(reg | BIT_READ);
  uint8_t v = _spi->transfer(0x00);
  deselect();
  return v;
}

void CC1101Radio::writeReg(uint8_t reg, uint8_t value) {
  select();
  _spi->transfer(reg);
  _spi->transfer(value);
  deselect();
}

// Status registers share addresses with strobes; the burst bit selects the register.
uint8_t CC1101Radio::readStatus(uint8_t reg) {
  select();
  _spi->transfer(reg | BIT_READ | BIT_BURST);
  uint8_t v = _spi->transfer(0x00);
  deselect();
  return v;
}

// RXBYTES/TXBYTES can be read mid-update; the datasheet says to read until two reads agree.
uint8_t CC1101Radio::readStatusStable(uint8_t reg) {
  uint8_t a = readStatus(reg);
  for (uint8_t i = 0; i < 4; i++) {
    uint8_t b = readStatus(reg);
    if (a == b) return b;
    a = b;
  }
  return a;
}

uint8_t CC1101Radio::strobe(uint8_t cmd) {
  select();
  uint8_t status = _spi->transfer(cmd);
  deselect();
  return status;
}

void CC1101Radio::readBurst(uint8_t reg, uint8_t* buf, uint8_t len) {
  select();
  _spi->transfer(reg | BIT_READ | BIT_BURST);
  for (uint8_t i = 0; i < len; i++) buf[i] = _spi->transfer(0x00);
  deselect();
}

void CC1101Radio::writeBurst(uint8_t reg, const uint8_t* buf, uint8_t len) {
  select();
  _spi->transfer(reg | BIT_BURST);
  for (uint8_t i = 0; i < len; i++) _spi->transfer(buf[i]);
  deselect();
}

// ---------------------------------------------------------------- init

void CC1101Radio::reset() {
  // Power-on reset sequence from the datasheet, then SRES.
  digitalWrite(_cs, HIGH);
  delayMicroseconds(5);
  digitalWrite(_cs, LOW);
  delayMicroseconds(10);
  digitalWrite(_cs, HIGH);
  delayMicroseconds(45);
  strobe(SRES);
  delay(2);
}

int16_t CC1101Radio::begin(float freqMHz, float bitrateKbps, float devKHz, float rxBwKHz,
                           Modulation mod) {
  pinMode(_cs, OUTPUT);
  digitalWrite(_cs, HIGH);
  reset();

  uint8_t ver = readStatus(VERSION);
  if (ver == 0x00 || ver == 0xFF) return ERR_CHIP_NOT_FOUND;  // genuine parts report 0x04/0x14

  for (const RegVal& rv : DEFAULTS) writeReg(rv.reg, rv.val);

  _mod = mod;
  setModulation(mod);
  if (!setBitrate(bitrateKbps)) return ERR_BAD_ARG;
  if (mod != MOD_ASK_OOK && !setDeviation(devKHz)) return ERR_BAD_ARG;
  if (!setRxBandwidth(rxBwKHz)) return ERR_BAD_ARG;
  if (!setFrequency(freqMHz)) return ERR_BAD_ARG;
  setOutputPowerDbm(0);
  return ERR_NONE;
}

// ---------------------------------------------------------------- configuration

bool CC1101Radio::setFrequency(float mhz) {
  bool ok = (mhz >= 300.0f && mhz <= 348.0f) || (mhz >= 387.0f && mhz <= 464.0f) ||
            (mhz >= 779.0f && mhz <= 928.0f);
  if (!ok) return false;

  // FREQ = f_carrier * 2^16 / f_xosc  (resolution ~397 Hz)
  uint32_t word = (uint32_t)((double)mhz * 1e6 * 65536.0 / XOSC_HZ + 0.5);
  idle();
  uint8_t b[3] = {(uint8_t)(word >> 16), (uint8_t)(word >> 8), (uint8_t)word};
  writeBurst(FREQ2, b, 3);
  _freqMHz = mhz;
  return true;
}

bool CC1101Radio::setBitrate(float kbps) {
  if (kbps < 0.6f || kbps > 500.0f) return false;
  double r = (double)kbps * 1000.0;
  // R = (256 + M) * 2^E / 2^28 * f_xosc
  int e = (int)floor(log(r * 1048576.0 / XOSC_HZ) / log(2.0));
  if (e < 0) e = 0;
  if (e > 15) e = 15;
  int m = (int)lround(r * 268435456.0 / (XOSC_HZ * (double)(1UL << e))) - 256;
  if (m > 255) {
    m = 0;
    if (e < 15) e++;
  }
  if (m < 0) m = 0;

  writeReg(MDMCFG3, (uint8_t)m);
  writeReg(MDMCFG4, (readReg(MDMCFG4) & 0xF0) | (uint8_t)e);
  _bitrateBps = (256.0 + m) * (double)(1UL << e) / 268435456.0 * XOSC_HZ;
  return true;
}

bool CC1101Radio::setDeviation(float kHz) {
  if (kHz < 1.5f || kHz > 381.0f) return false;
  // f_dev = f_xosc / 2^17 * (8 + M) * 2^E
  double target = (double)kHz * 1000.0;
  double bestErr = 1e18;
  uint8_t bestE = 0, bestM = 0;
  for (uint8_t e = 0; e < 4; e++) {
    for (uint8_t m = 0; m < 8; m++) {
      double dev = XOSC_HZ / 131072.0 * (8 + m) * (double)(1 << e);
      double err = fabs(dev - target);
      if (err < bestErr) {
        bestErr = err;
        bestE = e;
        bestM = m;
      }
    }
  }
  writeReg(DEVIATN, (uint8_t)((bestE << 4) | bestM));
  return true;
}

bool CC1101Radio::setRxBandwidth(float kHz) {
  if (kHz < 58.0f || kHz > 812.0f) return false;
  // BW = f_xosc / (8 * (4 + M) * 2^E)
  double target = (double)kHz * 1000.0;
  double bestErr = 1e18;
  uint8_t bestE = 0, bestM = 0;
  for (uint8_t e = 0; e < 4; e++) {
    for (uint8_t m = 0; m < 4; m++) {
      double bw = XOSC_HZ / (8.0 * (4 + m) * (double)(1 << e));
      double err = fabs(bw - target);
      if (err < bestErr) {
        bestErr = err;
        bestE = e;
        bestM = m;
      }
    }
  }
  writeReg(MDMCFG4, (uint8_t)((readReg(MDMCFG4) & 0x0F) | (bestE << 6) | (bestM << 4)));
  return true;
}

void CC1101Radio::setModulation(Modulation m) {
  _mod = m;
  // MOD_FORMAT in bits 6:4, no Manchester, SYNC_MODE 2 (16/16 sync bits detected)
  writeReg(MDMCFG2, (uint8_t)((m << 4) | 0x02));
  applyPa();
}

void CC1101Radio::setSyncWord(uint8_t hi, uint8_t lo) {
  writeReg(SYNC1, hi);
  writeReg(SYNC0, lo);
}

void CC1101Radio::setChannel(uint8_t channel) { writeReg(CHANNR, channel); }

void CC1101Radio::setCrc(bool enabled) {
  uint8_t v = readReg(PKTCTRL0);
  writeReg(PKTCTRL0, enabled ? (v | 0x04) : (v & ~0x04));
}

void CC1101Radio::setWhitening(bool enabled) {
  uint8_t v = readReg(PKTCTRL0);
  writeReg(PKTCTRL0, enabled ? (v | 0x40) : (v & ~0x40));
}

int8_t CC1101Radio::setOutputPowerDbm(int8_t dbm) {
  const uint8_t* table = PA_433;
  if (_freqMHz < 360.0f) table = PA_315;
  else if (_freqMHz >= 779.0f && _freqMHz < 900.0f) table = PA_868;
  else if (_freqMHz >= 900.0f) table = PA_915;

  uint8_t best = 0;
  int bestDiff = 1000;
  for (uint8_t i = 0; i < 8; i++) {
    int d = abs((int)PA_DBM[i] - (int)dbm);
    if (d < bestDiff) {
      bestDiff = d;
      best = i;
    }
  }
  _pa = table[best];
  applyPa();
  return PA_DBM[best];
}

void CC1101Radio::setPaRaw(uint8_t paValue) {
  _pa = paValue;
  applyPa();
}

// FSK/MSK use PATABLE[0]; ASK/OOK switches between PATABLE[0] (off) and PATABLE[1] (on).
void CC1101Radio::applyPa() {
  uint8_t table[8] = {0};
  if (_mod == MOD_ASK_OOK) {
    table[1] = _pa;
    writeReg(FREND0, 0x11);
  } else {
    table[0] = _pa;
    writeReg(FREND0, 0x10);
  }
  writeBurst(PATABLE, table, 8);
}

// ---------------------------------------------------------------- state control

bool CC1101Radio::waitMarc(uint8_t state, uint32_t timeoutUs) {
  uint32_t t = micros();
  while ((readStatus(MARCSTATE) & 0x1F) != state) {
    if ((micros() - t) > timeoutUs) return false;
  }
  return true;
}

void CC1101Radio::idle() {
  strobe(SIDLE);
  waitMarc(MARC_IDLE, 5000);
}

void CC1101Radio::sleep() {
  idle();
  strobe(SPWD);
}

void CC1101Radio::calibrate() {
  idle();
  strobe(SCAL);
  delay(1);
}

bool CC1101Radio::startReceive() {
  idle();
  strobe(SFRX);
  strobe(SRX);
  return true;
}

int16_t CC1101Radio::transmit(const uint8_t* data, uint8_t len, uint32_t timeoutMs) {
  if (len == 0 || len > MAX_PAYLOAD) return ERR_LENGTH;

  idle();
  strobe(SFTX);

  // TX FIFO burst write: length byte first (variable-length mode), then the payload.
  select();
  _spi->transfer(FIFO | BIT_BURST);
  _spi->transfer(len);
  for (uint8_t i = 0; i < len; i++) _spi->transfer(data[i]);
  deselect();

  strobe(STX);

  if (timeoutMs == 0) {
    // preamble + sync + length + payload + CRC, with 50% margin
    double bits = (len + 12) * 8.0;
    timeoutMs = (uint32_t)(bits * 1000.0 / _bitrateBps * 1.5) + 20;
  }
  uint32_t start = millis();
  while ((readStatus(MARCSTATE) & 0x1F) != MARC_IDLE) {  // MCSM1 sends us to IDLE after TX
    if (millis() - start > timeoutMs) {
      idle();
      strobe(SFTX);
      return ERR_TIMEOUT;
    }
  }
  return ERR_NONE;
}

// ---------------------------------------------------------------- receive

float CC1101Radio::rssiToDbm(uint8_t raw) {
  int16_t v = raw >= 128 ? (int16_t)raw - 256 : (int16_t)raw;
  return v / 2.0f - 74.0f;  // 74 dB offset is typical; it varies slightly with band/data rate
}

float CC1101Radio::getRSSI() { return rssiToDbm(readStatus(RSSI)); }

int16_t CC1101Radio::available() {
  uint8_t rx = readStatusStable(RXBYTES);
  if (rx & 0x80) return ERR_OVERFLOW;
  return rx & 0x7F;
}

int16_t CC1101Radio::readPacket(uint8_t* buf, uint8_t maxLen, PacketInfo* info) {
  int16_t n = available();
  if (n == ERR_OVERFLOW) {
    startReceive();
    return ERR_OVERFLOW;
  }
  if (n == 0) return 0;

  // MCSM1 sends the chip to IDLE once a packet has been fully received, so data in the FIFO
  // while still in RX means a packet is only partly in.
  if ((readStatus(MARCSTATE) & 0x1F) == MARC_RX) return 0;

  uint8_t len = readReg(FIFO);  // single read pops the length byte
  if (len == 0 || len > MAX_PAYLOAD || (int16_t)len + 2 > n - 1) {
    startReceive();
    return ERR_LENGTH;
  }
  if (len > maxLen) {
    startReceive();
    return ERR_BUF_SMALL;
  }

  readBurst(FIFO, buf, len);
  uint8_t status[2];
  readBurst(FIFO, status, 2);  // RSSI, then LQI | CRC_OK<<7 (PKTCTRL1 append-status)
  startReceive();

  bool crcOk = (status[1] & 0x80) != 0;
  if (info) {
    info->rssi = rssiToDbm(status[0]);
    info->lqi = status[1] & 0x7F;
    info->crcOk = crcOk;
  }
  if (!crcOk && (readReg(PKTCTRL0) & 0x04)) return ERR_CRC;
  return len;
}

// ---------------------------------------------------------------- info

uint8_t CC1101Radio::partNumber() { return readStatus(PARTNUM); }
uint8_t CC1101Radio::version() { return readStatus(VERSION); }
uint8_t CC1101Radio::marcState() { return readStatus(MARCSTATE) & 0x1F; }
