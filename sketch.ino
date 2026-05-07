#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <math.h>

// ESP32 + 10x10 WS2812/NeoPixel matrix for dynamic electricity prices.
// Wokwi simulation uses its built-in "Wokwi-GUEST" WiFi network and fetches
// Dutch hourly electricity prices from EnergyZero's public API.

#define LED_PIN 5
#define MATRIX_W 10
#define MATRIX_H 10
#define NUM_LEDS (MATRIX_W * MATRIX_H)
#define BRIGHTNESS 80

#define WIFI_SSID "Wokwi-GUEST"
#define WIFI_PASSWORD ""
#define TZ_INFO "CET-1CEST,M3.5.0,M10.5.0/3" // Europe/Amsterdam

Adafruit_NeoPixel pixels(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// Fallback hourly €/kWh prices. Used until live prices are fetched, or if WiFi/API fails.
float prices[24] = {
  0.19, 0.17, 0.15, 0.14, 0.13, 0.16,
  0.24, 0.31, 0.34, 0.27, 0.18, 0.08,
 -0.02,-0.04, 0.01, 0.09, 0.21, 0.39,
  0.46, 0.42, 0.33, 0.28, 0.24, 0.21
};

bool livePricesLoaded = false;
uint32_t lastFrame = 0;
uint32_t lastFetchAttempt = 0;
uint8_t currentHour = 0;

uint16_t xy(uint8_t x, uint8_t y) {
  // Serpentine matrix mapping, origin bottom-left for graph drawing.
  if (y % 2 == 0) return y * MATRIX_W + x;
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

    // A white dot at the top of the current hour makes "now" obvious.
    if (now) pixels.setPixelColor(xy(x, MATRIX_H - 1), rgb(255, 255, 255));
  }

  pixels.show();
}

void showWifiProgress(uint8_t step) {
  clearMatrix();
  for (uint8_t i = 0; i <= step && i < MATRIX_W; i++) {
    pixels.setPixelColor(xy(i, 0), rgb(0, 0, 80));
  }
  pixels.show();
}

void updateCurrentHourFromClock() {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 100)) {
    currentHour = timeinfo.tm_hour;
  }
}

String isoDate(time_t t) {
  struct tm tmUtc;
  gmtime_r(&t, &tmUtc);
  char buf[25];
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S.000Z", &tmUtc);
  return String(buf);
}

bool connectWiFi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  for (uint8_t i = 0; i < 30 && WiFi.status() != WL_CONNECTED; i++) {
    showWifiProgress(i % MATRIX_W);
    delay(250);
    Serial.print('.');
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi connected, IP: ");
    Serial.println(WiFi.localIP());
    return true;
  }

  Serial.println("WiFi failed; using fallback prices.");
  return false;
}

bool syncClock() {
  configTzTime(TZ_INFO, "pool.ntp.org", "time.google.com");
  Serial.print("Syncing NTP time");
  for (uint8_t i = 0; i < 40; i++) {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 250)) {
      currentHour = timeinfo.tm_hour;
      Serial.println();
      Serial.print("Local hour: ");
      Serial.println(currentHour);
      return true;
    }
    Serial.print('.');
  }
  Serial.println(" failed; simulated hour will advance from fallback value.");
  return false;
}

bool fetchLivePrices() {
  if (WiFi.status() != WL_CONNECTED) return false;

  time_t now = time(nullptr);
  if (now < 1700000000) {
    Serial.println("Clock not set; cannot build EnergyZero date range.");
    return false;
  }

  struct tm local;
  localtime_r(&now, &local);
  local.tm_hour = 0;
  local.tm_min = 0;
  local.tm_sec = 0;
  time_t startLocal = mktime(&local);
  time_t endLocal = startLocal + 24 * 60 * 60;

  String url = "https://api.energyzero.nl/v1/energyprices?fromDate=" + isoDate(startLocal) +
               "&tillDate=" + isoDate(endLocal) +
               "&interval=4&usageType=1&inclBtw=true";

  Serial.println("Fetching live prices:");
  Serial.println(url);

  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.begin(url);
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.print("HTTP failed: ");
    Serial.println(code);
    http.end();
    return false;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, http.getStream());
  http.end();
  if (err) {
    Serial.print("JSON parse failed: ");
    Serial.println(err.c_str());
    return false;
  }

  JsonArray arr = doc["Prices"].as<JsonArray>();
  if (arr.size() < 24) {
    Serial.print("Expected 24 prices, got ");
    Serial.println(arr.size());
    return false;
  }

  for (uint8_t i = 0; i < 24; i++) {
    prices[i] = arr[i]["price"].as<float>();
  }
  livePricesLoaded = true;

  Serial.println("Loaded live EnergyZero prices incl. BTW:");
  for (uint8_t i = 0; i < 24; i++) {
    if (i < 10) Serial.print('0');
    Serial.print(i);
    Serial.print(":00 ");
    Serial.println(prices[i], 3);
  }
  return true;
}

void printStatus() {
  float now = prices[currentHour];
  Serial.print(livePricesLoaded ? "LIVE " : "FALLBACK ");
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
  Serial.println("Live prices: EnergyZero public API, incl. BTW, Netherlands.");
  Serial.println("10 columns = next 10 hours; bar height/color = price; white top dot = current hour.");

  if (connectWiFi()) {
    syncClock();
    fetchLivePrices();
  }

  updateCurrentHourFromClock();
  printStatus();
  drawPriceBars();
}

void loop() {
  // Refresh live prices every hour if possible.
  if (millis() - lastFetchAttempt > 60UL * 60UL * 1000UL) {
    lastFetchAttempt = millis();
    fetchLivePrices();
  }

  // In Wokwi, update display every 2 seconds. With a valid clock this stays on
  // the real current hour; otherwise it advances so the demo remains animated.
  if (millis() - lastFrame > 2000) {
    lastFrame = millis();
    uint8_t before = currentHour;
    updateCurrentHourFromClock();
    if (currentHour == before && !livePricesLoaded) {
      currentHour = (currentHour + 1) % 24;
    }
    printStatus();
    drawPriceBars();
  }
}
