/*
 * Kaminari
 *
 * Copyright (C) 2020 Richard "Shred" Körber
 *   https://github.com/shred/kaminari
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <SPI.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <Adafruit_NeoPixel.h>
#include <PubSubClient.h>

#include "AS3935.h"
#include "Config.h"
#include "myWiFi.h"

#define SPI_CS   15     // IO15 (D8)
#define SPI_MOSI 13     // IO13 (D7)
#define SPI_MISO 12     // IO12 (D6)
#define SPI_SCK  14     // IO14 (D5)
#define AS_INT    5     // IO5  (D1)
#define NEOPIXEL  4     // IO4  (D2)

#define PORT 80

#define COLOR_CONNECTING   0x00800000
#define COLOR_CALIBRATING  0x00404000
#define COLOR_BLACK        0x00000000

#define DISTANCE_ANIMATION_TIME 5000
#define ENERGY_ANIMATION_TIME    500

ConfigManager cfgMgr;
ESP8266WebServer server(PORT);
AS3935 detector(SPI_CS, AS_INT);
Adafruit_NeoPixel neopixel(1, NEOPIXEL, NEO_GRBW + NEO_KHZ800);
WiFiClient wifiClient;
PubSubClient client(MY_MQTT_SERVER_HOST, MY_MQTT_SERVER_PORT, wifiClient);

unsigned long beforeConnection = millis();
unsigned long beforeAnimation = millis();
unsigned long beforeMqttConnection = millis();
bool connected = false;
unsigned int currentColor = 0;
Lightning ledLightning;

inline static unsigned long timeDifference(unsigned long now, unsigned long past) {
    // This is actually safe from millis() overflow because all types are unsigned long!
    return past > 0 ? (now - past) : 0;
}

void sendJsonResponse(ARDUINOJSON_NAMESPACE::JsonDocument &doc, int status = 200) {
    String json;
    serializeJson(doc, json);
    server.send(status, "application/json", json);
}

bool authenticated() {
    if (server.header("X-API-Key") == MY_APIKEY) {
        return true;
    }
    if (server.arg("api_key") == MY_APIKEY) {
        return true;
    }
    server.send(403);
    return false;
}

void handleStatus() {
    DynamicJsonDocument doc(8192);

    unsigned long now = millis();

    JsonArray la = doc.createNestedArray("lightnings");
    Lightning lightning;
    for (int ix = 0; ; ix++) {
        if (!detector.getLastLightningDetection(ix, lightning)) {
            break;
        }
        StaticJsonDocument<200> subdoc;
        subdoc["age"] = timeDifference(now, lightning.time) / 1000;
        subdoc["energy"] = lightning.energy;
        if (lightning.distance < 0x3F) {
            subdoc["distance"] = lightning.distance;
        } else {
            subdoc["distance"] = (char*) NULL;
        }
        la.add(subdoc);
    }

    unsigned int distance = detector.getEstimatedDistance();
    if (distance < 0x3F) {
        doc["distance"] = distance;
    } else {
        doc["distance"] = (char*) NULL;
    }

    doc["energy"] = detector.getEnergy();
    doc["noiseFloorLevel"] = detector.getNoiseFloorLevel();
    doc["disturbersPerMinute"] = detector.getDisturbersPerMinute();
    doc["watchdogThreshold"] = detector.getWatchdogThreshold();

    sendJsonResponse(doc);
}

void handleSettings() {
    DynamicJsonDocument doc(1024);
    doc["tuning"] = detector.getFrequency();
    doc["noiseFloorLevel"] = detector.getNoiseFloorLevel();
    doc["outdoorMode"] = detector.getOutdoorMode();
    doc["watchdogThreshold"] = detector.getWatchdogThreshold();
    doc["minimumNumberOfLightning"] = detector.getMinimumNumberOfLightning();
    doc["spikeRejection"] = detector.getSpikeRejection();
    doc["outdoorMode"] = detector.getOutdoorMode();
    doc["upperDisturberThreshold"] = detector.getUpperDisturberThreshold();
    doc["lowerDisturberThreshold"] = detector.getLowerDisturberThreshold();
    doc["autoWatchdogMode"] = detector.isAutoWatchdogMode();
    doc["statusLed"] = cfgMgr.config.ledEnabled;
    doc["blueBrightness"] = cfgMgr.config.blueBrightness;
    sendJsonResponse(doc);
}

void handleUpdate() {
    if (authenticated()) {
        if (server.hasArg("watchdogThreshold")) {
            long val = String(server.arg("watchdogThreshold")).toInt();
            if (val >= 0 && val <= 10) {
                cfgMgr.config.watchdogThreshold = val;
                detector.setWatchdogThreshold(val);
            }
        }

        if (server.hasArg("minimumNumberOfLightning")) {
            long val = String(server.arg("minimumNumberOfLightning")).toInt();
            if (val == 1 || val == 5 || val == 9 || val == 16) {
                cfgMgr.config.minimumNumberOfLightning = val;
                detector.setMinimumNumberOfLightning(val);
            }
        }

        if (server.hasArg("spikeRejection")) {
            long val = String(server.arg("spikeRejection")).toInt();
            if (val >= 0 && val <= 11) {
                cfgMgr.config.spikeRejection = val;
                detector.setSpikeRejection(val);
            }
        }

        if (server.hasArg("outdoorMode")) {
            bool val = String(server.arg("outdoorMode")) == "true";
            cfgMgr.config.outdoorMode = val;
            detector.setOutdoorMode(val);
        }

        if (server.hasArg("upperDisturberThreshold")) {
            long val = String(server.arg("upperDisturberThreshold")).toInt();
            if (val >= 0 && val <= 10000) {
                cfgMgr.config.upperDisturberThreshold = val;
                detector.setUpperDisturberThreshold(val);
            }
        }

        if (server.hasArg("lowerDisturberThreshold")) {
            long val = String(server.arg("lowerDisturberThreshold")).toInt();
            if (val >= 0 && val <= 10000) {
                cfgMgr.config.lowerDisturberThreshold = val;
                detector.setLowerDisturberThreshold(val);
            }
        }

        if (server.hasArg("autoWatchdogMode")) {
            bool val = String(server.arg("autoWatchdogMode")) == "true";
            cfgMgr.config.autoWatchdogMode = val;
            detector.setAutoWatchdogMode(val);
        }

        if (server.hasArg("statusLed")) {
            cfgMgr.config.ledEnabled = String(server.arg("statusLed")) == "true";
        }

        if (server.hasArg("blueBrightness")) {
            long val = String(server.arg("blueBrightness")).toInt();
            if (val >= 0 && val <= 255) {
                cfgMgr.config.blueBrightness = val;
            }
        }

        cfgMgr.commit();
        handleSettings();
    }
}

void handleCalibrate() {
    if (authenticated()) {
        unsigned int oldColor = currentColor;
        color(COLOR_CALIBRATING);
        StaticJsonDocument<200> doc;
        doc["tuning"] = detector.calibrate();
        sendJsonResponse(doc);
        color(oldColor);
    }
}

void handleClear() {
    if (authenticated()) {
        detector.clearDetections();
        server.send(200, "text/plain", "OK");
    }
}

void handleReset() {
    if (authenticated()) {
        detector.reset();
        handleCalibrate();
        setupDetector();
        detector.clearStatistics();
    }
}

void sendMqttStatus() {
    DynamicJsonDocument doc(8192);
    String json;

    Lightning lightning;
    if (detector.getLastLightningDetection(0, lightning)) {
        doc["energy"] = lightning.energy;
        if (lightning.distance < 0x3F) {
            doc["distance"] = lightning.distance;
        } else {
            doc["distance"] = (char*) NULL;
        }
    } else {
        doc["energy"] = (char*) NULL;
        doc["distance"] = (char*) NULL;
    }

    doc["tuning"] = detector.getFrequency();
    doc["noiseFloorLevel"] = detector.getNoiseFloorLevel();
    doc["disturbersPerMinute"] = detector.getDisturbersPerMinute();
    doc["watchdogThreshold"] = detector.getWatchdogThreshold();

    serializeJson(doc, json);
    if (!client.publish(MY_MQTT_TOPIC, json.c_str(), MY_MQTT_RETAIN)) {
        Serial.print("Failed to send MQTT message, rc=");
        Serial.println(client.state());
    }
}

void setupDetector() {
    detector.setOutdoorMode(cfgMgr.config.outdoorMode);
    detector.setWatchdogThreshold(cfgMgr.config.watchdogThreshold);
    detector.setMinimumNumberOfLightning(cfgMgr.config.minimumNumberOfLightning);
    detector.setSpikeRejection(cfgMgr.config.spikeRejection);
    detector.setUpperDisturberThreshold(cfgMgr.config.upperDisturberThreshold);
    detector.setLowerDisturberThreshold(cfgMgr.config.lowerDisturberThreshold);
    detector.setAutoWatchdogMode(cfgMgr.config.autoWatchdogMode);
}

void color(unsigned int color) {
    if (color != currentColor) {
        neopixel.fill(color);
        neopixel.show();
        currentColor = color;
    }
}

inline int limit(int v) {
    return v <= 255 ? (v >= 0 ? v : 0): 255;
}

void updateColor(unsigned long now) {
    if (!connected) {
        color(now % 1000 <= 500 ? COLOR_CONNECTING : COLOR_BLACK);
        return;
    }

    if (!cfgMgr.config.ledEnabled) {
        color(COLOR_BLACK);
        return;
    }

    // Calculate blue color (noise floor level)
    int blue = 0;
    if (detector.isNoiseFloorLevelOutOfRange()) {
        blue = now % 1000 <= 500 ? (cfgMgr.config.blueBrightness > 128 ? cfgMgr.config.blueBrightness : 128) : 0;
    } else {
        blue = detector.getNoiseFloorLevel() * cfgMgr.config.blueBrightness / (detector.getOutdoorMode() ? 2000 : 146);
    }

    // Calculate lightning age (white) and distance (green to red)
    long red = 0;
    long green = 0;
    long white = 0;
    if (ledLightning.time > 0 && ledLightning.distance <= 40) {
        unsigned long diff = timeDifference(now, ledLightning.time);
        long distancePast = (DISTANCE_ANIMATION_TIME + ENERGY_ANIMATION_TIME) - diff;
        long energyPast = ENERGY_ANIMATION_TIME - diff;
        if (energyPast >= 0) {
            white = ((     ledLightning.energy  ) * energyPast   * 255) / (300000 * ENERGY_ANIMATION_TIME);
            blue = 0;
        } else if (distancePast >= 0) {
            green = ((     ledLightning.distance) * distancePast * 255) / (40 * DISTANCE_ANIMATION_TIME);
            red   = ((40 - ledLightning.distance) * distancePast * 255) / (40 * DISTANCE_ANIMATION_TIME);
            long newblue = ((long) blue) * diff / (DISTANCE_ANIMATION_TIME + ENERGY_ANIMATION_TIME);
            if (newblue < blue) {
                blue = newblue;
            }
        }
    }

    color(    (limit(white) & 0xFF) << 24
            | (limit(red  ) & 0xFF) << 16
            | (limit(green) & 0xFF) << 8
            | (limit(blue ) & 0xFF));
}

void setup() {
    Serial.begin(115200);
    Serial.println("");

    cfgMgr.begin();

    detector.begin();

    neopixel.begin();
    neopixel.setBrightness(255);

    // Init detector
    color(COLOR_CALIBRATING);
    Serial.println("Calibrating antenna...");
    detector.reset();
    unsigned int freq = detector.calibrate();
    Serial.print("Calibrated antenna frequency: ");
    Serial.print(freq);
    Serial.println(" Hz");
    color(COLOR_BLACK);

    // Set up the detector
    setupDetector();

    // Start WiFi
    WiFi.mode(WIFI_STA);
    WiFi.begin(MY_SSID, MY_PSK);

    // Start MDNS
    if (MDNS.begin(MY_MDNS_NAME)) {
        Serial.println("MDNS responder started");
        MDNS.addService("http", "tcp", PORT);
    }

    // Start server
    server.on("/status", handleStatus);
    server.on("/settings", handleSettings);
    server.on("/update", handleUpdate);
    server.on("/calibrate", handleCalibrate);
    server.on("/clear", handleClear);
    server.on("/reset", handleReset);
    server.onNotFound([]() {
        server.send(404, "text/plain", server.uri() + ": not found\n");
    });
    const char * headerkeys[] = { "X-API-Key" };
    server.collectHeaders(headerkeys, sizeof(headerkeys) / sizeof(char*));
    server.begin();
    Serial.print("Server is listening on port ");
    Serial.println(PORT);
}

void loop() {
    const unsigned long now = millis();

    if (timeDifference(now, beforeConnection) > 1000) {
        beforeConnection = now;
        if (WiFi.status() != WL_CONNECTED) {
            connected = false;
        } else if (!connected) {
            Serial.print("Connected to ");
            Serial.println(WiFi.SSID());
            Serial.print("IP address: ");
            Serial.println(WiFi.localIP());
            Serial.print("MAC: ");
            Serial.println(WiFi.macAddress());
            connected = true;
        }
    }

    if (connected) {
        server.handleClient();
        MDNS.update();

        #ifdef MY_MQTT_ENABLED
        if (!client.loop() && timeDifference(now, beforeMqttConnection) > 1000) {
            beforeMqttConnection = now;
            if (client.connect("kaminari", MY_MQTT_USER, MY_MQTT_PASSWORD)) {
                Serial.println("Successfully connected to MQTT server");
            } else {
                Serial.print("Connection to MQTT server failed, rc=");
                Serial.println(client.state());
            }
        }
        #endif
    }

    if (detector.update()) {
        if (!detector.getLastLightningDetection(0, ledLightning)) {
            ledLightning.time = 0;
            #ifdef MY_MQTT_ENABLED
            if (connected && client.connected()) {
                sendMqttStatus();
            }
            #endif
        }
    }

    if (timeDifference(now, beforeAnimation) > 50) {
        beforeAnimation = now;
        updateColor(now);
    }
}
