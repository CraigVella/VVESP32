#include "VVeggies.h"
#include <EEPROM.h>

VVeggies::VVeggies(std::function<void(void)> onStateUpdate) : _onStateUpdateCallback(onStateUpdate){
    pinMode(VVeggies::PIN_Water, OUTPUT);
    pinMode(VVeggies::PIN_hBridgeEn, OUTPUT);
    pinMode(VVeggies::PIN_SoilSensor1, INPUT);
    pinMode(VVeggies::PIN_SoilSensor2, INPUT);
    pinMode(VVeggies::PIN_Battery, INPUT);
    adcAttachPin(VVeggies::PIN_SoilSensor1);
    adcAttachPin(VVeggies::PIN_SoilSensor2);
    adcAttachPin(VVeggies::PIN_Battery);
    digitalWrite(VVeggies::PIN_Water, false);
    digitalWrite(VVeggies::PIN_hBridgeEn, false);
}

const VVeggies::VVState& VVeggies::getState() {
    return this->state;
}

void VVeggies::doTasks() {
    uint32_t delta = millis() - this->TIMER_lastIndependantTime;
    this->evaluateWater(delta);
    if ((millis() - this->TIMER_lastTime) >= this->state.updateFrequency) {
      this->_onStateUpdateCallback();
      this->TIMER_lastTime = millis();
    }
    if (delta)
        this->TIMER_lastIndependantTime = millis();
}

String VVeggies::getStateJson() {
    StaticJsonDocument<JSON_OBJECT_SIZE(VVSTATE_SIZE)> json;
    json["waterState"] = this->getState().waterState;
    json["inOverride"] = this->getState().inOverride;
    json["maxWateringTime"] = this->getState().maxWateringTime;
    json["maxOverrideTime"] = this->getState().maxOverrideTime;
    json["timeWatering"] = this->getState().timeWatering;
    json["dateTime"] = this->getState().lastReading;
    json["soil"][0]["value"] = this->getState().moistureSensor[0];
    json["soil"][0]["trigger"] = this->getState().moistureSensorTrigger[0];
    json["soil"][1]["value"] = this->getState().moistureSensor[1];
    json["soil"][1]["trigger"] = this->getState().moistureSensorTrigger[1];
    json["battery"] = this->getState().batteryReading;
    json["updateFrequency"] = this->getState().updateFrequency;
    json["triggersEnabled"] = this->getState().triggersEnabled;
    json["triggerHoldTime"] = this->getState().triggerHoldTime;
    json["triggerHeld"] = this->getState().triggerHeld;
    
    unsigned long jsonSize = measureJson(json)+1;
    char * buff = (char *) malloc(jsonSize);
    serializeJson(json,buff,jsonSize);
    String ret(buff);
    free(buff);
    return ret;
}

void VVeggies::processCommand(const char * command, const char * arg) {
    if (strcmp("updateFrequency",command) == 0) {
        Serial.println(String("Updating updateFrequency to ") + arg);
        this->setUpdateFrequency(strtoul(arg,NULL,10));
      } else if (strcmp("waterOverride",command) == 0) {
        Serial.println("Water override recieved");
        this->waterOverride((arg[0] == '0') ? false : true);
      } else if (strcmp("setSoilTrigger0",command) == 0) {
        Serial.println(String("Setting Soil Trigger 0 to ") + arg);
        this->setSoilTrigger(0,strtoul(arg,NULL,10));
      } else if (strcmp("setSoilTrigger1",command) == 0) {
        Serial.println(String("Setting Soil Trigger 1 to ") + arg);
        this->setSoilTrigger(1,strtoul(arg,NULL,10));
      } else if (strcmp("setTriggersEnabled",command) == 0) {
        Serial.println(String("Setting Triggers enabled to ") + arg);
        this->setTriggersEnabled((arg[0] == '0') ? false : true);
      } else if (strcmp("setMaxWateringTime",command) == 0) {
        Serial.println(String("Setting Max Watering Time to ") + arg);
        this->setMaxWateringTime(strtoul(arg, NULL, 10));
      } else if (strcmp("setMaxOverrideTime",command) == 0) {
        Serial.println(String("Setting Max Override Time to ") + arg);
        this->setMaxOverrideTime(strtoul(arg, NULL, 10));
      } else if (strcmp("setTriggerHoldTime",command) == 0) {
        Serial.println(String("Setting Trigger Hold Time to: ") + arg);
        this->setTriggerHoldTime(strtoul(arg, NULL, 10));
      }
}

void VVeggies::setTriggersEnabled(bool enabled) {
    bool en = EEPROM.readBool(EE_ADDR_TRIGEN);
    if (en != enabled) {
        EEPROM.writeBool(EE_ADDR_TRIGEN, enabled);
        EEPROM.commit();
        this->state.triggersEnabled = enabled;
        this->_onStateUpdateCallback();
    }
}

