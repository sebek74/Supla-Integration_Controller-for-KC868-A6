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
  "Włącz wyświetlacz 2",
  "Rekuperator - przewietrzanie",
  "Duża wilgotność powietrza 1",
  "Duża wilgotność powietrza 2",
  "Niska jakość powietrza"
};

const char* sensorName [] {
  "Alarm z rejestratora",
  "Alarm z Integry",
  "Światła z Integry",
  "Czuwanie alarmu",
  "Dzwonek do furtki",
  "Przycisk przewietrzania",
  "Blokada wewnętrznych czujników",
  "Zapal światła garaż i front",
  "Zgaś światła garaż i front",
  "Czujnik włączenia okapu"
};

extern int32_t frameId;

#ifdef TEST_BOARD

class HipHopBinary : public Supla::Sensor::VirtualBinary {
  public:
  bool getValue() override {
    return frameId%50>25;
  };
};

#endif // TEST_BOARD

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

class IntCtrlActionHandler : public Supla::ActionHandler {
  private:
    int32_t lightPressTimestamp = 0;
  public:
    void handleAction(int event, int action) override {
      SUPLA_LOG_DEBUG("action event: %d %d", event, action);
      uint32_t remainingSeconds = 0;
      time_t now = time(nullptr);
      struct tm* timeinfo = localtime(&now);
      display.restoreFullContrast();
      switch (action) {
        case ActionId::VideoAlarmOn:
          IntCtrlRelay::GetRelayById(RelayId::VideoRecorderAlarm)->turnOn();
          IntCtrlRelay::GetRelayById(RelayId::VAlarmLights)->turnOn(config.getLightsAlarmTimeSec()*1000);
          messenger.sendMessage(MSG_VIDEORECORDER_ALARM_TRIGGER, MSG_TITLE_ALARM);        
          break;
        case ActionId::VideoAlarmOff:
          IntCtrlRelay::GetRelayById(RelayId::VideoRecorderAlarm)->turnOff();
          break;
        case ActionId::IntegraAlarmOn:
          IntCtrlRelay::GetRelayById(RelayId::VAlarmLights)->turnOn(config.getLightsAlarmTimeSec()*1000);
          messenger.sendMessage(MSG_INTEGRA_ALARM_TRIGGER, MSG_TITLE_ALARM);
          break;
        case ActionId::IntegraAlarmOff:
          IntCtrlRelay::GetRelayById(RelayId::VAlarmLights)->turnOff();
          break;
        case ActionId::IntegraArmed:
          messenger.sendMessage(MSG_ALARM_ARMED, MSG_TITLE_ALARM, messenger.getDateTime());
          IntCtrlRelay::alarmArmedBlockedInternalSensors->clear();
          break;
        case ActionId::IntegraDisarmed:
          messenger.sendMessage(MSG_ALARM_DISARMED, MSG_TITLE_ALARM, messenger.getDateTime());
          IntCtrlRelay::alarmArmedBlockedInternalSensors->clear();
          break;
        case ActionId::DoorBell:
          messenger.sendMessage(MSG_GATE_BELL, MSG_TITLE_DOORSTATION, messenger.getDateTime());
          break;
        case HoodOn:
          IntCtrlRelay::GetRelayById(RelayId::AirVent_HoodMode)->turnOn();
          IntCtrlRelay::hoodModeStart = millis();
          break;
        case HoodOff:
          IntCtrlRelay::GetRelayById(RelayId::AirVent_HoodMode)->turnOff();
          IntCtrlRelay::hoodModeStart = 0;
          break;
         case OpenGate:
          display.setupAction(ActionIconId::GATE_OPEN);
          break;
        case OpenFrontDoor:
          display.setupAction(ActionIconId::FRONT_DOOR_OPEN);
          break;
        case ManualBoostMode:
          SUPLA_LOG_DEBUG("Manual boost mode activated");
          messenger.sendMessage(MSG_AIRVENT_BOOST_ACTIVATED, MSG_TITLE_ALARM); 
          IntCtrlRelay::GetBoostRelay()->turnOn(config.getAirVentBoostTimeMin()*60*1000);
          break;
        case HumidityExceeded1:  
          if (!IntCtrlRelay::GetBoostRelay()->isOnOrPaused()) {
            SUPLA_LOG_DEBUG("Humidity1 exceeded - boost mode activated");
            IntCtrlRelay::GetBoostRelay()->turnOn();
          } else
             SUPLA_LOG_DEBUG("Humidity1 exceeded - boost mode already activated");
          break;
        case HumidityNominal1:
          IntCtrlRelay::GetBoostRelay()->getRemainingCountdownTimerSec(&remainingSeconds);
          if (IntCtrlRelay::GetBoostRelay()->isOnOrPaused() && remainingSeconds==0 && 
              !IntCtrlRelay::GetRelayById(RelayId::AirVent_HumidityExceeded2)->isOn() &&
              !IntCtrlRelay::GetRelayById(RelayId::AirVent_AirQualityLow)->isOn()) {
                SUPLA_LOG_DEBUG("Humidity1 nominal - boost mode deactivated");
                IntCtrlRelay::GetBoostRelay()->turnOff();
          } else 
            SUPLA_LOG_DEBUG("Humidity1 nominal - cannot deactivate");
          break;
        case HumidityExceeded2:
          if (!IntCtrlRelay::GetBoostRelay()->isOnOrPaused()) {
            SUPLA_LOG_DEBUG("Humidity2 exceeded - boost mode activated");
            IntCtrlRelay::GetBoostRelay()->turnOn();
          } else
             SUPLA_LOG_DEBUG("Humidity1 exceeded - boost mode already activated");
          break;
        case HumidityNominal2:
          IntCtrlRelay::GetBoostRelay()->getRemainingCountdownTimerSec(&remainingSeconds);
          if (IntCtrlRelay::GetBoostRelay()->isOnOrPaused() && remainingSeconds==0 && 
              !IntCtrlRelay::GetRelayById(RelayId::AirVent_HumidityExceeded1)->isOn() &&
              !IntCtrlRelay::GetRelayById(RelayId::AirVent_AirQualityLow)->isOn()) {
                SUPLA_LOG_DEBUG("Humidity2 nominal - boost mode deactivated");
                IntCtrlRelay::GetBoostRelay()->turnOff();
          } else 
            SUPLA_LOG_DEBUG("Humidity2 nominal - cannot deactivate");
          break;
        case AirConditionLow:         
          if (!IntCtrlRelay::GetBoostRelay()->isOnOrPaused()) {
            SUPLA_LOG_DEBUG("Air condition low - boost mode activated");
            IntCtrlRelay::GetBoostRelay()->turnOn();
          } else
             SUPLA_LOG_DEBUG("Air condition low - boost mode already activated");
          break;
        case AirConditionNominal:
          IntCtrlRelay::GetBoostRelay()->getRemainingCountdownTimerSec(&remainingSeconds);
          if (!IntCtrlRelay::GetBoostRelay()->isOnOrPaused() && remainingSeconds==0 && 
              !IntCtrlRelay::GetRelayById(RelayId::AirVent_HumidityExceeded1)->isOn() &&
              !IntCtrlRelay::GetRelayById(RelayId::AirVent_HumidityExceeded2)->isOn()) {
                SUPLA_LOG_DEBUG("Air condition nominal - boost mode deactivated");
                IntCtrlRelay::GetBoostRelay()->turnOff();
          } else 
            SUPLA_LOG_DEBUG("Air condition nominal - cannot deactivate");
          break;
        case LightTimeCheckOn:
          if (lightPressTimestamp+3000>millis()) { // filtruj podwójne zdarzenia      
            SUPLA_LOG_DEBUG("light trigger press filtered at %d (interval %d)", millis(), millis()-lightPressTimestamp);
            lightPressTimestamp = 0;
            break; 
          }
          lightPressTimestamp = millis();
          SUPLA_LOG_DEBUG("light trigger press at %d", lightPressTimestamp);
          break;
        case LightTimeCheckOff:
          if (lightPressTimestamp ==0) break;
          int32_t releaseTime = millis();
          SUPLA_LOG_DEBUG("light trigger release at %d (on time: %d)", releaseTime, releaseTime-lightPressTimestamp);
          if (releaseTime-lightPressTimestamp<800) {
            IntCtrlRelay::lightsOnSensor->set();
            display.setupAction(ActionIconId::LIGHTS_ON);
            SUPLA_LOG_DEBUG("light on action detected");
          } else if (releaseTime-lightPressTimestamp>=2000) {
            IntCtrlRelay::lightsOffSensor->set();
            display.setupAction(ActionIconId::LIGHTS_OFF);
            SUPLA_LOG_DEBUG("light off action detected");
          } else if (releaseTime-lightPressTimestamp>=800 && releaseTime-lightPressTimestamp<2000) {
            IntCtrlRelay::alarmArmedBlockedInternalSensors->set();
            messenger.sendMessage(MSG_ALARM_BLOCADES, MSG_TITLE_ALARM);
            SUPLA_LOG_DEBUG("alarm sensors blocked action detected");
          }
          break;
      }
    }
} intCtrlActionHandler;

