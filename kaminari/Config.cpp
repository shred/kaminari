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

#include <Arduino.h>
#include <EEPROM_Rotate.h>

#include "Config.h"

#define CHECKSUM_ADDRESS 0xF00
#define CHECKSUM_VALUE 0x8C

void ConfigManager::begin() {
    EEPROMr.offset(0xFF0);
    EEPROMr.begin(4096);
    EEPROMr.get(0, config);

    int checksum = EEPROMr.read(CHECKSUM_ADDRESS);
    if (checksum != CHECKSUM_VALUE) {
        EEPROMr.write(CHECKSUM_ADDRESS, CHECKSUM_VALUE);
        init();
    }
}

void ConfigManager::init() {
    config.ledEnabled = true;
    config.blueBrightness = 48;
    config.watchdogThreshold = 1;
    config.minimumNumberOfLightning = 1;
    config.spikeRejection = 2;
    config.outdoorMode = false;
    Serial.println("Configuration was initialized.");
    commit();
}

void ConfigManager::commit() {
    EEPROMr.put(0, config);
    if (EEPROMr.commit()) {
        Serial.println("Configuration was persisted.");
    } else {
        Serial.println("Failed to persist configuration.");
    }
}
