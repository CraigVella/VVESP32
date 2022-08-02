#include <Arduino.h>
#include <ArduinoJson.h>
#include <functional>

#ifndef __VVEGGIES__
#define __VVEGGIES__

#define EE_ADDR_UPDATEFREQ  0x00
#define EE_ADDR_ST0TRIG     0x02
#define EE_ADDR_ST1TRIG     0x04
#define EE_ADDR_TRIGEN      0x06
#define EE_ADDR_MAXWT       0x07
#define EE_ADDR_MAXOT       0x0B
#define EE_ADDR_TRIGHT      0x0F

#define VVSTATE_SIZE        18

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
        bool inOverride                   = false;
        unsigned long timeWatering        = 0;
        unsigned long maxWateringTime     = 0;
        unsigned long maxOverrideTime     = 0;
        unsigned long triggerHoldTime     = 0;
        unsigned long triggerHeld         = 0;
        uint16_t moistureSensor[2]        = {};
        uint16_t moistureSensorTrigger[2] = {};
        uint16_t batteryReading           = 0;
        uint16_t updateFrequency          = 0;
        bool triggersEnabled              = true;
    } state;

    unsigned long TIMER_lastTime = 0;
    unsigned long TIMER_lastIndependantTime = 0;

    void evaluateWater(unsigned long delta);

    public:
    VVeggies(std::function<void(void)> onStateUpdate);
    const VVState& getState(void);
    String getStateJson(void);
    void doTasks(void);
    void processCommand(const char * command, const char * arg);
    void waterOverride(bool value);
    void turnOnWater(bool value);
    void setUpdateFrequency(uint16_t freq);
    void updateInternalStateFromEEPROM(void);
    void setSoilTrigger(uint8_t number, uint16_t value);
    void setTriggersEnabled(bool enabled);
    void setMaxWateringTime(unsigned long maxWT);
    void setMaxOverrideTime(unsigned long maxOT);
    void setTriggerHoldTime(unsigned long trigHT);
};

#endif