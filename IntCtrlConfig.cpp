#include <sstream>
#include <string>

#include <SuplaDevice.h>
#include <Update.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <supla/control/relay.h>
#include <supla/control/button.h>
#include <supla/control/action_trigger.h>
#include <supla/storage/littlefs_config.h>
#include <supla/network/esp_web_server.h>
#include <supla/device/supla_ca_cert.h>
#include <supla/events.h>

// Includes for HTML elements
#include <supla/network/html/device_info.h>
#include <supla/network/html/protocol_parameters.h>
#include <supla/network/html/wifi_parameters.h>
#include <supla/network/html/ethernet_parameters.h>
#include <supla/network/html/button_update.h>
#include <supla/network/html/button_refresh.h>
#include <supla/network/html/custom_text_parameter.h>
#include <supla/network/html/custom_parameter.h>
#include <supla/network/html/select_input_parameter.h>
#include <supla/network/html/custom_checkbox_parameter.h>
#include "Definitions.h"
#include "IntCtrlConfig.h"
#include <supla/storage/eeprom.h>
#include <RTClib.h>

Supla::Eeprom eeprom;
Supla::EspWebServer suplaServer;
extern RTC_DS3231* rtc;

#include <DallasTemperature.h>
#include "IntCtrlSensors.h"
extern IntCtrlSensors sensors;

#define TELNET_DEBUG "TELNET_DBG"
#define TEMP_PARAM_HEADER "DALLAS"

class ButtonReset : public Supla::HtmlElement {
public:
  ButtonReset() : HtmlElement(Supla::HtmlSection::HTML_SECTION_BUTTON_AFTER) {};
  void send(Supla::WebSender* sender) override {
    sender->send("<input type=\"hidden\" name=\"just_reboot\" id=\"just_reboot\" value=\"0\">"
                 "<button type=\"button\" onclick=\"var e=document.getElementById('just_reboot'); e.value='1'; var f=document.getElementById('cfgform'); f.submit();\">"
                 "RESTART"
                 "</button>");
  }

  // Przechwytuje kliknięcie przycisku
  bool handleResponse(const char* key, const char* value) override {
    if (strcmp(key, "just_reboot") == 0 && strcmp(value, "1") == 0) {
      SUPLA_LOG_DEBUG("Otrzymano żądanie restartu urządzenia...");    
      // Bezpieczny restart urządzenia dostarczany przez bibliotekę Supla
      SuplaDevice.softRestart();
      return true;
    }
    return false;
  }
};


class ButtonDownloadConfig : public Supla::HtmlElement {
public:
  ButtonDownloadConfig() : HtmlElement(Supla::HtmlSection::HTML_SECTION_BUTTON_AFTER) {};
  void send(Supla::WebSender* sender) override {
    sender->send(
        "<a href=\"/download-config\" style=\"text-decoration:none;\">"
        "<button type=\"button\">Pobierz plik konfiguracyjny (Backup)</button>"
        "</a>");
  }
};

class ButtonUploadConfig : public Supla::HtmlElement {
  public:
    ButtonUploadConfig() : HtmlElement(Supla::HtmlSection::HTML_SECTION_BUTTON_AFTER) {}
    void send(Supla::WebSender* sender) override {
        sender->send(
            "<div class=\"w\" style=\"margin-top:15px; border:1px dashed #ccc; padding:15px; border-radius:4px;\">"
            "<b style=\"display:block; margin-bottom:8px;\">⚙️ Przywróć konfigurację z pliku:</b>"
            "<form method=\"POST\" action=\"/upload-config\" enctype=\"multipart/form-data\">"
            "<input type=\"file\" name=\"update\" style=\"margin-bottom:10px; width:100%;\"><br>"
            "<button type=\"submit\" style=\"background-color:#007bff; color:white; border:none; padding:8px 15px; border-radius:4px; cursor:pointer; width:100%; font-size:14px;\">"
            "📤 Wgraj i przywróć konfigurację"
            "</button>"
            "</form>"
            "</div>"
        );
    }
};

void IntCtrlSection::send(Supla::WebSender* sender) {
  sender->send("<h4>");
  sender->send(h4title);
  sender->send("</h4>");
}
  
