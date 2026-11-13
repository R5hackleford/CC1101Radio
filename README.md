A CC1101 library dealing with an ESP32 microcontroller for Arduino IDE.



# **CC1101Radio Library User Guide**                                                                                                                         

---

## **Introduction**

The **CC1101Radio** library is a powerful tool for interfacing with the **CC1101** wireless transceiver module. This library abstracts the low-level 
SPI communication, allowing developers to easily configure, transmit, and receive data using the CC1101 chip. It is designed for use with 
microcontroller platforms like Arduino, making it ideal for IoT, sensor networks, and wireless communication projects.

---

## **Getting Started**

### **1. Installation**
- Place the `CC1101Radio.h` and `CC1101Radio.cpp` files in your Arduino libraries folder (typically found at `Arduino/libraries/`).
- Restart the Arduino IDE to ensure the library is recognized.

### **2. Dependencies**
- **Arduino SPI Library**: The library relies on Arduino's built-in SPI communication framework. Ensure it is installed (it is included by default in 
most Arduino installations).

---

## **Library Overview**

### **Main Class: `CC1101Radio`**
The library provides a single class, `CC11/01Radio`, which encapsulates all functionality for interacting with the CC1101 module.

### **Key Features**
- SPI communication (hardware or software)
- Frequency configuration
- Data transmission and reception
- Power-saving modes
- Register customization

---

## **Usage Examples**

### **1. Basic Initialization**
```cpp
#include <CC1101Radio.h>

// Initialize the radio module
CC1101Radio radio;

void setup() {
  Serial.begin(9600);
  
  // Initialize the radio with default SPI pins (CS: 10, SCK: 13, MOSI: 11, MISO: 12)
  if (!radio.begin()) {
    Serial.println("Radio initialization failed!");
    while (1); // Halt if initialization fails
  }
  Serial.println("Radio initialized successfully!");
}
```

### **2. Transmit Data**
```cpp
void loop() {
  // Transmit a string
  radio.transmit("Hello, CC1101!");
  delay(1000); // Wait 1 second before sending again
}
```

### **3. Receive Data**
```cpp
void loop() {
  if (radio.available()) {
    char buffer[32];
    int length = radio.receive(buffer, sizeof(buffer));
    if (length > 0) {
      Serial.print("Received: ");
      Serial.println(buffer);
    }
  }
}
```

---

## **Advanced Features**

### **1. Custom SPI Pins**
Specify custom SPI pins during initialization:
```cpp
CC1101Radio radio(10, 13, 11, 12); // CS, SCK, MOSI, MISO
```

### **2. Set Frequency**
Configure the operating frequency (e.g., 433 MHz):
```cpp
radio.setFrequency(433.92); // Frequency in MHz
```

### **3. Data Rate and Power Settings**
Adjust data rate and output power:
```cpp
radio.setDataRate(250);      // Data rate in kbps
radio.setOutputPower(10);    // Output power in dBm
```

### **4. Power-Saving Modes**
Enable low-power modes for extended battery life:
```cpp
radio.enableLowPower(); // Activates sleep mode when not in use
```

### **5. Register Customization**
Directly access and modify CC1101 registers:
```cpp
radio.writeRegister(0x3E, 0x83); // Write to register 0x3E
uint8_t value = radio.readRegister(0x3E); // Read register 0x3E
```

---

## **Configuration Options**

| Parameter              | Description                              | Default Value |
|------------------------|------------------------------------------|----------------|
| SPI Pins               | CS, SCK, MOSI, MISO                      | 10, 13, 11, 12 |
| Frequency              | Operating frequency in MHz               | 433.92         |
| Data Rate              | Data transmission rate in kbps           | 250            |
| Output Power           | Transmit power in dBm                    | 10             |
| Power Mode             | Low-power mode enabled                   | Disabled       |

---

## **Troubleshooting**

### **Common Issues**
1. **No Data Received**  
   - Ensure both devices are on the same frequency.
   - Check SPI pin connections (CS, SCK, MOSI, MISO).
   - Verify that the receiver is in RX mode.

2. **Initialization Failed**  
   - Confirm the CC1101 module is properly connected.
   - Check the power supply voltage (typically 1.9V–3.6V).
   - Ensure the SPI pins are correctly assigned.

3. **Interference or Dropouts**  
   - Use a lower data rate (e.g., 19.2 kbps).
   - Adjust the output power to match the transmission range.

---

## **Conclusion**

The **CC1101Radio** library simplifies wireless communication with the CC1101 module, enabling developers to focus on application logic rather than 
low-level hardware details. Whether you're building a sensor network, remote control, or IoT device, this library provides the tools needed to 
implement robust wireless communication.

