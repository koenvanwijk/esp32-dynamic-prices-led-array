# ESP32 Dynamic Electricity Prices LED Matrix

Starter project for visualising dynamic electricity prices on a **10×10 WS2812/NeoPixel LED matrix** with an ESP32.

The first version runs in the **Wokwi ESP32 simulator** and uses mocked Dutch day-ahead prices, so the display immediately shows something useful:

- 10 columns = the next 10 hours
- bar height = relative electricity price
- colour = price band
- white dot at top of first column = current hour
- serial monitor prints the current price and usage advice

## Colour legend

| Price | Meaning | Colour |
|---:|---|---|
| `< €0.00/kWh` | negative price | blue |
| `< €0.10/kWh` | very cheap | green |
| `< €0.20/kWh` | cheap | yellow-green |
| `< €0.30/kWh` | normal | yellow/orange |
| `< €0.40/kWh` | expensive | orange |
| `>= €0.40/kWh` | very expensive | red |

## Run in Wokwi

1. Open <https://wokwi.com/projects/new/esp32>
2. Replace/add these files from this repo:
   - `sketch.ino`
   - `diagram.json`
   - `libraries.txt`
3. Start the simulation.

If this repo is public on GitHub, Wokwi can also import it directly via:

```text
https://wokwi.com/projects/new/esp32?template=https://github.com/<owner>/<repo>
```

## Hardware wiring

| ESP32 | 10×10 NeoPixel matrix |
|---|---|
| GPIO 5 | DIN |
| 3V3/VIN* | VCC |
| GND | GND |

\*For real hardware, power 100 LEDs from a separate 5V supply sized for the brightness you use, and connect grounds together. Do not power a full-brightness 100-LED matrix from the ESP32 3V3 pin.

## Next steps

- Add WiFi + NTP time sync.
- Fetch dynamic prices from a supplier API, Home Assistant, ENTSO-E, or Nord Pool/EPEX source.
- Replace the mocked `prices[24]` array in `sketch.ino` with live hourly prices.
- Add display modes: cheapest 3-hour block, current tariff category, or battery/EV charging advice.