#define INVERT_LOGIC true
#define HIGH_IS_ON true

void IntCtrlRelay::RegisterRelays() {
  auto button = new Supla::Control::Button(BUTTON_CFG_RELAY_GPIO, true, true);
  button->configureAsConfigButton(&SuplaDevice);

  relay[RelayId::VideoRecorderAlarm] = /*new Supla::Control::VirtualRelay();*/ new IntCtrlRelay(0, VideoRecorderAlarm);
  relay[RelayId::VideoRecorderAlarm]->getChannel()->setDefault(SUPLA_CHANNELFNC_POWERSWITCH);
  relay[RelayId::SchedDoorSensorAlarm] = /*new Supla::Control::VirtualRelay();*/ new IntCtrlRelay(1, SchedDoorSensorAlarm);
  relay[RelayId::SchedDoorSensorAlarm]->getChannel()->setDefault(SUPLA_CHANNELFNC_POWERSWITCH);
  relay[RelayId::AirVent_HoodMode] = /*new Supla::Control::VirtualRelay();*/ new IntCtrlRelay(3, AirVent_HoodMode);
  relay[RelayId::AirVent_HoodMode]->getChannel()->setDefault(SUPLA_CHANNELFNC_POWERSWITCH);
  relay[RelayId::AirVent_BoostMode] = /*new Supla::Control::VirtualRelay();*/ new IntCtrlRepeatedRelay(EXTRA_GPIO_2, HIGH_IS_ON);
  relay[RelayId::AirVent_BoostMode]->getChannel()->setDefault(SUPLA_CHANNELFNC_POWERSWITCH);
  relay[RelayId::FrontDoorOpen] = /*new Supla::Control::VirtualRelay(); */ new IntCtrlRelay(4, FrontDoorOpen);
  relay[RelayId::FrontDoorOpen]->getChannel()->setDefault(SUPLA_CHANNELFNC_CONTROLLINGTHEGATEWAYLOCK);
  relay[RelayId::GateOpen] = /*new Supla::Control::VirtualRelay(); */new IntCtrlRelay(5, GateOpen);
  relay[RelayId::GateOpen]->getChannel()->setDefault(SUPLA_CHANNELFNC_CONTROLLINGTHEGATE);
  relay[RelayId::GarageOpen] = new ProtectedRelay(EXTRA_GPIO_1, HIGH_IS_ON);/*new Relay(EXTRA_GPIO_1, false);*/
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

  button = new Supla::Control::Button(inPcf, InputId::BoostModeButton, false, INVERT_LOGIC);
  button->setButtonType(Supla::Control::Button::ButtonType::MONOSTABLE);
  button->setHoldTime(1000);
  button->setMulticlickTime(350);
  button->setSwNoiseFilterDelay(150);
  button->setOnLoadConfigType(Supla::Control::Button::OnLoadConfigType::LOAD_BUTTON_SETUP_ONLY);
  button->addAction(Supla::TURN_OFF, relay[RelayId::AirVent_BoostMode], Supla::ON_HOLD);
  button->addAction(ActionId::ManualBoostMode, intCtrlActionHandler, Supla::ON_CLICK_1);
  auto at = new Supla::Control::ActionTrigger();
  at->setRelatedChannel(relay[RelayId::AirVent_BoostMode]);
  at->attach(button);

  hoodSensor = new Supla::Sensor::Binary(AIRVENT_HOODMODE_SENSOR, false, INVERT_LOGIC);
  hoodSensor->setInitialCaption(sensorName[InputId::HoodSensor]);
  hoodSensor->getChannel()->setDefault(SUPLA_CHANNELFNC_BINARY_SENSOR);
  hoodSensor->getChannel()->setChannelNumber(RelayId::_LastRelay+1+InputId::HoodSensor);
  hoodSensor->addAction(ActionId::HoodOn, intCtrlActionHandler, Supla::ON_TURN_ON);
  hoodSensor->addAction(ActionId::HoodOff, intCtrlActionHandler, Supla::ON_TURN_OFF);
  hoodSensor->setFilteringTimeMs(1000, true);

  alarmTriggerFromVideoRecorder = new TimedBinarySensor(120, inPcf, InputId::AlarmTriggerFromVideoRecorder, false, INVERT_LOGIC);
  alarmTriggerFromVideoRecorder->setInitialCaption(sensorName[InputId::AlarmTriggerFromVideoRecorder]);
  alarmTriggerFromVideoRecorder->getChannel()->setDefault(SUPLA_CHANNELFNC_BINARY_SENSOR);
  alarmTriggerFromVideoRecorder->getChannel()->setChannelNumber(RelayId::_LastRelay+1+InputId::AlarmTriggerFromVideoRecorder);
  alarmTriggerFromVideoRecorder->addAction(ActionId::VideoAlarmOn, intCtrlActionHandler, Supla::ON_TURN_ON);
  alarmTriggerFromVideoRecorder->addAction(ActionId::VideoAlarmOff, intCtrlActionHandler, Supla::ON_TURN_OFF);
  alarmTriggerFromVideoRecorder->setFilteringTimeMs(1000, true);

  alarmTriggerFromIntegra = new Supla::Sensor::Binary(inPcf, InputId::AlarmTriggerFromIntegra, false, INVERT_LOGIC);
  alarmTriggerFromIntegra->setInitialCaption(sensorName[InputId::AlarmTriggerFromIntegra]);
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

  lightTriggerSensor = new Supla::Sensor::Binary(inPcf, InputId::LightTriggerFromIntegra, false, INVERT_LOGIC);
  lightTriggerSensor->setInitialCaption(sensorName[InputId::LightTriggerFromIntegra]);
  lightTriggerSensor->getChannel()->setChannelNumber(RelayId::_LastRelay+1+InputId::LightTriggerFromIntegra);
  lightTriggerSensor->addAction(ActionId::LightTimeCheckOn, intCtrlActionHandler, Supla::ON_TURN_ON);
  lightTriggerSensor->addAction(ActionId::LightTimeCheckOff, intCtrlActionHandler, Supla::ON_TURN_OFF);
  
  lightsOnSensor = new Supla::Sensor::VirtualBinary();
  lightsOnSensor->setTimeoutDs(30);
  lightsOnSensor->setUseConfiguredTimeout(true);
  lightsOnSensor->setInitialCaption(sensorName[InputId::VLightOn]);
  lightsOnSensor->getChannel()->setDefault(SUPLA_CHANNELFNC_BINARY_SENSOR);
  lightsOnSensor->getChannel()->setChannelNumber(RelayId::_LastRelay+1+InputId::VLightOn);

  lightsOffSensor = new Supla::Sensor::VirtualBinary();
  lightsOffSensor->setTimeoutDs(30);
  lightsOffSensor->setUseConfiguredTimeout(true);
  lightsOffSensor->setInitialCaption(sensorName[InputId::VLightOff]);
  lightsOffSensor->getChannel()->setDefault(SUPLA_CHANNELFNC_BINARY_SENSOR);
  lightsOffSensor->getChannel()->setChannelNumber(RelayId::_LastRelay+1+InputId::VLightOff);

  relay[RelayId::AirVent_HumidityExceeded1] = new Supla::Control::VirtualRelay();
  relay[RelayId::AirVent_HumidityExceeded1]->getChannel()->setDefault(SUPLA_CHANNELFNC_STAIRCASETIMER);
  relay[RelayId::AirVent_HumidityExceeded2] = new Supla::Control::VirtualRelay();
  relay[RelayId::AirVent_HumidityExceeded2]->getChannel()->setDefault(SUPLA_CHANNELFNC_STAIRCASETIMER);
  relay[RelayId::AirVent_AirQualityLow] = new Supla::Control::VirtualRelay();
  relay[RelayId::AirVent_AirQualityLow]->getChannel()->setDefault(SUPLA_CHANNELFNC_STAIRCASETIMER);

  relay[RelayId::VRestoreDisplay1]->addAction(ActionId::RestoreDisplay, intCtrlActionHandler, Supla::ON_TURN_ON);
  relay[RelayId::VRestoreDisplay2]->addAction(ActionId::RestoreDisplay, intCtrlActionHandler, Supla::ON_TURN_ON);
  relay[RelayId::FrontDoorOpen]->addAction(ActionId::OpenFrontDoor, intCtrlActionHandler, Supla::ON_TURN_ON);
  relay[RelayId::GateOpen]->addAction(ActionId::OpenGate, intCtrlActionHandler, Supla::ON_TURN_ON);
  relay[RelayId::AirVent_HumidityExceeded1]->addAction(ActionId::HumidityExceeded1, intCtrlActionHandler, Supla::ON_TURN_ON);
  relay[RelayId::AirVent_HumidityExceeded1]->addAction(ActionId::HumidityNominal1, intCtrlActionHandler, Supla::ON_TURN_OFF);
  relay[RelayId::AirVent_HumidityExceeded2]->addAction(ActionId::HumidityExceeded2, intCtrlActionHandler, Supla::ON_TURN_ON);
  relay[RelayId::AirVent_HumidityExceeded2]->addAction(ActionId::HumidityNominal2, intCtrlActionHandler, Supla::ON_TURN_OFF);
  relay[RelayId::AirVent_AirQualityLow]->addAction(ActionId::AirConditionLow, intCtrlActionHandler, Supla::ON_TURN_ON);
  relay[RelayId::AirVent_AirQualityLow]->addAction(ActionId::AirConditionNominal, intCtrlActionHandler, Supla::ON_TURN_OFF);

  for (int i = RelayId::_FirstRelay; i<=RelayId::_LastRelay; i++) {
    if (i<HumidityExceeded1)
      relay[i]->getChannel()->setChannelNumber(i);
    else
      relay[i]->getChannel()->setChannelNumber(i+RelayId::_LastRelay*4);  // workaround pominiecie problemu konflikyu kanałów (nowe kanaly)
    relay[i]->getChannel()->setDefaultIcon(0);
    relay[i]->setInitialCaption(relayNames[i]);
  }
}

