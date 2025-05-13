# NetNuke
NetNuke is a wireless pentesting and monitoring gadget based on the [ESP32 SoC](https://www.espressif.com/en/products/socs/esp32).

> ⚠️ **DISCLAIMER**  
> This tool is intended **only for educational and authorized security testing** purposes. Unauthorized use of this software to disrupt or interfere with networks you do not own or have explicit permission to test is illegal and unethical.

---

## 🔧 Features

- 📡 **Wi-Fi Scanner** (TODO) – View nearby access points and clients in real-time.
- 📶 **Deauthentication Attacks** (TODO) – Disconnect clients from Wi-Fi networks.
- 🧲 **Wi-Fi Jamming** (Coding, working partially) – Jam specific or broad Wi-Fi channels (2.4 GHz).
- 🧲 **Bluetooth Jamming** (Coding, working partially) – Jam Bluetooth channels (2.4 GHz).
- 📘 **Bluetooth Scanner** (TODO) – Scan for nearby BLE and Classic Bluetooth devices.
- 📥 **Packet Sniffing** (TODO) – Monitor 802.11 Wi-Fi packets.
- 🔒 **MAC Address Randomization** (TODO) – Evade tracking.


---

## 🛠 Hardware Requirements

- **38pin ESP32 Development Board**
- **NRF24L01 2.4Ghz Transceptor**
- **I2C OLED Display (SSD1306/SH1106)**
- **Buttons / Rotary Encoder**
- Optional:
  - **CC1101 Module** for 433-815Mhz radio features
  - **Lithium 3.7V Battery + Lithium battery charger** for portability
  - **3D Printed Case**

---

## 🚀 Getting Started

### 1. Clone the Repository

```bash
git clone https://github.com/mpascu/NetNuke.git
```


### 2. Install Dependencies

Install Arduino and the following libraries:
 - GEM
 - Adafruit GFX Library
 - Adafruit SH110X or Adafruit SSD1306 depending on the screen you're using
 - RF24
 - KeyDetector
 - U8g2 (Required by GEM)