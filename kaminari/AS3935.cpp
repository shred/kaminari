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
#include <SPI.h>

#include "AS3935.h"

#define BITRATE 1400000         // max bitrate is 2 MHz, should not be dividable by 500 kHz
#define NOISE_LEVEL_RAISE_DELAY      60000  // Delay before noise level can be raised again
#define NOISE_LEVEL_REDUCTION_DELAY 600000  // Delay before noise reduction is lowered again

const int outdoorLevels[]   = { 390,  630,  860, 1100, 1140, 1570, 1800, 2000 };
const int indoorLevels[]    = {  28,   45,   62,   78,   95,  112,  130,  146 };
const int minNumLightning[] = { 1, 5, 9, 16 };

volatile static unsigned long counter = 0;       // Internal counter

ICACHE_RAM_ATTR static void counterISR() {
    counter++;
}


AS3935::AS3935(int csPin, int intPin) {
    unsigned long now = millis();
    this->csPin = csPin;
    this->intPin = intPin;
    this->frequency = 0;
    this->lastNoiseLevelChange = now;
    this->lastNoiseLevelRaise = now;
    this->noiseLevelBalance = 0;
    this->currentNoiseFloorLevel = 0;
    this->currentOutdoorMode = false;
    this->noiseFloorLevelOutOfRange = false;
    this->disturberCounterStart = now;
    this->disturberCounter = 0;

    clearDetections();
}

void AS3935::begin() {
    SPI.begin();

    pinMode(this->csPin, OUTPUT);
    digitalWrite(this->csPin, HIGH);

    pinMode(this->intPin, INPUT);

    attachInterrupt(digitalPinToInterrupt(this->intPin), counterISR, RISING);
}

void AS3935::end() {
    detachInterrupt(digitalPinToInterrupt(this->intPin));
    digitalWrite(this->csPin, HIGH);
    SPI.end();
}

bool AS3935::update() {
    bool hasChanged = false;
    unsigned long now = millis();

    if (counter > 0) {
        counter = 0;

        open();
        delay(2);       // wait 2 ms before reading the interrupt register!
        char interrupt = spiRead(0x03) & 0x0F;
        close();

        if ((interrupt & 0x01) != 0 && (now - lastNoiseLevelRaise) > NOISE_LEVEL_RAISE_DELAY) {
            // Noise level too high
            lastNoiseLevelChange = now;
            lastNoiseLevelRaise = now;
            noiseLevelBalance++;
            if (noiseLevelBalance > 1) {
                noiseLevelBalance--;
                if (raiseNoiseFloorLevel()) {
                    noiseFloorLevelOutOfRange = false;
                } else {
                    noiseFloorLevelOutOfRange = true;
                }
                hasChanged = true;
            }
        }

        if ((interrupt & 0x04) != 0) {
            // Disturber detected
            lastDisturberDetection = now;
            disturberCounter++;
            hasChanged = true;
        }

        if ((interrupt & 0x08) != 0) {
            // Lightning detected
            for (int ix = sizeof(lastLightningDetections) / sizeof(Lightning) - 1; ix > 0; ix--) {
                lastLightningDetections[ix] = lastLightningDetections[ix - 1];
            }
            lastLightningDetections[0].time = now;
            lastLightningDetections[0].energy = getEnergy();
            lastLightningDetections[0].distance = getEstimatedDistance();
            hasChanged = true;
        }
    }

    // Try to reduce the noise level again
    if ((now - lastNoiseLevelChange) > NOISE_LEVEL_REDUCTION_DELAY) {
        lastNoiseLevelChange = now;
        noiseLevelBalance--;
        if (noiseLevelBalance < -1) {
            noiseLevelBalance++;
            hasChanged = true;
            if (noiseFloorLevelOutOfRange) {
                noiseFloorLevelOutOfRange = false;
            } else {
                reduceNoiseFloorLevel();
            }
        }
    }

    return hasChanged;
}

void AS3935::powerDown() {
    open();
    spiWrite(0x00, 0x25);
    close();
}

