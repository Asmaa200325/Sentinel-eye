# 👁️ Sentinel Eye - Smart Surveillance System

Sentinel Eye is an intelligent security and surveillance system built using Embedded Systems and Internet of Things (IoT) technologies. Developed as a core project for the Embedded Systems course, this system delivers a cost-effective, smart, and real-time solution for live area monitoring.

---

## 🎯 Project Overview (Problem & Solution)
* The Problem: Traditional surveillance systems are often expensive, require complex wiring infrastructure, and fail to provide instant, smart alerts directly to the user when a breach occurs.
* The Solution: A compact, wireless security node built around the ESP32-CAM microcontroller and a smart motion sensor that monitors the environment continuously.
* The Result: Upon detecting any motion, the system captures a high-resolution image and sends an instant notification along with the live media directly to the user's phone via a Telegram Bot within milliseconds.

---

## 🛠️ System Architecture

### 1. Hardware Components
* Microcontroller: ESP32-CAM (The master control unit with an integrated camera and built-in Wi-Fi)
* Sensor: PIR Motion Sensor (Passive Infrared sensor used for precise motion detection)
* Power Source: FTDI Programmer / USB Power Supply (For programming and powering up the board)

### 2. Software & Tools
* Programming Language: C++ (Embedded C)
* IDE: Arduino IDE
* Libraries Used: 
  * WiFi.h & WiFiClientSecure.h (For secure network connectivity)
  * UniversalTelegramBot.h (To interface with the Telegram API and send media)
  * ArduinoJson.h (For parsing incoming payload data)

---

## 🚀 Key Features
* [x] Real-time Motion Detection: High-accuracy motion sensing using the PIR sensor.
* [x] Instant Telegram Alerts: Delivers alerts and captured photos directly to a dedicated Telegram bot with no external apps required.
* [x] Wireless Connectivity: Operates entirely over a local Wi-Fi network.
* [x] Compact & Low Cost: A highly portable and energy-efficient design, offering a budget-friendly alternative to commercial security cameras.

---

## 👥 Team Members
This project was successfully developed and collaborated on by:
* Asmaa Idris
* Juman Alghamdi
* Jori Alsalmi

---

## 📂 Setup & Installation
1. Clone or download this repository and update the main configuration variables (or config.h file) with your network and bot credentials:
   ```cpp
   const char* ssid = "YOUR_WIFI_SSID";
   const char* password = "YOUR_WIFI_PASSWORD";
   const char* botToken = "YOUR_TELEGRAM_BOT_TOKEN";
