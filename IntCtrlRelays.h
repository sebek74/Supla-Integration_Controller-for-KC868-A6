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
  LightTriggerFromIntegra = 2,
  AlarmArmament = 3,
  FrontDoorBell = 4,
  HoodSensor = 5,
  VAlarmBlockedInternalSensors = 6
};

enum RelayId {
  _FirstRelay = 0,
  VideoRecorderAlarm = 0,
  SchedDoorSensorAlarm = 1,
  AirVent_HoodMode = 2,
  FrontDoorOpen = 3,
  GateOpen = 4,
  GarageOpen = 5,
  VHeaters2h = 6,
  VHeaters4h = 7,
  VHeaters8h = 8,
  VAlarmLights = 9,
  VRestoreDisplay1 = 10,
  VRestoreDisplay2 = 11, 
  _LastRelay = 11
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
  TurnLightsOn,
  TurnLightsOff,
  BlockInternalSensors,
  OpenGate,
  OpenFrontDoor
};

#define MAX_SHARED_RELAYS 8

class IntCtrlActionHandler;

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
      return true;
    return actualValue;
  }
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
  static bool IsRestoreDisplayRelayActive() { return relay[RelayId::VRestoreDisplay1]->isOn() || relay[RelayId::VRestoreDisplay2]->isOn(); };
  static bool IsAlarmActive() { return alarmTriggerFromIntegra->getValue(); };
  static bool IsVideoRecorderAlarmActive() { return relay[RelayId::VideoRecorderAlarm]->isOn(); };
  static bool IsAlarmLightsOn() { return relay[RelayId::VAlarmLights]->isOn(); };
  static bool IsHoodActive() { return hoodSensor && hoodSensor->getValue(); };
  static bool IsAirVentHoodModeActive() { return relay[AirVent_HoodMode]->isOn(); };
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