void AS3935::reset() {
    open();
    spiWrite(0x3C, 0x96);       // PRESET_DEFAULT: Reset all registers
    delay(2);
    spiRead(0x03);              // Clear interrupts
    counter = 0;
    close();
}

unsigned long AS3935::calibrate(unsigned long freq) {
    open();
    if (spiRead(0x00) & 0x37 == 0x00) {
        Serial.println("The AS3935 does not respond. Please check your SPI wiring!");
    }

    spiWrite(0x03, 0xC0);       // Set division ratio to 128

    byte mask = 0x00;
    unsigned long actualFreq;
    for (int bit = 3; bit >= 0; bit--) {
        spiWrite(0x08, 0x80 | mask | (1 << bit));
        counter = 0;
        delay(1000);
        actualFreq = counter * 128;
        if (actualFreq/1000 > freq/1000) {
            mask |= 1 << bit;
        }
    }

    if (actualFreq == 0) {
        Serial.println("No interrupt was detected during calibration. Please check your IRQ wiring!");
    } else if (actualFreq < 483092 || actualFreq > 517500) {
        Serial.println("Warning: calibrated frequency is out of tolerance range.");
    }

    this->frequency = actualFreq;

    spiWrite(0x08, mask);           // Set the target frequency, disable DISP_LCO output
    delay(50);                      // Let everything settle for a while
    spiWrite(0x3D, 0x96);           // CALIB_RCO: Now calibrate the RCOs
    spiWrite(0x08, 0x20 | mask);    // Enable TRCO for calibration
    delay(2);                       // Wait another 2 ms
    spiWrite(0x08, mask);           // Disable TRCO again
    delay(50);                      // Let everything settle again
    spiRead(0x03);                  // Clear interrupts
    counter = 0;

    close();
    return actualFreq;
}

bool AS3935::raiseNoiseFloorLevel() {
    bool success = false;
    open();
    byte current = spiRead(0x01);
    int value = (current >> 4) & 0x07;
    value++;
    if (value <= 7) {
        spiWrite(0x01, (current & 0x8F) | value << 4);
        success = true;
    }
    updateNoiseFloorLevel();
    close();
    return success;
}

bool AS3935::reduceNoiseFloorLevel() {
    bool success = false;
    open();
    byte current = spiRead(0x01);
    int value = (current >> 4) & 0x07;
    value--;
    if (value >= 0) {
        spiWrite(0x01, (current & 0x8F) | value << 4);
        success = true;
    }
    updateNoiseFloorLevel();
    close();
    return success;
}

int AS3935::getNoiseFloorLevel() {
    if (currentNoiseFloorLevel == 0) {
        open();
        updateNoiseFloorLevel();
        close();
    }
    return currentNoiseFloorLevel;
}

bool AS3935::isNoiseFloorLevelOutOfRange() const {
    return noiseFloorLevelOutOfRange;
}

bool AS3935::getOutdoorMode() const {
    return currentOutdoorMode;
}

void AS3935::setOutdoorMode(bool outdoor) {
    open();
    spiWrite(0x00, outdoor ? 0x1C : 0x24);
    updateNoiseFloorLevel();
    close();
}

int AS3935::getWatchdogThreshold() const {
    open();
    int wdth = spiRead(0x01) & 0x0F;
    close();
    return wdth;
}

void AS3935::setWatchdogThreshold(int threshold) {
    open();
    byte current = spiRead(0x01);
    current = (current & 0xF0) | (threshold & 0x0F);
    spiWrite(0x01, current);
    close();
}

void AS3935::clearStatistics() {
    open();
    byte current = spiRead(0x02);
    current &= 0xBF;
    spiWrite(0x02, current);
    delay(2);
    current |= 0x40;
    spiWrite(0x02, current);
    close();
}

int AS3935::getMinimumNumberOfLightning() const {
    open();
    int value = (spiRead(0x02) & 0x30) >> 4;
    close();
    return minNumLightning[value];
}