void IntCtrlRelay::InitalizeRelays() {
  #define RELAY_SETUP(id, time) \
    relay[id]->enableCountdownTimerFunction(); \
    relay[id]->setStoredTurnOnDurationMs(time); //konieczne, bo inaczej przekaznik sie wyłącza od razu niezaleznie od ustawienia wartości w turnOn
  
  RELAY_SETUP(RelayId::VHeaters2h, 5000)
  RELAY_SETUP(RelayId::VHeaters4h, 5000)
  RELAY_SETUP(RelayId::VHeaters8h, 5000)
  RELAY_SETUP(RelayId::VAlarmLights, 5000)
  RELAY_SETUP(RelayId::AirVent_HumidityExceeded1, config.getAirVentHumidityBoostMaxTimeMin()*60*1000)
  RELAY_SETUP(RelayId::AirVent_HumidityExceeded2, config.getAirVentHumidityBoostMaxTimeMin()*60*1000)
  RELAY_SETUP(RelayId::AirVent_AirQualityLow, config.getAirVentLowConditionBoostMaxTimeMin()*60*1000)
}

void IntCtrlRelay::Iterate() {  
  // wysłanie komunikatu o włączonym okapie
  if (hoodModeStart>0 && hoodModeStart+config.getHoodModeTimeWarningMin()*60*1000<millis()) {
    messenger.sendMessage(MSG_HOOD_TIME_WARNING, MSG_TITLE_HOOD, config.getHoodModeTimeWarningMin());
    hoodModeStart=0;
  }
}