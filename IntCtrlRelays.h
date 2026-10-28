#ifndef INTCTRLRELAYS_H
#define INTCTRLRELAYS_H
#pragma once

#include <supla/sensor/binary.h>
#include <supla/control/relay.h>
#include <supla/control/virtual_relay.h>
#include <supla/sensor/virtual_binary.h>
#include <supla/control/button.h>
#include <supla/io/PCF8574.h>
#include "Definitions.h"


enum InputId {
  AlarmTriggerFromVideoRecorder = 0,
  AlarmTriggerFromIntegra = 1,
  LightTriggerFromIntegra = 2,      // fizyczny sensor, ustawia sensory wirtualne 6,7,8
  AlarmArmament = 3,
  FrontDoorBell = 4,
  HoodSensor = 5,
  VAlarmBlockedInternalSensors = 6,
  VLightOn = 7,
  VLightOff = 8,
  BoostModeButton = 9,
};

enum RelayId {
  _FirstRelay = 0,
  VideoRecorderAlarm = 0,
  SchedDoorSensorAlarm = 1, // trzeba ustawić przekaźnik przy zmianie wartości czujnika drzwi w szopie
  AirVent_HoodMode = 2, // tryb okapu centrali wentylacyjnej
  FrontDoorOpen = 3,  // furtka
  GateOpen = 4,       // brama wjazdowa
  GarageOpen = 5,         // wrota 2
  VHeaters2h = 6,       // pompa obiegu wysokotemperaturowego grzejników na 2h
  VHeaters4h = 7,
  VHeaters8h = 8,
  VAlarmLights = 9,       // alarm świateł zewnętrznych
  VRestoreDisplay1 = 10,  // wirtualny przełącznik do ustawienia przez włączenie światła w garażu - włącza ekran
  VRestoreDisplay2 = 11,  // wirtualny przełącznik do ustawienia przez włączenie drugiego światła w garażu - włącza ekran
  AirVent_BoostMode = 12, // tryb przewietrzania centrali wentylacyjnej
  AirVent_HumidityExceeded1 = 13,   // włącznik dla miernika wilgotności 1
  AirVent_HumidityExceeded2 = 14,   // włącznik dla miernika wilgotności 2
  AirVent_AirQualityLow = 15, // włącznik dla miernika jakości powietrza
  _LastRelay = 15
};

enum ActionId {
  RestoreDisplay,
  VideoAlarmOn,
  VideoAlarmOff,
  IntegraAlarmOn,
  IntegraAlarmOff,
  IntegraArmed,
  IntegraDisarmed,
  HoodOn,
  HoodOff,
  DoorBell,
  Heaters2h,
  Heaters4h,
  Heaters8h,
  OpenGate,
  OpenFrontDoor,
  LightTimeCheckOn,
  LightTimeCheckOff,
  ManualBoostMode,
  HumidityExceeded1,
  HumidityNominal1,
  HumidityExceeded2,
  HumidityNominal2,
  AirConditionLow,
  AirConditionNominal
};

#define MAX_SHARED_RELAYS 8

class IntCtrlActionHandler;
class IntCtrlRepeatedRelay;

class TimedBinarySensor : public Supla::Sensor::Binary {
  private: 
    int32_t holdTimeS = 0;
    int32_t startTimeMs = 0;
    bool previousValue = false;
  public:
    TimedBinarySensor(int32_t time,
                  Supla::Io::Base *io,
                  int pin,
                  bool pullUp = false,
                  bool invertLogic = false) : Supla::Sensor::Binary(io, pin, pullUp, invertLogic) { holdTimeS = time; };

  bool getValue() override {
    bool actualValue = Supla::Sensor::Binary::getValue();
    if (actualValue && !previousValue)
      startTimeMs = millis();
    previousValue = actualValue;
    if (startTimeMs>0 && startTimeMs+holdTimeS*1000>millis())
      return false;
    return actualValue;
  }
};
/*
class TimedVirtualSensor : public Supla::Sensor::VirtualBinary {
  private: 
    int32_t holdTimeS = 0;
    int32_t startTimeMs = 0;
    bool previousValue = false;
  public:
    TimedVirtualSensor(int32_t time) : holdTimeS(time) {};

  bool getValue() override {
    bool actualValue = Supla::Sensor::VirtualBinary::getValue();
    if (actualValue && !previousValue)
      startTimeMs = millis();
    previousValue = actualValue;
    if (startTimeMs>0 && startTimeMs+holdTimeS*1000>millis())
      return false;
    return actualValue;
  }
};*/


class IntCtrlRepeatedRelay : public Supla::Control::Relay {
  private:
    uint32_t startTime = 0;
    uint32_t pauseTime = 0;
    uint32_t countDownTime = 0;
    const uint32_t repeatInterval = 10*60*1000-10000; // powtórz załączanie i odłączanie przed upływem 10 minut (po 10 min rekuperator sam zmienia bieg mimo włączonego trybu przewietrzania)
    const uint32_t pauseInterval = 10*1000;  // miękki czas przerwy - 10 sekund
    bool paused = false;
  
  public:
    IntCtrlRepeatedRelay(int pin, bool highIsOn = true) : Supla::Control::Relay(pin, highIsOn) {};
  
