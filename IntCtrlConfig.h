#ifndef INTCTRLCONFIG_H
#define INTCTRLCONFIG_H

#include <supla/network/esp_wifi.h>
#include <WiFi.h>
#include <supla/network/esp32eth.h>
#include <Supla/network/html_element.h>
#include <Supla/network/web_sender.h>
#include <Supla/storage/LittleFS_config.h>
#include "Definitions.h"

class IntCtrlSection : public Supla::HtmlElement {
  private:
    const char* h4title;
  public:
    IntCtrlSection(const char* title) : HtmlElement(Supla::HtmlSection::HTML_SECTION_FORM), h4title(title) {};
    void send(Supla::WebSender* sender) override;
};

// This class provides configuration storage (like SSID, password, all
// user defined parameters, etc. We use LittleFS.

class IntCtrlConfig  : public Supla::LittleFsConfig {
  private:
    uint8_t telnetDebug;
    int32_t hoodModeTimeWarningMin;
    int32_t airVentManualBoostTimeMin;
    int32_t airVentHumidityBoostMaxTimeMin;
    int32_t airVentLowConditionBoostMaxTimeMin;
    int32_t lightsAlarmTimeSec;
    char sensorNames[8][17];
    Supla::Network *network;

  public:
    IntCtrlConfig() : Supla::LittleFsConfig(2048)  {
      telnetDebug = 0;
      airVentManualBoostTimeMin = 15;
      airVentHumidityBoostMaxTimeMin = 30;
      airVentLowConditionBoostMaxTimeMin = 60;
      hoodModeTimeWarningMin = 60;
      lightsAlarmTimeSec = 120;
      network = nullptr;
      char defaultName[17];
      for (int i=0; i<8; i++) {
        snprintf(defaultName, sizeof(defaultName), "Temperatura %d", i+1);
        strcpy (sensorNames[i], defaultName);
      }
    };
    bool getTelnetDebug() { return telnetDebug>0; };
    int32_t getAirVentBoostTimeMin() { return airVentManualBoostTimeMin; };
    int32_t getAirVentHumidityBoostMaxTimeMin() { return airVentHumidityBoostMaxTimeMin; };
    int32_t getAirVentLowConditionBoostMaxTimeMin() { return airVentLowConditionBoostMaxTimeMin; };
    int32_t getHoodModeTimeWarningMin() { return hoodModeTimeWarningMin; };
    int32_t getLightsAlarmTimeSec() { return lightsAlarmTimeSec; };

    char* getSensorName(int32_t id) { return sensorNames[id]; };
    
    bool isNetReady() {
      if (network==nullptr) return false;
      return network->isReady();
    };

    IPAddress getIp() {
      if (network==nullptr) return false;
        return WiFi.softAPIP();
      return INADDR_NONE;
    };

    String getSSID() {
      return WiFi.softAPSSID();
    }
    
    void setupNetwork();
    void initialize();
    void loadStorage();
    void loadNetwork();
    void applyCert();
    void setupDownloadConfig();
};

#endif //INTCTRLCONFIG_H