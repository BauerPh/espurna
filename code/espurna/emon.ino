/*

EMON MODULE

Copyright (C) 2016-2017 by Xose Pérez <xose dot perez at gmail dot com>

*/

#if ENABLE_EMON

#include <EmonLiteESP.h>
#include "brzo_i2c.h"
#include <EEPROM.h>

// ADC121 Registers
#define ADC121_REG_RESULT       0x00
#define ADC121_REG_ALERT        0x01
#define ADC121_REG_CONFIG       0x02
#define ADC121_REG_LIMITL       0x03
#define ADC121_REG_LIMITH       0x04
#define ADC121_REG_HYST         0x05
#define ADC121_REG_CONVL        0x06
#define ADC121_REG_CONVH        0x07

EmonLiteESP emon;
double _current = 0;
unsigned int _power = 0;
unsigned char _emonGPIO;

// -----------------------------------------------------------------------------
// Provider
// -----------------------------------------------------------------------------

unsigned int currentCallback() {

    #if EMON_PROVIDER == EMON_ANALOG_PROVIDER
        return analogRead(EMON_CURRENT_PIN);
    #endif

    #if EMON_PROVIDER == EMON_ADC121_PROVIDER
        uint8_t buffer[2];
        brzo_i2c_start_transaction(EMON_ADC121_ADDRESS, I2C_SCL_FREQUENCY);
        buffer[0] = ADC121_REG_RESULT;
        brzo_i2c_write(buffer, 1, false);
        brzo_i2c_read(buffer, 2, false);
        brzo_i2c_end_transaction();
        unsigned int value;
        value = (buffer[0] & 0x0F) << 8;
        value |= buffer[1];
        return value;
    #endif

}

// -----------------------------------------------------------------------------
// EMON
// -----------------------------------------------------------------------------

void setCurrentRatio(float value) {
    emon.setCurrentRatio(value);
}

unsigned int getPower() {
    return _power;
}

double getCurrent() {
    return _current;
}

void powerMonitorSetup() {

    // backwards compatibility
    moveSetting("pwMainsVoltage", "emonMains");
    moveSetting("pwCurrentRatio", "emonRatio");
    saveSettings();

    _emonGPIO = getSetting("emonGPIO", EMON_CURRENT_PIN).toInt();

    emon.initCurrent(
        currentCallback,
        EMON_ADC_BITS,
        EMON_REFERENCE_VOLTAGE,
        getSetting("emonRatio", EMON_CURRENT_RATIO).toFloat()
    );
    emon.setPrecision(EMON_CURRENT_PRECISION);

    #if EMON_PROVIDER == EMON_ADC121_PROVIDER
        uint8_t buffer[2];
        buffer[0] = ADC121_REG_CONFIG;
        buffer[1] = 0x00;
        brzo_i2c_start_transaction(EMON_ADC121_ADDRESS, I2C_SCL_FREQUENCY);
        brzo_i2c_write(buffer, 2, false);
        brzo_i2c_end_transaction();
    #endif

    apiRegister("/api/power", "power", [](char * buffer, size_t len) {
        snprintf(buffer, len, "%d", _power);
    });

}

void powerMonitorLoop() {

    static unsigned long next_measurement = millis();
    static bool warmup = true;
    static byte measurements = 0;
    static double max = 0;
    static double min = 0;
    static double sum = 0;

    if (!mqttConnected()) return;

    if (warmup) {
        warmup = false;
        emon.warmup();
    }

    if (millis() > next_measurement) {

        // Safety check: do not read current if relay is OFF
        // You could be monitoring another line with the current clamp...
        //if (!relayStatus(0)) {
        //    _current = 0;
        //} else {
            _current = emon.getCurrent(EMON_SAMPLES);
            _current -= EMON_CURRENT_OFFSET;
            if (_current < 0) _current = 0;
        //}

        if (measurements == 0) {
            max = min = _current;
        } else {
            if (_current > max) max = _current;
            if (_current < min) min = _current;
        }
        sum += _current;
        ++measurements;

        float mainsVoltage = getSetting("emonMains", EMON_MAINS_VOLTAGE).toFloat();

        char current[6];
        dtostrf(_current, 5, 2, current);
        DEBUG_MSG("[ENERGY] Current: %sA\n", current);
        DEBUG_MSG("[ENERGY] Power: %dW\n", int(_current * mainsVoltage));

        // Update websocket clients
        char text[64];
        sprintf_P(text, PSTR("{\"emonVisible\": 1, \"powApparentPower\": %d}"), int(_current * mainsVoltage));
        wsSend(text);

        // Send MQTT messages averaged every EMON_MEASUREMENTS
        if (measurements == EMON_MEASUREMENTS) {

            _power = (int) ((sum - max - min) * mainsVoltage / (measurements - 2));
            char power[6];
            snprintf(power, 6, "%d", _power);

            double energy_inc = (double) _power * EMON_INTERVAL * EMON_MEASUREMENTS / 1000.0 / 3600.0;
            char energy_buf[10];
            dtostrf(energy_inc, 9, 2, energy_buf);
            char *e = energy_buf;
            while ((unsigned char) *e == ' ') ++e;

            mqttSend(getSetting("emonPowerTopic", EMON_APOWER_TOPIC).c_str(), power);
            mqttSend(getSetting("emonEnergyTopic", EMON_ENERGY_TOPIC).c_str(), e);

            #if ENABLE_DOMOTICZ
            {
                char buffer[20];
                snprintf(buffer, 20, "%s;%s", power, e);
                domoticzSend("dczPowIdx", 0, buffer);
            }
            #endif

            sum = measurements = 0;

        }

        next_measurement += EMON_INTERVAL;

    }

}

#endif
