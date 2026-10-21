#include <SuplaDevice.h>
#include <Wire.h>
#include <supla/control/button.h>
#include <supla/control/relay.h>
#include <supla/control/virtual_relay.h>
#include <supla/sensor/binary.h>
#include <supla/sensor/virtual_binary.h>
#include <supla/events.h>
#include <supla/control/button.h>
#include <supla/control/action_trigger.h>
#include "Definitions.h"
#include "IntCtrlRelays.h"
#include "IntCtrlConfig.h"
#include "IntCtrlDisplay.h"
#include "IntCtrlMessenger.h"

extern IntCtrlDisplay display;
extern IntCtrlConfig config;
extern IntCtrlMessenger messenger;

const char* relayNames [] {
  "Alarm z rejestratora",
  "Otwarte drzwi szopy",
  "Rekuperator - tryb okap",
  "Furtka",
  "Brama",
  "Garaż",
  "Grzejnik łazienkowe - 2h",
  "Grzejnik łazienkowe - 4h",
  "Grzejnik łazienkowe - 8h",
  "Alarm świateł",
  "Włącz wyświetlacz 1",
  "Włącz wyświetlacz 2" 
};

const char* sensorName [] {
  "Alarm z rejestartora",
  "Czuwanie alarmu",
  "Dzwonek do furtki",
  "Czujnik włączenia okapu",
  "Światła z Integry",
  "Alarm z Integry",
  "Blokada wewnętrznych czujników"
};

extern int32_t frameId;

class HipHopBinary : public Supla::Sensor::VirtualBinary {
  public:
  bool getValue() override {
    return frameId%50>25;
  };
};

class ProtectedRelay : public Supla::Control::Relay {
  public:
    ProtectedRelay(int pin, bool highIsOn = true) : Supla::Control::Relay(pin, highIsOn) {};
    virtual void turnOn(_supla_int_t duration = 0) override {
      if (IntCtrlRelay::IsAlarmArmed()) {
        messenger.sendMessage(MSG_ALARM_GARAGE_LOCKED, MSG_TITLE_ALARM);
        return;
      }
      display.setupAction(ActionIconId::GARAGE_DOOR_OPEN);
      Supla::Control::Relay::turnOn();
    }
};

class IntCtrlActionHandler : public Supla::ActionHandler {
  public:
    void handleAction(int event, int action) override {
      SUPLA_LOG_DEBUG("action event: %d %d", event, action);
      time_t now = time(nullptr);
      struct tm* timeinfo = localtime(&now);
      display.restoreFullContrast();
      switch (action) {
        case ActionId::VideoAlarmOn:
          IntCtrlRelay::GetRelayById(RelayId::VideoRecorderAlarm)->turnOn();
          IntCtrlRelay::GetRelayById(RelayId::VAlarmLights)->turnOn(ALARM_LIGHTS_TIME_S*1000);
          messenger.sendMessage(MSG_VIDEORECORDER_ALARM_TRIGGER, MSG_TITLE_ALARM);
          break;
        case ActionId::VideoAlarmOff:
          IntCtrlRelay::GetRelayById(RelayId::VideoRecorderAlarm)->turnOff();
          break;
        case ActionId::IntegraAlarmOn:
          IntCtrlRelay::GetRelayById(RelayId::VAlarmLights)->turnOn(ALARM_LIGHTS_TIME_S*1000);
          messenger.sendMessage(MSG_INTEGRA_ALARM_TRIGGER, MSG_TITLE_ALARM);
          break;
        case ActionId::IntegraAlarmOff:
          IntCtrlRelay::GetRelayById(RelayId::VAlarmLights)->turnOff();
          break;
        case ActionId::IntegraArmed:
          messenger.sendMessage(MSG_ALARM_ARMED, MSG_TITLE_ALARM, timeinfo->tm_hour, timeinfo->tm_min);
          IntCtrlRelay::alarmArmedBlockedInternalSensors->clear();
          break;
        case ActionId::IntegraDisarmed:
          messenger.sendMessage(MSG_ALARM_DISARMED, MSG_TITLE_ALARM, timeinfo->tm_hour, timeinfo->tm_min);
          IntCtrlRelay::alarmArmedBlockedInternalSensors->clear();
          break;
        case ActionId::BlockInternalSensors:
          IntCtrlRelay::alarmArmedBlockedInternalSensors->set();
          break;
        case ActionId::DoorBell:
          messenger.sendMessage(MSG_GATE_BELL, MSG_TITLE_DOORSTATION, timeinfo->tm_hour, timeinfo->tm_min);
          break;
        case HoodOn:
          IntCtrlRelay::GetRelayById(RelayId::AirVent_HoodMode)->turnOn();
          IntCtrlRelay::hoodModeStart = millis();
          break;
        case HoodOff:
          IntCtrlRelay::GetRelayById(RelayId::AirVent_HoodMode)->turnOff();
          IntCtrlRelay::hoodModeStart = 0;
          break;
         case TurnLightsOn:
          display.setupAction(ActionIconId::LIGHTS_ON);
          break;
        case TurnLightsOff:
          display.setupAction(ActionIconId::LIGHTS_OFF);
          break;
        case OpenGate:
          display.setupAction(ActionIconId::GATE_OPEN);
          break;
        case OpenFrontDoor:
          display.setupAction(ActionIconId::FRONT_DOOR_OPEN);
          break;
        case RestoreDisplay:
          break;
      }
    }
} intCtrlActionHandler;