    bool isPaused() { return !isOn() && paused; };
    
    bool isOnOrPaused() { return isOn() || (!isOn() && paused);};
    uint32_t getAirBoostRemainingTime() { 
      if (!isOnOrPaused()) return 0;
      uint32_t remainingTime = 0;
      if (!paused)
        getRemainingCountdownTimerSec(&remainingTime);
      else
        remainingTime = countDownTime;
      return remainingTime;
    };
    void iterateAlways() override {
      Supla::Control::Relay::iterateAlways();
      int32_t actualTime = millis();
      if (isOn() && !paused && actualTime>startTime+repeatInterval) {  
        countDownTime = 0;
        getRemainingCountdownTimerSec(&countDownTime);
        if (countDownTime*1000<pauseInterval+10000) return;
        paused = true;
        pauseTime = actualTime;
        turnOff();
      }
      if (!isOn() && paused && actualTime>pauseTime+pauseInterval) {
        paused = false;
        turnOn(countDownTime*1000);
      }
    };

    void turnOn(_supla_int_t duration = 0) override {
      startTime = millis();
      Supla::Control::Relay::turnOn(duration);
    };
};

class IntCtrlRelay : public Supla::Control::Relay {
  
protected:
  friend class IntCtrlActionHandler;
  inline static bool relaysInitialized = false;
  int myRelayId = 0;
  inline static Supla::Sensor::BinaryBase* hoodSensor = nullptr;
  inline static TimedBinarySensor* frontDoorBellSensor = nullptr;
  inline static TimedBinarySensor* alarmTriggerFromVideoRecorder = nullptr;
  inline static Supla::Sensor::Binary* alarmTriggerFromIntegra = nullptr;
  inline static Supla::Sensor::Binary* alarmArmedSensor = nullptr;
  inline static Supla::Sensor::Binary* lightTriggerSensor = nullptr;
  inline static Supla::Sensor::VirtualBinary* lightsOnSensor = nullptr; 
  inline static Supla::Sensor::VirtualBinary* lightsOffSensor = nullptr; 
  inline static Supla::Sensor::VirtualBinary* alarmArmedBlockedInternalSensors = nullptr;
  inline static Supla::Io::PCF8574 *inPcf = new Supla::Io::PCF8574(PCF_INPUTS_ADDR);
  inline static Supla::Io::PCF8574 *outPcf = new Supla::Io::PCF8574(PCF_RELAYS_ADDR);
  inline static Supla::Control::Relay *relay[RelayId::_LastRelay+1] = {};
  inline static int32_t hoodModeStart = 0;
public:
  static void RegisterRelays(); 
  static void InitalizeRelays();
  static void InitializeConfigButton();
  static void Iterate();

  explicit IntCtrlRelay(int pin, int relayId) : Supla::Control::Relay(outPcf, pin, false) {
      //getChannel()->setDefaultFunction(SUPLA_CHANNELFNC_STAIRCASETIMER);
      getChannel()->setDefaultIcon(1);
      myRelayId = relayId;
  };
  static Supla::Control::Relay* GetRelayById(int relayId) {return relay[relayId]; };
  static IntCtrlRepeatedRelay* GetBoostRelay() { return static_cast<IntCtrlRepeatedRelay*>(relay[RelayId::AirVent_BoostMode]); };
  static bool IsRestoreDisplayRelayActive() { return relay[RelayId::VRestoreDisplay1]->isOn() || relay[RelayId::VRestoreDisplay2]->isOn(); };
  static bool IsAlarmActive() { return alarmTriggerFromIntegra->getValue(); };
  static bool IsVideoRecorderAlarmActive() { return relay[RelayId::VideoRecorderAlarm]->isOn(); };
  static bool IsAlarmLightsOn() { return relay[RelayId::VAlarmLights]->isOn(); };
  static bool IsHoodActive() { return hoodSensor && hoodSensor->getValue(); };
  static bool IsAirVentHoodModeActive() { return relay[AirVent_HoodMode]->isOn(); };
  static bool IsAirVentBoostModeActive() { return GetBoostRelay()->isOnOrPaused(); };
  static bool IsHeaterActive() { return relay[VHeaters2h]->isOn() || relay[VHeaters4h]->isOn() || relay[VHeaters8h]->isOn(); };
  static bool GetHeaterTimeRemainingS(uint32_t& result) {
      if (relay[VHeaters8h]->getRemainingCountdownTimerSec(&result)) return true;
      if (relay[VHeaters2h]->getRemainingCountdownTimerSec(&result)) return true;
      if (relay[VHeaters4h]->getRemainingCountdownTimerSec(&result)) return true;
      return false;
  }
  static bool IsAlarmArmed() { return alarmArmedSensor->getValue(); };
  static bool IsAlarmInternalSensorsBlocked() { return alarmArmedBlockedInternalSensors->getValue(); };
  static bool IsBellRinging() { return frontDoorBellSensor->getValue(); };
  static bool IsSchedDoorOpen() { return relay[SchedDoorSensorAlarm]->isOn(); };

};

#endif //INTCTRLRELAYS_H