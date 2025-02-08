/*
 * Kaminari
 *
 * Copyright (C) 2020 Richard "Shred" Körber
 *   https://codeberg.org/shred/kaminari
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

#ifndef __Config__
#define __Config__

#include <EEPROM_Rotate.h>

/**
 * Kaminari's permanent configuration.
 */
struct Config {
    int watchdogThreshold;
    int minimumNumberOfLightning;
    int spikeRejection;
    int blueBrightness;
    bool ledEnabled;
    bool outdoorMode;
    int disturberBrightness;
};

/**
 * Manager for the configuration structure.
 */
class ConfigManager {
public:

    /**
     * Set up the config, reading the config from EEPROM.
     */
    void begin();

    /**
     * Initialize the configuration, by setting default values.
     */
    void init();

    /**
     * Commit changes to the config in the EEPROM.
     */
    void commit();

    /**
     * The configuration itself.
     */
    Config config;

private:
    EEPROM_Rotate EEPROMr;
};

#endif