#include <ArduinoJson.h>
#include <FastLED.h>
#include <HTTPClient.h>
#include <M5Unified.h>
#include <WiFi.h>

#include "M5UnitENV.h"

// constants
const int NUM_LEDS = 1;
const int LED_DATA_PIN = 27;
CRGB leds[NUM_LEDS];

const char* ssid = "";
const char* pass = "";

const char* url = "";
const char* macAddress = "";

const char* dbID = "";
const char* dbURL = "";
const char* token = "";

// env sensor
SHT3X sht3x;
QMP6988 qmp;

double tmp = 0.0;
double hum = 0.0;
double pressure = 0.0;

// LED
void setupLED() {
    FastLED.addLeds<WS2811, LED_DATA_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(20);
}

void resetLED() {
    leds[0] = CRGB::Black;
    FastLED.show();
}

// wifi
void setupWiFi() {
    WiFi.begin(ssid, pass);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        leds[0] = CRGB::Amethyst;
        FastLED.show();
        delay(1000);
        leds[0] = CRGB::Black;
        FastLED.show();
        delay(1000);
        Serial.println("Connecting...");
    }
    Serial.println("WiFi Connected");
}

// env sensor
void setupEnv3() {
    if (!qmp.begin(&Wire, QMP6988_SLAVE_ADDRESS_L, 26, 32, 400000U)) {
        while (1) {
            Serial.println("Couldn't find QMP6988");
            leds[0] = CRGB::Orange;
            FastLED.show();
            delay(500);
        }
    }

    if (!sht3x.begin(&Wire, SHT3X_I2C_ADDR, 26, 32, 400000U)) {
        while (1) {
            Serial.println("Couldn't find SHT3X");
            leds[0] = CRGB::Orange;
            FastLED.show();
            delay(500);
        }
    }
}

bool wakeOnLAN() {
    HTTPClient http;

    http.begin(url);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    String postData = "macSelect=" + String(macAddress);
    int httpResponseCode = http.POST(postData);

    if (httpResponseCode > 0) {
        String response = http.getString();
        Serial.println("WOL Response: " + response);
        http.end();
        return true;
    } else {
        Serial.println("Error on sending POST: " + String(httpResponseCode));
        http.end();
        return false;
    }
}

JsonDocument serialize(double temp, double humi, double press) {
    JsonDocument doc;

    JsonObject parent = doc["parent"].to<JsonObject>();
    parent["database_id"] = dbID;

    JsonObject properties = doc["properties"].to<JsonObject>();

    JsonObject temperature = properties["temperature"].to<JsonObject>();
    temperature["number"] = temp;

    JsonObject humidity = properties["humidity"].to<JsonObject>();
    humidity["number"] = humi;

    JsonObject pressure = properties["pressure"].to<JsonObject>();
    pressure["number"] = press;

    return doc;
}

void postNotion(JsonDocument doc) {
    HTTPClient http;

    std::string bearer = std::string("Bearer ") + token;
    http.begin(dbURL);
    http.addHeader("Authorization", String(bearer.c_str()));
    http.addHeader("Notion-Version", "2022-06-28");
    http.addHeader("Content-Type", "application/json");

    // payload
    String payload;
    serializeJson(doc, payload);

    int httpCode = http.POST(payload);
    if ((httpCode > 0) && (httpCode != HTTP_CODE_OK)) {
        Serial.printf("Failed, error: %s\n",
                      http.errorToString(httpCode).c_str());
    }

    http.end();
    return;
}

void setup() {
    M5.begin();
    Serial.begin(115200);

    setupLED();
    setupWiFi();
    setupEnv3();

    leds[0] = CRGB::Blue;
    FastLED.show();
    delay(2000);
    resetLED();
}

void loop() {
    static unsigned long lastUpdateTime = 0;
    const unsigned long interval = 5 * 60 * 1000;  // 5 min
    M5.update();

    // Wake on LAN
    if (M5.BtnA.isPressed()) {
        leds[0] = CRGB::Yellow;
        FastLED.show();

        if (wakeOnLAN()) {
            leds[0] = CRGB::Green;
        } else {
            leds[0] = CRGB::Red;
        }
        FastLED.show();
        delay(2000);
        resetLED();
    }

    // timer
    unsigned long currentTime = millis();
    if (currentTime - lastUpdateTime >= interval) {
        lastUpdateTime = currentTime;

        bool sht3xUpdated = sht3x.update();
        bool qmpUpdated = qmp.update();

        if (sht3xUpdated && qmpUpdated) {
            double temperature_sht3x = sht3x.cTemp;
            double temperature_qmp = qmp.cTemp;
            double temperature = (temperature_sht3x + temperature_qmp) / 2;

            double humidity = sht3x.humidity;
            double pressure = qmp.pressure;

            JsonDocument doc = serialize(temperature, humidity, pressure);

            postNotion(doc);
        }

        leds[0] = CRGB::Black;
        FastLED.show();
    }
}