class IntCtrlSharedRelay :  public /*Supla::Control::Relay*/IntCtrlRelay {
  private:
    inline static int sharedRelaysAmount=0;
    inline static IntCtrlSharedRelay* sharedRelays[MAX_SHARED_RELAYS] = {};
    int timeOn;
  public:
    IntCtrlSharedRelay(int pin, int relayId, int time) : /*Supla::Control::Relay(2, true) */IntCtrlRelay(pin, relayId) {
      timeOn = time;
      if (sharedRelaysAmount==MAX_SHARED_RELAYS) return;
      sharedRelays[sharedRelaysAmount] = this;
      sharedRelaysAmount++;
    }
    void turnOn(_supla_int_t duration = 0) override {
      for (int i=0; i<sharedRelaysAmount; i++)
        if (sharedRelays[i] != this)
          sharedRelays[i]->turnOff();
      Supla::Control::Relay::turnOn(timeOn);
    }
    void turnOff(_supla_int_t duration = 0) override {
      uint32_t remainingTime;
      if (getRemainingCountdownTimerSec(&remainingTime) && remainingTime<5) {
        messenger.sendMessage(MSG_HEATERS_OFF, MSG_TITLE_HEATERS, timeOn/60/60);
      }
      Supla::Control::Relay::turnOff(duration);
    }
};