For further assistance, refer to the [CC1101 datasheet](https://www.ti.com/lit/ds/symlink/cc1101.pdf) for detailed register configurations and timing 
diagrams.

Version 1.02
What Changed:

325 kHz boundary: DN022 says “≤ 325 kHz” uses FIFOTHR 0x47, TEST2 0x81 and TEST1 0x35. My earlier code used “< 325”, so exactly 325 kHz got the wrong values. It’s fixed and covered by a test.

OOK AGC: setModulation(MOD_ASK_OOK) now writes AGCCTRL2 = 0x04, AGCCTRL1 = 0x00, AGCCTRL0 = 0x92. That’s the best case in DN022’s Figure 2, measured at 3.8 kBd and 100 kHz.

AGCCTRL2: it’s now written as a whole byte, so the FSK MAX_DVGA_GAIN bits don’t carry over. setOokAgc() takes 0–7 (default 4) and agcCtrl0 defaults to 0x92.

FREND1 in OOK: it’s 0xB6 above about 101 kHz and 0x56 at or below. It follows setRxBandwidth() while you’re in OOK, and the FSK value comes back when you leave OOK.


Limits from DN022


Measure to tune: DN022 says the best AGC settings change with data rate and bandwidth. It also says some combinations are unstable above the sensitivity limit. Check PER every 2 dB across the whole input range, not just at the sensitivity limit.

Update 1.02 Changes:

What Changed:

325 kHz boundary: DN022 says “≤ 325 kHz” uses FIFOTHR 0x47, TEST2 0x81 and TEST1 0x35. My earlier code used “< 325”, so exactly 325 kHz got the wrong values. It’s fixed and covered by a test.

OOK AGC: setModulation(MOD_ASK_OOK) now writes AGCCTRL2 = 0x04, AGCCTRL1 = 0x00, AGCCTRL0 = 0x92. That’s the best case in DN022’s Figure 2, measured at 3.8 kBd and 100 kHz.

AGCCTRL2: it’s now written as a whole byte, so the FSK MAX_DVGA_GAIN bits don’t carry over. setOokAgc() takes 0–7 (default 4) and agcCtrl0 defaults to 0x92.

FREND1 in OOK: it’s 0xB6 above about 101 kHz and 0x56 at or below. It follows setRxBandwidth() while you’re in OOK, and the FSK value comes back when you leave OOK.


Limits from DN022


Measure to tune: DN022 says the best AGC settings change with data rate and bandwidth. It also says some combinations are unstable above the sensitivity limit. Check PER every 2 dB across the whole input range, not just at the sensitivity limit.

Decision boundary: AGCCTRL0 = 0x91 sets an 8 dB boundary and 0x92 sets 12 dB. At 8 dB the “low” bit must be at least 16 dB below the “high” bit.

FSCTRL1 not set: DN022 says the IF frequency should follow the RX bandwidth, using the SmartRF preset at or above yours (58/100/232/325/540/812 kHz). The note only shows 0x08 for the 232 kHz preset, so I didn’t invent a table. Read the value off SmartRF Studio for your bandwidth and set it with writeReg(FSCTRL1, …).

Decision boundary: AGCCTRL0 = 0x91 sets an 8 dB boundary and 0x92 sets 12 dB. At 8 dB the “low” bit must be at least 16 dB below the “high” bit.

FSCTRL1 not set: DN022 says the IF frequency should follow the RX bandwidth, using the SmartRF preset at or above yours (58/100/232/325/540/812 kHz). The note only shows 0x08 for the 232 kHz preset, so I didn’t invent a table. Read the value off SmartRF Studio for your bandwidth and set it with writeReg(FSCTRL1, …).

v1.03

Class structure: BusLock and the unlocked primitives are in the private section, as in your sketch. I added rmwRegUnlocked() as suggested. csLow() now contains the chip-ready wait that used to be in select(), and select()/deselect() are gone.
Public methods: they are thin wrappers (BusLock plus the unlocked version), so the public API is unchanged.
Multi-step sequences under one lock:
reset() holds the lock across the whole manual-reset sequence.
wake() holds it across the CS wake-up and the register and PATABLE restore.
sleep() holds it across the saves and the power-down strobe.
applyPa() holds it across the FREND0 write and the PATABLE burst.
startReceive() holds it across SFRX and SRX.
sendPacket() holds it across the flush, the optional PKTLEN change, the FIFO load, the CRC check and STX, so no other task’s traffic gets between the FIFO load and STX.
readPacket() holds it across the payload and status-byte reads.
Polling waits: these don’t hold the lock. waitMarc(), the TX wait loop and the 1 ms delay after reset take the lock per read, so other radios on the same bus aren’t starved.



Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>
