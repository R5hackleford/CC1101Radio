#include <Arduino.h>
#include <SPI.h>
#include <CC1101Radio.h>

constexpr int PIN_SCK = 18;
constexpr int PIN_MISO = 19;
constexpr int PIN_MOSI = 23;
constexpr int PIN_CS = 5;

CC1101Radio radio(SPI, PIN_CS, PIN_MISO);

void setup() {
  Serial.begin(115200);
  SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS);

  int16_t rc = radio.begin(433.92f, 4.8f, 47.6f, 135.0f, CC1101Radio::MOD_GFSK);
  if (rc != CC1101Radio::ERR_NONE) {
    Serial.printf("CC1101 init failed: %d\n", rc);
    return;
  }

  Serial.printf("PARTNUM=0x%02X VERSION=0x%02X\n", radio.partNumber(), radio.version());
  radio.startReceive();
}

void loop() {
  uint8_t data[CC1101Radio::MAX_PAYLOAD];
  CC1101Radio::PacketInfo info{};
  int16_t len = radio.readPacket(data, sizeof(data), &info);
  if (len > 0) {
    Serial.printf("RX %d bytes, RSSI %.1f dBm, LQI %u, CRC %s\n",
                  len, info.rssi, info.lqi, info.crcOk ? "OK" : "BAD");
  }
}
