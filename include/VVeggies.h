#include <Arduino.h>
#include <ArduinoJson.h>
#include <functional>

#ifndef __VVEGGIES__
#define __VVEGGIES__

#define EE_ADDR_UPDATEFREQ  0x00
#define EE_ADDR_MS1TRIG     0x02
#define EE_ADDR_MS2TRIG     0x04

#define VVSTATE_SIZE        10

class VVeggies {
    protected:
    static const uint8_t PIN_Water        = 2;
    static const uint8_t PIN_hBridgeEn    = 0;
    static const uint8_t PIN_SoilSensor1  = 34;
    static const uint8_t PIN_SoilSensor2  = 35;
    static const uint8_t PIN_Battery      = 39;

    std::function<void(void)> _onStateUpdateCallback;

    struct VVState {
        time_t lastReading                = 0;
        bool waterState                   = false;
        uint16_t moistureSensor[2]        = {};
        uint16_t moistureSensorTrigger[2] = {};
        uint16_t batteryReading           = 0;
        uint16_t updateFrequency          = 0;
    } state;

    unsigned long TIMER_lastTime = 0;

    public:
    VVeggies(std::function<void(void)> onStateUpdate);
    const VVState& getState(void);
    String getStateJson(void);
    void doTasks(void);
    void processCommand(const char * command, const char * arg);
    void evaluateWater();
    void waterOverride(bool value);
    void turnOnWater(bool value);
    void setUpdateFrequency(uint16_t freq);
    void updateInternalStateFromEEPROM(void);
};

#endif