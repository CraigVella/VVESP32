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
    if ((millis() - this->TIMER_lastTime) >= this->state.updateFrequency) {
      this->evaluateWater();
      this->TIMER_lastTime = millis();
    }
}

String VVeggies::getStateJson() {
    StaticJsonDocument<JSON_OBJECT_SIZE(VVSTATE_SIZE)> json;
    json["waterState"] = this->getState().waterState;
    json["dateTime"] = this->getState().lastReading;
    json["soil"][0]["value"] = this->getState().moistureSensor[0];
    json["soil"][0]["trigger"] = this->getState().moistureSensorTrigger[0];
    json["soil"][1]["value"] = this->getState().moistureSensor[1];
    json["soil"][1]["trigger"] = this->getState().moistureSensorTrigger[1];
    json["battery"] = this->getState().batteryReading;
    json["updateFrequency"] = this->getState().updateFrequency;
    
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
      }
}

void VVeggies::evaluateWater() {
    this->state.lastReading = time(NULL);
    this->state.moistureSensor[0] = analogRead(VVeggies::PIN_SoilSensor1);
    this->state.moistureSensor[1] = analogRead(VVeggies::PIN_SoilSensor2);
    this->state.batteryReading = analogRead(VVeggies::PIN_Battery);
    this->_onStateUpdateCallback();
}

void VVeggies::waterOverride(bool value) {
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

void VVeggies::updateInternalStateFromEEPROM() {
    this->state.updateFrequency = EEPROM.readUShort(EE_ADDR_UPDATEFREQ);
    Serial.println(String("Setting update frequency from EEPROM: ") + state.updateFrequency);
}
