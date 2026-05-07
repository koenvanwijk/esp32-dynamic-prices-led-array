# ESP32 Dynamic Electricity Prices LED Matrix

Starter project for visualising dynamic electricity prices on a **10×10 WS2812/NeoPixel LED matrix** with an ESP32.

The project runs in the **Wokwi ESP32 simulator** and fetches live Dutch hourly electricity prices from the public EnergyZero API. If WiFi/API/NTP fails, it falls back to built-in demo prices so the display still shows something useful.

- 10 columns = the next 10 hours
- bar height = relative electricity price
- colour = price band
- white dot at top of first column = current hour
- serial monitor prints the current price and usage advice

## Simulator

Open/import this GitHub project in Wokwi:

<https://wokwi.com/projects/new/esp32?template=https://github.com/koenvanwijk/esp32-dynamic-prices-led-array>

If that template import does not auto-load in your browser, open <https://wokwi.com/projects/new/esp32> and copy these files in manually:

- `sketch.ino`
- `diagram.json`
- `libraries.txt`

## Live prices

The sketch uses:

```text
https://api.energyzero.nl/v1/energyprices
```

with parameters:

- current local day, Europe/Amsterdam timezone
- hourly interval
- electricity usage prices
- `inclBtw=true`

Wokwi internet uses:

```cpp
#define WIFI_SSID "Wokwi-GUEST"
#define WIFI_PASSWORD ""
```

For a real ESP32, replace those with your own WiFi credentials or move them into a separate non-committed secrets header.

## Colour legend

| Price | Meaning | Colour |
|---:|---|---|
| `< €0.00/kWh` | negative price | blue |
| `< €0.10/kWh` | very cheap | green |
| `< €0.20/kWh` | cheap | yellow-green |
| `< €0.30/kWh` | normal | yellow/orange |
| `< €0.40/kWh` | expensive | orange |
| `>= €0.40/kWh` | very expensive | red |

## Hardware wiring

| ESP32 | 10×10 NeoPixel matrix |
|---|---|
| GPIO 5 | DIN |
| 3V3/VIN* | VCC |
| GND | GND |

\*For real hardware, power 100 LEDs from a separate 5V supply sized for the brightness you use, and connect grounds together. Do not power a full-brightness 100-LED matrix from the ESP32 3V3 pin.

## Next steps

- Add a setup screen/button to switch display modes.
- Show the cheapest 3-hour block for dishwasher/EV/battery charging.
- Add Home Assistant/MQTT integration for local energy automation.
- Add supplier-specific margin/energy-tax corrections if desired.