void AS3935::setMinimumNumberOfLightning(int num) {
    int value = -1;
    switch (num) {
        case  1: value = 0x00; break;
        case  5: value = 0x01; break;
        case  9: value = 0x02; break;
        case 16: value = 0x03; break;
    }
    if (value >= 0) {
        open();
        byte current = spiRead(0x02);
        current &= 0xCF;
        current |= value << 4;
        spiWrite(0x02, current);
        close();
    }
}

int AS3935::getSpikeRejection() const {
    open();
    int value = spiRead(0x02) & 0x0F;
    close();
    return value;
}

void AS3935::setSpikeRejection(int rejection) {
    open();
    byte current = spiRead(0x02);
    current &= 0xF0;
    current |= rejection & 0x0F;
    spiWrite(0x02, current);
    close();
}

unsigned long AS3935::getFrequency() const {
    return this->frequency;
}

unsigned long AS3935::getEnergy() const {
    open();
    byte mmsb = spiRead(0x06) & 0x1F;
    byte msb  = spiRead(0x05) & 0xFF;
    byte lsb  = spiRead(0x04) & 0xFF;
    unsigned long energy = (mmsb << 16) | (msb << 8) | lsb;
    close();
    return energy;
}

unsigned int AS3935::getEstimatedDistance() const {
    open();
    unsigned int distance = spiRead(0x07) & 0x3F;
    close();
    return distance;
}

unsigned long AS3935::getLastDisturberDetection() const {
    return lastDisturberDetection;
}

unsigned int AS3935::getDisturbersPerMinute() const {
    return (60 * 1000L * disturberCounter) / (millis() - disturberCounterStart);
}

bool AS3935::getLastLightningDetection(int index, Lightning& lightning) const {
    if (index >= 0
            && index < sizeof(lastLightningDetections) / sizeof(Lightning)
            && lastLightningDetections[index].time != 0) {
        lightning.time = lastLightningDetections[index].time;
        lightning.energy = lastLightningDetections[index].energy;
        lightning.distance = lastLightningDetections[index].distance;
        return true;
    }
    return false;
}

void AS3935::clearDetections() {
    lastDisturberDetection = 0;
    for (int ix = 0; ix < sizeof(lastLightningDetections) / sizeof(Lightning); ix++) {
        lastLightningDetections[ix].time = 0;
    }
    disturberCounterStart = millis();
    disturberCounter = 0;
}

void AS3935::dump(byte* dump) const {
    open();
    digitalWrite(csPin, LOW);
    SPI.transfer(0x40);
    for (int i = 0; i < 0x33; i++) {
        dump[i] = SPI.transfer(0x00);
    }
    digitalWrite(csPin, HIGH);
    close();
}

void AS3935::debug() const {
    char output[40];
    open();
    digitalWrite(csPin, LOW);
    SPI.transfer(0x40);
    for (int i = 0; i <= 0x32; i++) {
        byte value = SPI.transfer(0x00);
        sprintf(output, "%02X = %02X", i, value);
        Serial.println(output);
    }
    digitalWrite(csPin, HIGH);
    close();
}

void AS3935::open() const {
    SPI.beginTransaction(SPISettings(BITRATE, MSBFIRST, SPI_MODE1));
}

void AS3935::close() const {
    SPI.endTransaction();
}

void AS3935::spiWrite(byte address, byte value) const {
    digitalWrite(csPin, LOW);
    SPI.transfer(address & 0x3F);
    SPI.transfer(value);
    digitalWrite(csPin, HIGH);
}

byte AS3935::spiRead(byte address) const {
    digitalWrite(csPin, LOW);
    SPI.transfer(0x40 | (address & 0x3F));
    byte result = SPI.transfer(0x00);
    digitalWrite(csPin, HIGH);
    return result;
}

void AS3935::updateNoiseFloorLevel() {
    int level = (spiRead(0x01) >> 4) & 0x07;
    currentOutdoorMode = spiRead(0x00) == 0x1C;
    currentNoiseFloorLevel = currentOutdoorMode ? outdoorLevels[level] : indoorLevels[level];
}
