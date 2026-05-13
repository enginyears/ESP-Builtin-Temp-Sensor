# 🌡️ ESP32 Built-in Temperature Gauge

> A live animated temperature gauge — powered by a sensor already inside your ESP32. No extra components. Ever.

---

## 💡 The Idea

Your ESP32 chip has a temperature sensor baked right into the silicon.  
Most beginners never know it's there.

This firmware reads it every 2 seconds and displays a live animated gauge on a self-hosted webpage. Connect your phone to the ESP32's WiFi, open a browser, and watch the needle move in real time.

---

## 🧰 What You Need

| Item | Cost |
|------|------|
| ESP32 Dev Board | ~$4 |
| USB Cable | Free |
| VS Code + PlatformIO | Free |
| External components | ❌ None |

---

## 🚀 Quick Start

```bash
git clone https://github.com/YOUR_USERNAME/esp32-temp-gauge
# Open in VS Code with PlatformIO
# Click Upload →
```

1. Connect phone to WiFi: `ESP32 TempGauge` / password: `tempwatch`
2. Open browser → `http://192.168.4.1`
3. Watch the needle update live every 2 seconds

---

## 🧠 How It Works

| Part | What it does |
|------|-------------|
| `temperatureRead()` | Built-in ESP32 function — reads the chip's internal sensor |
| WiFi AP mode | ESP32 creates its own WiFi network |
| WebServer | ESP32 hosts the gauge webpage |
| `/temp` JSON route | Sends live temp data to the browser every 2s |
| SVG needle | Browser animates the gauge using the JSON data |

---

## ⚠️ Note on Accuracy

The internal sensor reads the **chip temperature**, not room temperature. It runs a bit warm (~40–60°C is normal). To see it change on camera, gently warm the chip with your fingers — the needle will visibly rise.

---

## 🧠 What You Learn

- Reading built-in ESP32 sensors
- Creating a JSON API endpoint on a microcontroller
- Animating SVG with JavaScript
- Fetch API basics (how browsers request data)

---

## 📺 Part of the `enginyears` Beginner Series
> *One ESP32. Infinite possibilities. Starting from zero.*

Instagram: [@enginyears.me](https://instagram.com/enginyears.me)