void VVeggies::evaluateWater(unsigned long delta) {
    this->state.lastReading = time(NULL);
    this->state.moistureSensor[0] = analogRead(VVeggies::PIN_SoilSensor1);
    this->state.moistureSensor[1] = analogRead(VVeggies::PIN_SoilSensor2);
    this->state.batteryReading = analogRead(VVeggies::PIN_Battery);
    if (this->getState().waterState) {
        this->state.timeWatering += delta;
    } else {
        this->state.timeWatering = 0;
    }
    if (this->getState().triggersEnabled && !this->getState().inOverride) {
        if ((this->getState().moistureSensor[0] >= this->getState().moistureSensorTrigger[0]) ||
            (this->getState().moistureSensor[1] >= this->getState().moistureSensorTrigger[1])) {
            this->state.triggerHeld += delta;
        } else {
            this->state.triggerHeld = 0;
        }
        if ((this->getState().triggerHeld >= this->getState().triggerHoldTime) && 
            (this->getState().timeWatering < this->getState().maxWateringTime)) {
            if (!this->getState().waterState) 
                this->turnOnWater(true);
        } else {
            if ((this->getState().timeWatering >= this->getState().maxWateringTime)) {
                this->setTriggersEnabled(false); // Shut off triggers, something is wrong, the max watering time shoudln't be 
            }                                    // hit if the sensors are working correctly
            if (this->getState().waterState)
                this->turnOnWater(false);
        }
    } else {
        this->state.triggerHeld = 0;
    }
    if (this->getState().inOverride && this->getState().waterState) {
        if (this->getState().timeWatering >= this->getState().maxOverrideTime) {
            this->waterOverride(false); // end Override
        }
    }
}

void VVeggies::waterOverride(bool value) {
    this->state.triggerHeld = 0;
    this->state.inOverride = value;
    this->turnOnWater(value);
}

void VVeggies::turnOnWater(bool value) {
    this->state.waterState = value;
    digitalWrite(VVeggies::PIN_hBridgeEn, value);
    digitalWrite(VVeggies::PIN_Water, value);
    this->_onStateUpdateCallback();
}

void VVeggies::setUpdateFrequency(uint16_t freq){
    uint16_t val = EEPROM.readUShort(EE_ADDR_UPDATEFREQ);
    if (val != freq) {
        EEPROM.writeUShort(EE_ADDR_UPDATEFREQ,freq);
        EEPROM.commit();
        this->state.updateFrequency = freq;
        this->_onStateUpdateCallback();
    }
}

void VVeggies::setTriggerHoldTime(unsigned long trigHT) {
    uint32_t val = EEPROM.readULong(EE_ADDR_TRIGHT);
    if (val != trigHT) {
        EEPROM.writeULong(EE_ADDR_TRIGHT, trigHT);
        EEPROM.commit();
        this->state.triggerHoldTime = trigHT;
        this->_onStateUpdateCallback();
    }
}

void VVeggies::setMaxWateringTime(unsigned long maxWT){
    uint32_t val = EEPROM.readUShort(EE_ADDR_MAXWT);
    if (val != maxWT) {
        EEPROM.writeULong(EE_ADDR_MAXWT,maxWT);
        EEPROM.commit();
        this->state.maxWateringTime = maxWT;
        this->_onStateUpdateCallback();
    }
}

void VVeggies::setMaxOverrideTime(unsigned long maxOT){
    uint32_t val = EEPROM.readUShort(EE_ADDR_MAXOT);
    if (val != maxOT) {
        EEPROM.writeULong(EE_ADDR_MAXOT,maxOT);
        EEPROM.commit();
        this->state.maxOverrideTime = maxOT;
        this->_onStateUpdateCallback();
    }
}

void VVeggies::setSoilTrigger(uint8_t number, uint16_t value) {
    uint16_t val = number ? EEPROM.readUShort(EE_ADDR_ST1TRIG) : EEPROM.readUShort(EE_ADDR_ST0TRIG);
    if (val != value) {
        EEPROM.writeUShort(number ? EE_ADDR_ST1TRIG : EE_ADDR_ST0TRIG, value);
        EEPROM.commit();
        this->state.moistureSensorTrigger[number] = value;
        this->_onStateUpdateCallback();
    }
}

void VVeggies::updateInternalStateFromEEPROM() {
    this->state.updateFrequency = EEPROM.readUShort(EE_ADDR_UPDATEFREQ);
    Serial.println(String("Setting update frequency from EEPROM: ") + state.updateFrequency);
    this->state.moistureSensorTrigger[0] = EEPROM.readUShort(EE_ADDR_ST0TRIG);
    this->state.moistureSensorTrigger[1] = EEPROM.readUShort(EE_ADDR_ST1TRIG);
    Serial.println(String("Set soil trigger 0,1 : ") + String(this->state.moistureSensorTrigger[0]) + String(",") +  this->state.moistureSensorTrigger[1]);
    this->state.triggersEnabled = EEPROM.readBool(EE_ADDR_TRIGEN);
    Serial.println(String("Set Triggers Enabled to: ") + this->state.triggersEnabled);
    this->state.maxWateringTime = EEPROM.readULong(EE_ADDR_MAXWT);
    Serial.println(String("Set Max Watering Time to: ") + this->state.maxWateringTime);
    this->state.maxOverrideTime = EEPROM.readULong(EE_ADDR_MAXOT);
    Serial.println(String("Set Max Override Time to: ") + this->state.maxOverrideTime);
    this->state.triggerHoldTime = EEPROM.readULong(EE_ADDR_TRIGHT);
    Serial.println(String("Set Trigger Hold Time to: ") + this->state.triggerHoldTime);
}