void IntCtrlConfig::initialize() {
  auto storage = Supla::Storage::ConfigInstance();
  this->init();

  new Supla::Html::DeviceInfo(&SuplaDevice);
  new Supla::Html::WifiParameters();
  new Supla::Html::ProtocolParameters();
  
  new IntCtrlSection("Nazwy czujników temperatury");
  for (int i=0; i<sensors.getSensorAmount(); i++) {
    char param[16], address[64];
    snprintf(param, sizeof(param), "%s%d", TEMP_PARAM_HEADER, i);
    snprintf(address, sizeof(address), "Czujnik temp. (%.1f°C) adr: ", sensors.getTemp(i));
    sensors.getDeviceAdress(i, address+strlen(address), 64-strlen(address));
    new Supla::Html::CustomTextParameter(param, address, 64);
  }

  new IntCtrlSection("Debug");

  new Supla::Html::CustomCheckboxParameter(TELNET_DEBUG, "Usługa Telnet dla wyjścia Serial", telnetDebug);

  new Supla::Html::ButtonRefresh();
  new Supla::Html::ButtonUpdate(&suplaServer);
  new ButtonReset();
  //new ButtonDownloadConfig();
  //new ButtonUploadConfig();

  SuplaDevice.setInitialMode(Supla::InitialMode::StartInCfgMode);
  SuplaDevice.setLeaveCfgModeAfterInactivityMin(5);
  loadStorage();
  applyCert();
  setupNetwork();
  setupDownloadConfig();
  SUPLA_LOG_DEBUG("Zakończone ustawianie konfiguracji.");
}

void IntCtrlConfig::loadStorage() {    
  this->getUInt8(TELNET_DEBUG, &telnetDebug);
   
  int i = 0;
  for (int i=0; i<sensors.getSensorAmount(); i++) {
    char param[17];
    snprintf(param, 16, "%s%d", TEMP_PARAM_HEADER, i);
    this->getString(param, sensorNames[i], 16);
  }
}

void IntCtrlConfig::applyCert() {
  int32_t sec = 2;
  char custom_cert[3000]="";
  this->getInt32("sec", &sec);
  switch (sec) {
    case 0:
      SuplaDevice.setSuplaCACert(suplaCACert);
      SUPLA_LOG_DEBUG("Aplikuję standardowy certyfikat");
       //SuplaDevice.setSupla3rdPartyCACert(supla3rdCACert);
      break;
    case 1:
      if (this->getString("custom_ca", custom_cert, sizeof(custom_cert))) {
        SuplaDevice.setSuplaCACert(custom_cert);
        SUPLA_LOG_DEBUG("Aplikuję własny certyfikat (długość %d)", strlen(custom_cert));
      }
      break;
    default:
      break;  // bez certyfikatu
  }
}

void IntCtrlConfig::setupNetwork() {
    SUPLA_LOG_DEBUG("Uruchamianie połączenia bezprzewodowego WI-FI...");
    network = new Supla::ESPWifi();
    //network->SetSetupNeeded();
  }

File uploadFile;
void IntCtrlConfig::setupDownloadConfig() {
  auto serverPtr = suplaServer.getServerPtr();
  if (serverPtr != nullptr) {
      serverPtr->on("/download-config", HTTP_GET, []() {
          String filename = "/supla-dev.cfg"; 

          if (LittleFS.exists(filename)) {
              File file = LittleFS.open(filename, "r");
              if (file) {
                  // Wysyłamy plik jako załącznik (wymusi to pobranie pliku przez przeglądarkę)
                  suplaServer.getServerPtr()->sendHeader("Content-Disposition", "attachment; filename=supla-dev.cfg");
                  suplaServer.getServerPtr()->streamFile(file, "application/json");
                  file.close();
                  return;
              }
          }
          
          // Komunikat błędu, jeśli plik nie istnieje lub jest zablokowany
          suplaServer.getServerPtr()->send(404, "text/plain", "Blad: Plik konfiguracyjny nie istnieje lub nie udalo sie go otworzyc.");
      });
      serverPtr->on("/upload-config", HTTP_POST, []() {
            suplaServer.getServerPtr()->send(200, "text/html", 
                "<h3>Plik zostal wgrany pomyslnie!</h3><p>Urządzenie uruchamia sie ponownie z nowa konfiguracja...</p>");
            delay(1000);
            ESP.restart(); // Restart urządzenia w celu wczytania nowych ustawień
        }, 
        // Druga funkcja przetwarza napływające porcje (bity) pliku w tle (w trybie uploader)
        []() {
            HTTPUpload& upload = suplaServer.getServerPtr()->upload();
            
            if (upload.status == UPLOAD_FILE_START) {
                Serial.printf("Rozpoczęto wgrywanie pliku: %s\n", upload.filename.c_str());
                // Otwieramy plik konfiguracyjny w trybie zapisu (nadpisywanie)
                uploadFile = LittleFS.open("/config.json", "w");
            } 
            else if (upload.status == UPLOAD_FILE_WRITE) {
                if (uploadFile) {
                    // Zapisujemy odebrany bufor danych do pamięci Flash
                    uploadFile.write(upload.buf, upload.currentSize);
                }
            } 
            else if (upload.status == UPLOAD_FILE_END) {
                if (uploadFile) {
                    uploadFile.close();
                    Serial.printf("Pomyślnie zapisano plik. Rozmiar: %d bajtów\n", upload.totalSize);
                } else {
                    Serial.println("Błąd zapisu pliku na partycji LittleFS.");
                }
            }
        });
  }
}