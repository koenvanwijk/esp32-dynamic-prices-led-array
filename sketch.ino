#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <math.h>

// ESP32 + 10x10 WS2812/NeoPixel matrix demo for dynamic electricity prices.
// Wokwi simulation: mocked NL day-ahead prices. Real API integration can later
// fill `prices` via WiFi/HTTP and NTP.

#define LED_PIN 5
#define MATRIX_W 10
#define MATRIX_H 10
#define NUM_LEDS (MATRIX_W * MATRIX_H)
#define BRIGHTNESS 80

Adafruit_NeoPixel pixels(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// Example hourly €/kWh prices for a day. Includes a negative-price dip and
// evening peak so the matrix immediately shows useful variation in simulation.
float prices[24] = {
  0.19, 0.17, 0.15, 0.14, 0.13, 0.16,
  0.24, 0.31, 0.34, 0.27, 0.18, 0.08,
 -0.02,-0.04, 0.01, 0.09, 0.21, 0.39,
  0.46, 0.42, 0.33, 0.28, 0.24, 0.21
};

uint32_t lastFrame = 0;
uint8_t currentHour = 0;

uint16_t xy(uint8_t x, uint8_t y) {
  // Serpentine matrix mapping, origin bottom-left for graph drawing.
  if (y % 2 == 0) {
    return y * MATRIX_W + x;
  }
  return y * MATRIX_W + (MATRIX_W - 1 - x);
}

uint32_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return pixels.Color(r, g, b);
}

uint32_t priceColor(float price, bool highlight) {
  // Price bands: green = cheap/negative, yellow = normal, orange/red = expensive.
  uint8_t r, g, b;
  if (price < 0.00)      { r = 0;   g = 90;  b = 255; } // negative: blue
  else if (price < 0.10) { r = 0;   g = 220; b = 90;  }
  else if (price < 0.20) { r = 120; g = 220; b = 0;   }
  else if (price < 0.30) { r = 255; g = 190; b = 0;   }
  else if (price < 0.40) { r = 255; g = 90;  b = 0;   }
  else                   { r = 255; g = 0;   b = 0;   }

  if (highlight) {
    r = min(255, r + 80);
    g = min(255, g + 80);
    b = min(255, b + 80);
  }
  return rgb(r, g, b);
}

void clearMatrix() {
  for (uint16_t i = 0; i < NUM_LEDS; i++) pixels.setPixelColor(i, 0);
}

void drawPriceBars() {
  clearMatrix();

  float minP = prices[0], maxP = prices[0];
  for (uint8_t i = 1; i < 24; i++) {
    minP = min(minP, prices[i]);
    maxP = max(maxP, prices[i]);
  }
  float span = max(0.01f, maxP - minP);

  // 10 columns show the next 10 hours as vertical price bars.
  for (uint8_t x = 0; x < MATRIX_W; x++) {
    uint8_t hour = (currentHour + x) % 24;
    float normalized = (prices[hour] - minP) / span;
    uint8_t barHeight = 1 + round(normalized * (MATRIX_H - 1));
    bool now = (x == 0);

    for (uint8_t y = 0; y < barHeight; y++) {
      pixels.setPixelColor(xy(x, y), priceColor(prices[hour], now));
    }

    // A dim white dot at the top of the current hour makes "now" obvious.
    if (now) pixels.setPixelColor(xy(x, MATRIX_H - 1), rgb(255, 255, 255));
  }

  pixels.show();
}

void printStatus() {
  float now = prices[currentHour];
  Serial.print("Hour ");
  if (currentHour < 10) Serial.print('0');
  Serial.print(currentHour);
  Serial.print(":00  price = ");
  Serial.print(now, 3);
  Serial.print(" EUR/kWh  advice = ");
  if (now < 0.10) Serial.println("RUN dishwasher / charge battery");
  else if (now > 0.35) Serial.println("avoid heavy loads");
  else Serial.println("normal usage");
}

void setup() {
  Serial.begin(115200);
  delay(100);
  pixels.begin();
  pixels.setBrightness(BRIGHTNESS);
  clearMatrix();
  pixels.show();

  Serial.println("ESP32 Dynamic Electricity Prices LED Matrix");
  Serial.println("10 columns = next 10 hours; bar height/color = price; white top dot = current hour.");
  printStatus();
  drawPriceBars();
}

void loop() {
  // In simulation, advance one hour every 2 seconds so the display visibly changes.
  if (millis() - lastFrame > 2000) {
    lastFrame = millis();
    currentHour = (currentHour + 1) % 24;
    printStatus();
    drawPriceBars();
  }
}
