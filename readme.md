<div align="center">

# 📬 Smart Zigbee Mailbox Sensor

<p align="center">
  <img width="299" height="304" alt="image" src="https://github.com/user-attachments/assets/cfab8511-e6a7-44d8-bd4c-d6fbcd6e8c2c" style="border-radius: 8px;" />
</p>

*A discrete, reliable IoT monitoring system for your mailbox powered by an ESP32-H2, wired magnetic reed sensors, and native Zigbee2MQTT integration.*

---

![Version](https://img.shields.io/badge/version-1.0.0-blue.svg)
![Author](https://img.shields.io/badge/author-Adrien%20Brune-orange.svg)
![Language](https://img.shields.io/badge/language-C%2F++-yellow.svg)
![Hardware](https://img.shields.io/badge/hardware-ESP32H2-red.svg)
![Protocol](https://img.shields.io/badge/protocol-Zigbee-blueviolet.svg)
![Platform](https://img.shields.io/badge/platform-Home%20Assistant%20%7C%20Z2M-lightgrey.svg)
![Status](https://img.shields.io/badge/status-active-success.svg)


</div>

## 💡 Introduction

Never miss a delivery again! The **Smart Zigbee Mailbox Sensor** is designed to monitor both the main door and the letter flap of your mailbox in real-time. 

Because traditional metal mailboxes act as a Faraday cage that heavily blocks wireless signals, this project features a smart architectural workaround: the core electronics (powered by an **ESP32-H2** with native Zigbee support) are housed in a custom 3D-printed enclosure mounted **externally** outside the metal box. Only the discreet wired reed sensors are routed inside to monitor the openings, ensuring optimal wireless range and zero packet loss while integrating seamlessly into **Home Assistant via Zigbee2MQTT (Z2M)**.

---

## ✨ Key Features

* **Dual-State Detection:** Independently monitors both the main mailbox door and the letter slot flap via dedicated wired magnetic reed switches.
* **Faraday Cage Bypass:** Custom 3D-printed exterior housing places the ESP32-H2 outside the metal structure for maximum Zigbee range and reliability.
* **Native Zigbee2MQTT (Z2M) Support:** Fully compatible with Z2M and Home Assistant out of the box using custom external converter definitions.
* **Low Power & High Responsiveness:** Instantaneous state updates triggered by hardware-level digital interrupts on the ESP32-H2.
* **Clean Weatherproof Design:** Discreet external casing designed to blend in cleanly while protecting the microcontroller.

---

## 🛠️ Technical Stack & Hardware

* **Microcontroller:** ESP32-H2 (RISC-V SoC with native IEEE 802.15.4 / Zigbee)
* **Firmware Language:** C++
* **Communication Protocol:** Zigbee (Zigbee2MQTT compatible)
* **Sensors:** 2x Magnetic Reed Switches (Door + Flap)
* **Smart Home Platform:** Home Assistant via Zigbee2MQTT