void IntCtrlRelay::RegisterRelays() {

  auto button = new Supla::Control::Button(BUTTON_CFG_RELAY_GPIO, true, true);
  button->configureAsConfigButton(&SuplaDevice);

  relay[RelayId::VideoRecorderAlarm] = /*new Supla::Control::VirtualRelay();*/ new IntCtrlRelay(0, VideoRecorderAlarm);
  relay[RelayId::VideoRecorderAlarm]->getChannel()->setDefault(SUPLA_CHANNELFNC_POWERSWITCH);
  relay[RelayId::SchedDoorSensorAlarm] = /*new Supla::Control::VirtualRelay();*/ new IntCtrlRelay(1, SchedDoorSensorAlarm);
  relay[RelayId::SchedDoorSensorAlarm]->getChannel()->setDefault(SUPLA_CHANNELFNC_POWERSWITCH);
  relay[RelayId::AirVent_HoodMode] = /*new Supla::Control::VirtualRelay();*/ new IntCtrlRelay(3, AirVent_HoodMode);
  relay[RelayId::AirVent_HoodMode]->getChannel()->setDefault(SUPLA_CHANNELFNC_POWERSWITCH);
  relay[RelayId::FrontDoorOpen] = /*new Supla::Control::VirtualRelay(); */ new IntCtrlRelay(4, FrontDoorOpen);
  relay[RelayId::FrontDoorOpen]->getChannel()->setDefault(SUPLA_CHANNELFNC_CONTROLLINGTHEGATEWAYLOCK);
  relay[RelayId::GateOpen] = /*new Supla::Control::VirtualRelay(); */new IntCtrlRelay(5, GateOpen);
  relay[RelayId::GateOpen]->getChannel()->setDefault(SUPLA_CHANNELFNC_CONTROLLINGTHEGATE);
  relay[RelayId::GarageOpen] = new ProtectedRelay(EXTRA_GPIO_1, true);/*new Relay(EXTRA_GPIO_1, false);*/
  relay[RelayId::GarageOpen]->getChannel()->setDefault(SUPLA_CHANNELFNC_CONTROLLINGTHEGARAGEDOOR);
  relay[RelayId::VHeaters2h] = new IntCtrlSharedRelay(2, VHeaters2h, 2*60*60*1000);
  relay[RelayId::VHeaters2h]->getChannel()->setDefault(SUPLA_CHANNELFNC_POWERSWITCH);
  relay[RelayId::VHeaters4h] = new IntCtrlSharedRelay(2, VHeaters4h, 4*60*60*1000);
  relay[RelayId::VHeaters4h]->getChannel()->setDefault(SUPLA_CHANNELFNC_POWERSWITCH);
  relay[RelayId::VHeaters8h] = new IntCtrlSharedRelay(2, VHeaters8h, 8*60*60*1000);
  relay[RelayId::VHeaters8h]->getChannel()->setDefault(SUPLA_CHANNELFNC_POWERSWITCH);
  relay[RelayId::VAlarmLights] = new Supla::Control::VirtualRelay();
  relay[RelayId::VAlarmLights]->getChannel()->setDefault(SUPLA_CHANNELFNC_POWERSWITCH);
  relay[RelayId::VRestoreDisplay1] = new Supla::Control::VirtualRelay();
  relay[RelayId::VRestoreDisplay1]->getChannel()->setDefault(SUPLA_CHANNELFNC_POWERSWITCH);
  relay[RelayId::VRestoreDisplay2] = new Supla::Control::VirtualRelay();
  relay[RelayId::VRestoreDisplay2]->getChannel()->setDefault(SUPLA_CHANNELFNC_POWERSWITCH);

  for (int i = RelayId::_FirstRelay; i<=RelayId::_LastRelay; i++) {
    //relay[i]->setDefaultStateRestore();
    relay[i]->getChannel()->setChannelNumber(i);
    relay[i]->getChannel()->setDefaultIcon(0);
    relay[i]->setInitialCaption(relayNames[i]);
  }

  #define INVERT_LOGIC false

  hoodSensor = new Supla::Sensor::Binary(inPcf, InputId::HoodSensor, false, INVERT_LOGIC);
  hoodSensor->setInitialCaption(sensorName[InputId::HoodSensor]);
  hoodSensor->getChannel()->setDefault(SUPLA_CHANNELFNC_BINARY_SENSOR);
  hoodSensor->getChannel()->setChannelNumber(RelayId::_LastRelay+1+InputId::HoodSensor);
  hoodSensor->addAction(ActionId::HoodOn, intCtrlActionHandler, Supla::ON_TURN_ON);
  hoodSensor->addAction(ActionId::HoodOff, intCtrlActionHandler, Supla::ON_TURN_OFF);
  hoodSensor->setFilteringTimeMs(1000, true);

  alarmTriggerFromVideoRecorder = new TimedBinarySensor(120, inPcf, InputId::AlarmTriggerFromVideoRecorder, false, INVERT_LOGIC);
  alarmTriggerFromVideoRecorder->setInitialCaption(sensorName[InputId::AlarmTriggerFromVideoRecorder]);
  //sensor->getChannel()->setDefault(SUPLA_CHANNELFNC_ALARM);
  alarmTriggerFromVideoRecorder->getChannel()->setDefault(SUPLA_CHANNELFNC_BINARY_SENSOR);
  alarmTriggerFromVideoRecorder->getChannel()->setChannelNumber(RelayId::_LastRelay+1+InputId::AlarmTriggerFromVideoRecorder);
  alarmTriggerFromVideoRecorder->addAction(ActionId::VideoAlarmOn, intCtrlActionHandler, Supla::ON_TURN_ON);
  alarmTriggerFromVideoRecorder->addAction(ActionId::VideoAlarmOff, intCtrlActionHandler, Supla::ON_TURN_OFF);
  alarmTriggerFromVideoRecorder->setFilteringTimeMs(1000, true);

  alarmTriggerFromIntegra = new Supla::Sensor::Binary(inPcf, InputId::AlarmTriggerFromIntegra, false, INVERT_LOGIC);
  alarmTriggerFromIntegra->setInitialCaption(sensorName[InputId::AlarmTriggerFromIntegra]);
  //sensor->getChannel()->setDefault(SUPLA_CHANNELFNC_ALARM);
  alarmTriggerFromIntegra->getChannel()->setDefault(SUPLA_CHANNELFNC_BINARY_SENSOR);
  alarmTriggerFromIntegra->getChannel()->setChannelNumber(RelayId::_LastRelay+1+InputId::AlarmTriggerFromIntegra);
  alarmTriggerFromIntegra->addAction(ActionId::IntegraAlarmOn, intCtrlActionHandler, Supla::ON_TURN_ON);
  alarmTriggerFromIntegra->addAction(ActionId::IntegraAlarmOff, intCtrlActionHandler, Supla::ON_TURN_OFF);
  alarmTriggerFromIntegra->setFilteringTimeMs(1000, true);

  alarmArmedSensor = new Supla::Sensor::Binary(inPcf, InputId::AlarmArmament, false, INVERT_LOGIC);
  alarmArmedSensor->setInitialCaption(sensorName[InputId::AlarmArmament]);
  alarmArmedSensor->getChannel()->setDefault(SUPLA_CHANNELFNC_ALARMARMAMENTSENSOR);
  alarmArmedSensor->getChannel()->setChannelNumber(RelayId::_LastRelay+1+InputId::AlarmArmament);
  alarmArmedSensor->addAction(ActionId::IntegraArmed, intCtrlActionHandler, Supla::ON_TURN_ON);
  alarmArmedSensor->addAction(ActionId::IntegraDisarmed, intCtrlActionHandler, Supla::ON_TURN_OFF);
  alarmArmedSensor->setFilteringTimeMs(500, true);

  alarmArmedBlockedInternalSensors = new Supla::Sensor::VirtualBinary();
  alarmArmedBlockedInternalSensors->setInitialCaption(sensorName[InputId::VAlarmBlockedInternalSensors]);
  alarmArmedBlockedInternalSensors->getChannel()->setDefault(SUPLA_CHANNELFNC_BINARY_SENSOR);
  alarmArmedBlockedInternalSensors->getChannel()->setChannelNumber(RelayId::_LastRelay+1+InputId::VAlarmBlockedInternalSensors);

  frontDoorBellSensor = new TimedBinarySensor(20, inPcf, InputId::FrontDoorBell, false, INVERT_LOGIC);
  frontDoorBellSensor->setInitialCaption(sensorName[InputId::FrontDoorBell]);
  frontDoorBellSensor->getChannel()->setDefault(SUPLA_CHANNELFNC_BINARY_SENSOR);
  frontDoorBellSensor->getChannel()->setChannelNumber(RelayId::_LastRelay+1+InputId::FrontDoorBell);
  frontDoorBellSensor->addAction(ActionId::DoorBell, intCtrlActionHandler, Supla::ON_TURN_ON);
  frontDoorBellSensor->setFilteringTimeMs(500, true);

  button = new Supla::Control::Button(inPcf, InputId::LightTriggerFromIntegra, true, INVERT_LOGIC);
  button->setButtonType(Supla::Control::Button::ButtonType::MONOSTABLE);
  button->setHoldTime(1000);
  button->setMulticlickTime(350);
  button->setInitialCaption(sensorName[InputId::LightTriggerFromIntegra]);
  button->setOnLoadConfigType(Supla::Control::Button::OnLoadConfigType::LOAD_BUTTON_SETUP_ONLY);
  button->addAction(ActionId::TurnLightsOn, intCtrlActionHandler, Supla::ON_CLICK_1);
  button->addAction(ActionId::BlockInternalSensors, intCtrlActionHandler, Supla::ON_CLICK_2);
  button->addAction(ActionId::TurnLightsOff, intCtrlActionHandler, Supla::ON_HOLD);
  Supla::Control::ActionTrigger* at = new Supla::Control::ActionTrigger();
  at->getChannel()->setChannelNumber(RelayId::_LastRelay+1+InputId::LightTriggerFromIntegra);
  at->setInitialCaption(sensorName[InputId::LightTriggerFromIntegra]);
  at->attach(button);

  relay[RelayId::VRestoreDisplay1]->addAction(ActionId::RestoreDisplay, intCtrlActionHandler, Supla::ON_TURN_ON);
  relay[RelayId::VRestoreDisplay2]->addAction(ActionId::RestoreDisplay, intCtrlActionHandler, Supla::ON_TURN_ON);
  relay[RelayId::FrontDoorOpen]->addAction(ActionId::OpenFrontDoor, intCtrlActionHandler, Supla::ON_TURN_ON);
  relay[RelayId::GateOpen]->addAction(ActionId::OpenGate, intCtrlActionHandler, Supla::ON_TURN_ON);

}

void IntCtrlRelay::InitalizeRelays() {
  #define RELAY_SETUP(id) \
    relay[id]->enableCountdownTimerFunction(); \
    relay[id]->setStoredTurnOnDurationMs(5000); //konieczne, bo inaczej przekaznik sie wyłącza od razu niezaleznie od ustawienia wartości w turnOn
//relay[id]->setDefaultStaircaseDurationMs(0);
  
  RELAY_SETUP(RelayId::VHeaters2h)
  RELAY_SETUP(RelayId::VHeaters4h)
  RELAY_SETUP(RelayId::VHeaters8h)
  RELAY_SETUP(RelayId::VAlarmLights)
}


void IntCtrlRelay::Iterate() {  
  // wysłanie komunikatu o włączonym okapie
  if (hoodModeStart>0 && hoodModeStart+60*60*1000<millis()) {
    messenger.sendMessage(MSG_HOOD_TIME_WARNING, MSG_TITLE_HOOD, 60);
    hoodModeStart=0;
  }
}