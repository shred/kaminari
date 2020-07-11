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

#ifndef __AS3935__
#define __AS3935__

struct Lightning {
    unsigned long time;
    unsigned long energy;
    unsigned int distance;
};

/**
 * Driver for an AS3935 Franklin Lightning Detector connected via SPI.
 * 
 * The driver takes care for automatic adjustment of the noise floor level. If too much
 * noise is detected, the noise floor level is raised automatically. After a while, the
 * noise floor level is lowered again.
 * 
 * The driver collects the time, energy and distance of up to 16 lightning events. After
 * that, if another lightning is detected, the oldest event is removed.
 * 
 * All returned times represent the system time (millis()) when the event occured.
 * 
 * Note that for technical reasons, you can only run one instance per microcontroller.
 */
class AS3935 {
public:

    /**
     * Create a new AS3935 driver object.
     *
     * @param csPin     Pin number of the detector's Chip Select input
     * @param intPin    Pin number of the detector's Interrupt output
     */
    AS3935(int csPin, int intPin);

    /**
     * Begin working with the detector. This call initiates the hardware.
     */
    void begin();

    /**
     * End working with the detector. Resources are freed.
     */
    void end();

    /**
     * Update the detector status. This method should be invoked frequently, e.g. in
     * loop().
     * 
     * @return true if something has changed, false if nothing happened
     */
    bool update();

    /**
     * Power down the detector. It can be powered up by a reset, and must then be
     * recalibrated.
     */
    void powerDown();

    /**
     * Reset the detector. All settings are reset to default values.
     */
    void reset();

    /**
     * Auto-tune the antenna resonator to the given frequency.
     *
     * The calibration process takes a little more than 4 seconds. If the target frequency
     * cannot be reached, the closest possible frequency is used instead. System RCO and
     * Timer RCO are calibrated as well.
     *
     * @param freq      Target frequency in Hz, 500 kHz by default
     * @return Actual tuned-in resonator frequency (measured, not 100% accurate)
     */
    unsigned long calibrate(unsigned long freq = 500000);

    /**
     * Raise the noise floor level. The detector will be less sensitive to noise, but also
     * less sensititve to lightning events.
     *
     * Usually the driver takes care of adjusting the noise floor level automatically,
     * so there is no need to invoke this method.
     * 
     * @return true if the level could be raised, false if the upper limit was reached.
     */
    bool raiseNoiseFloorLevel();

    /**
     * Reduce the noise floor level. The detector will be more sensitive to lightning
     * events, but also more sensitive to noise.
     *
     * Usually the driver takes care of adjusting the noise floor level automatically,
     * so there is no need to invoke this method.
     * 
     * @return true if the level could be reduced, false if the lower limit was reached.
     */
    bool reduceNoiseFloorLevel();

    /**
     * Read the current continuous input noise level, in µVrms.
     */
    int getNoiseFloorLevel();

    /**
     * Return true if the noise floor level is out of its upper range. The detector is
     * unable to work properly if this happens.
     */
    bool isNoiseFloorLevelOutOfRange() const;

    /**
     * Return the current outdoor mode setting.
     */
    bool getOutdoorMode() const;

    /**
     * Enable or disable the outdoor mode.
     */
    void setOutdoorMode(bool outdoor);

    /**
     * Return the current watchdog threshold.
     */
    int getWatchdogThreshold() const;

    /**
     * Set a watchdog threshold. Value must be between 0 and 15. Default is 1.
     */
    void setWatchdogThreshold(int threshold);

    /**
     * Clear the statistics of the lightning distance estimation algorithm. Usually there
     * is no need to clear the statistics manually.
     */
    void clearStatistics();

    /**
     * Return the current minimum number of lightnings required for lightning detection.
     */
    int getMinimumNumberOfLightning() const;

    /**
     * Set the minimum number of lightnings required for lightning detection. Value must
     * be 1, 5, 9, or 16. Other values will be ignored.
     */
    void setMinimumNumberOfLightning(int num);

    /**
     * Return the current spike rejection value.
     */
    int getSpikeRejection() const;

    /**
     * Set a new spike rejection value. Value must be between 0 and 15. Default is 2.
     */
    void setSpikeRejection(int rejection);

    /**
     * Read the resonator frequency since last calibration run. If 0, the detector has not
     * been calibrated yet. The frequency was measured and is not 100% accurate.
     */
    unsigned long getFrequency() const;

    /**
     * Read the estimated energy of the last lightning or disturber. The energy has no
     * actual physical unit.
     */
    unsigned long getEnergy() const;

    /**
     * Read the estimated distance of the head of the approaching storm. The distance is
     * returned in kilometers. 1 means that the storm is overhead. 0x3F means that a
     * possible storm is out of range.
     */
    unsigned int getEstimatedDistance() const;

    /**
     * Return the number of detected disturbers per minute. The value is cleared when
     * clearDetections() is invoked.
     */
    unsigned int getDisturbersPerMinute() const;

    /**
     * Return one of the last detected lightnings. Lightnings are always returned in
     * descending order, starting from the most recent event.
     * 
     * @param index     Index number of lightning detection, starting with 0
     * @param lightning Target structure
     * @return true if the target structure was filled with lightning data, false if
     *         there is no more lightning data.
     */
    bool getLastLightningDetection(int index, Lightning& lightning) const;

    /**
     * Clear all detected lightnings.
     */
    void clearDetections();

    /**
     * Dump the AS3935 register set.
     * 
     * @param dump      Dump target, must be able to contain 51 bytes.
     */
    void dump(byte* dump) const;

    /**
     * Debug the AS3935 detector by dumping all registers to the console.
     */
    void debug() const;

private:
    int csPin;
    int intPin;
    unsigned long frequency;
    unsigned long lastNoiseLevelChange;
    unsigned long lastNoiseLevelRaise;
    unsigned long disturberCounterStart;
    int noiseLevelBalance;
    int currentNoiseFloorLevel;
    int disturberCounter;
    bool currentOutdoorMode;
    bool noiseFloorLevelOutOfRange;
    Lightning lastLightningDetections[16];

    /**
     * Open the connection to the detector.
     *
     * There is no check whether SPI is currently in use!
     */
    void open() const;

    /**
     * Close the connection to the detector.
     */
    void close() const;

    /**
     * Write to a register.
     *
     * @param address       Register address to write
     * @param value         New register value
     */
    void spiWrite(byte address, byte value) const;

    /**
     * Read from a register.
     *
     * @param address       Register address to read
     * @return Current register value
     */
    byte spiRead(byte address) const;

    /**
     * Read the current noise floor level from the detector, and update the object's
     * state accordingly.
     */
    void updateNoiseFloorLevel();

};

#endif