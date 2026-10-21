#include <SuplaDevice.h>
#include <supla/control/button.h>
#include <supla/control/relay.h>
#include <supla/sensor/binary.h>
#include <supla/control/virtual_relay.h>
#include <math.h>
#include <DallasTemperature.h>
#include "Definitions.h"
#include "IntCtrlSensors.h"
#include "Definitions.h"
#include "IntCtrlRelays.h"
#include "IntCtrlConfig.h"
#include "IntCtrlDisplay.h"
#include "Bitmaps.h"
#include "ArialMT10pt7b.h"
#include "ArialMT7pt7b.h"
#include "ArialMT5pt7b.h" 
/*
  mały font 5ka:

 & running man
 ' trybik
 ( termometr
 ) dzwoneczek
 * zegarek
 [ ą
 \ ć
 ] ę
 ^ ł
 _ ń
 ` ó 
 { ś
 | ż
 } ź
*/

extern IntCtrlSensors sensors;
extern IntCtrlConfig config;
extern const char* relayNames[];

bool IntCtrlDisplay::Initialize() {
#ifdef USE_SSD1306
 #define WHITE_COLOR SSD1306_WHITE
 #define INVERSE_COLOR SSD1306_INVERSE
 #define BLACK_COLOR SSD1306_INVERSE
 if (!begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS, true)) { 
    SUPLA_LOG_ERROR("Nie znaleziono wyświetlacza SSD1306");
    return false;
  } else {
    SUPLA_LOG_DEBUG("Wyświetlacz SSD1306 znaleziony");
    initialized = true;
    return true;
  }
#endif
#ifdef USE_SH1106G
  #define WHITE_COLOR SH110X_WHITE
  #define INVERSE_COLOR SH110X_INVERSE
  #define BLACK_COLOR SH110X_BLACK
  if (!begin(SCREEN_ADDRESS, true)) { 
    SUPLA_LOG_ERROR("Nie znaleziono wyświetlacza SH1106");
    return false;
  } else {
    SUPLA_LOG_DEBUG("Wyświetlacz SSH1106 znaleziony");
    initialized = true;
    return true;
  }
#endif 
}

int32_t frameId = 0;

void IntCtrlDisplay::iterate() {
  if (!initialized) return;
  if (millis() - lastDisplayUpdate < DISPLAY_INTERVAL) return;
  lastDisplayUpdate = millis();
  frameId++;    
  if (SuplaDevice.getDeviceMode() == Supla::DeviceMode::DEVICE_MODE_CONFIG) { 
    displayConfig();
    return;
  } 
  if (screenOff) return;
  dimContrast();
  clearDisplay();
  drawIcons();
  if (!drawAlert()) {
    drawSensor();
    drawHood();
    drawActionIcon();
    drawNetworkIcon();
  }
  display();
}

void IntCtrlDisplay::drawHood() {
  if (IntCtrlRelay::IsHoodActive()) {
    drawBitmap(0, 22, hood_bits[frameId%4], 18, 18, WHITE_COLOR);
    if (IntCtrlRelay::IsAirVentHoodModeActive())
      drawBitmap(0, 43, airvent_bits[1+frameId%2], 18, 21, WHITE_COLOR);
    else
      drawBitmap(0, 43, airvent_bits[0], 18, 21, WHITE_COLOR); }
}

void IntCtrlDisplay::drawIcons() {

  if (IntCtrlRelay::IsAlarmArmed()) {
    if (IntCtrlRelay::IsAlarmInternalSensorsBlocked())
     drawBitmap(0, 0, key_ext_bits, 27, 20, WHITE_COLOR);
    else
      drawBitmap(0, 0, key_bits, 27, 20, WHITE_COLOR);
  };

  if (IntCtrlRelay::IsAlarmLightsOn())
    drawBitmap(30, 0, alarm_light_bits, 22, 20, WHITE_COLOR);

  if (IntCtrlRelay::IsHeaterActive()) {
    drawBitmap(57, 0, heater_bits, 20, 20, WHITE_COLOR);
    uint32_t time = 0;
    if (IntCtrlRelay::GetHeaterTimeRemainingS(time)) {
      char timeRemaining[12];
      snprintf(timeRemaining, sizeof(timeRemaining), "%d%s%02d",  (time/3600)%60, (millis()%1000>500)?":":"~", (time/60)%60);
      setFont(&ArialMT10pt7b);
      setCursor(80, 17);
      setTextColor(WHITE_COLOR);
      printf(timeRemaining);
    }
  }
}

void IntCtrlDisplay::drawSensor() {
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);
  int32_t pagesAmount = sensors.getSensorAmount();
  if (timeinfo->tm_year>120) pagesAmount++;

  if (enterPageTime==0) enterPageTime = millis();
  if (millis()-enterPageTime>SENSOR_PAGE_TIME) {
    sensorPage++;
    enterPageTime = millis();
  }
  if (sensorPage>=pagesAmount) {
    sensorPage = 0;
    enterPageTime = millis();
  }

  char description[16];
  char sensorData1[12] = "", sensorData2[9] = "", sensorData3[9] = "";
 
  int16_t x1, y1;
  uint16_t w=0, w1=0, h, w2=0, w3=0;

  // czas
  if (sensorPage==sensors.getSensorAmount() && timeinfo->tm_year>120) {
    strcpy(description, "*Zegar");
    snprintf(sensorData1, sizeof(sensorData1), "%d%s%02d", timeinfo->tm_hour, (millis()%1000>500)?":":"~", timeinfo->tm_min);
    snprintf(sensorData2, sizeof(sensorData2), "%s%02d", (millis()%1000>500)?":":"~", timeinfo->tm_sec);
  } 
  else if (sensors.getSensorAmount()>0) {
    //drawBitmap(-2, 30, termometr_bits, 32, 33, WHITE_COLOR);
    description[0] = '(';
    strncpy(description+1, sensors.getDeviceDescription(sensorPage), sizeof(description)-1);
    double calkowita;
    double ulamkowa = modf(sensors.getTemp(sensorPage), &calkowita);
    snprintf(sensorData1, sizeof(sensorData1), "%.0f", calkowita);
    snprintf(sensorData2, sizeof(sensorData2), ".%1.0f", fabs(ulamkowa*10));
    sprintf(sensorData3, "'C");
  } else return;

  setFont(&ArialMT5pt7b);
  setTextColor(WHITE_COLOR);
  getTextBounds(P(description), 0, 30, &x1, &y1, &w, &h); 
  setCursor((SCREEN_WIDTH - w) / 2, MIDDLE_ROW);
  print(P(description));

  if (strlen(sensorData1)>0) {
    setFont(&ArialMT10pt7b);
    getTextBounds(String(sensorData1), 0, 63, &x1, &y1, &w1, &h);   
  }
  if (strlen(sensorData2)>0) {
    setFont(&ArialMT7pt7b);
    getTextBounds(String(sensorData2), 0, 63, &x1, &y1, &w2, &h);
  }
  if (strlen(sensorData3)>0) {
    setFont(&ArialMT10pt7b);
    getTextBounds(String(sensorData3), 0, 63, &x1, &y1, &w3, &h);
  }

  setCursor(((SCREEN_WIDTH - w1 - w2 - w3) / 2), BOTTOM_ROW);
  if (strlen(sensorData1)>0) {
    setFont(&ArialMT10pt7b);
    print(sensorData1);
  }
  if (strlen(sensorData2)>0) {
    setFont(&ArialMT7pt7b);
    print(sensorData2);
  }
  if (strlen(sensorData3)>0) {
    setFont(&ArialMT10pt7b);
    print(sensorData3);
  }
}

void IntCtrlDisplay::setupAction(ActionIconId actionId) {
  actionIcon = actionId;
  actionIconStart = millis();
}

void IntCtrlDisplay::drawActionIcon() {
  if (actionIcon!=NO_ACTION && actionIconStart+ACTION_DISPLAY_TIME_S*1000<millis())
    actionIcon = NO_ACTION;
  
  switch (actionIcon) {
    case LIGHTS_ON:
      if (frameId%2) drawBitmap(SCREEN_WIDTH-16, SCREEN_HEIGHT-40, lights_on_bits, 12, 20, WHITE_COLOR);
      break;
    case LIGHTS_OFF:
      if (frameId%2) drawBitmap(SCREEN_WIDTH-16, SCREEN_HEIGHT-40, lights_off_bits, 12, 20, WHITE_COLOR);
      break;
    case FRONT_DOOR_OPEN:
      if (frameId%2) drawBitmap(SCREEN_WIDTH-16, SCREEN_HEIGHT-40, front_door_bits, 12, 20, WHITE_COLOR);
      break;
    case GATE_OPEN:
      if (frameId%2) drawBitmap(SCREEN_WIDTH-16, SCREEN_HEIGHT-40, gate_bits, 13, 20, WHITE_COLOR);
      break;
    case NO_ACTION:
      if (IntCtrlRelay::IsSchedDoorOpen())
        drawBitmap(SCREEN_WIDTH-16, SCREEN_HEIGHT-40, sched_door_open_bits, 12, 20, WHITE_COLOR);
      break;
    case GARAGE_DOOR_OPEN:
      if (frameId%2) drawBitmap(SCREEN_WIDTH-16, SCREEN_HEIGHT-40, garage_bits, 13, 20, WHITE_COLOR);
      break;
  }  
}

// Funkcja rysująca odpowiednią ikonę w prawym górnym rogu ekranu (pozycja X:110, Y:0)
void IntCtrlDisplay::drawNetworkIcon() {
  int percent = WiFi.RSSI();
  int status = SuplaDevice.getCurrentStatus();
  if ((status == STATUS_INITIALIZED || status == STATUS_REGISTER_IN_PROGRESS) && frameId%2)
      drawBitmap(110, SCREEN_HEIGHT-16, wifi_bits[0], 16, 16, WHITE_COLOR);
  else if (status != STATUS_REGISTERED_AND_READY) {
      setFont(&ArialMT5pt7b);
      setTextColor(WHITE_COLOR); // Draw white text
      setCursor(SCREEN_WIDTH-18, SCREEN_HEIGHT-9);
      printf("%02d", status);
      setCursor(SCREEN_WIDTH-14, SCREEN_HEIGHT-1);
      print("x");
      return;
  }
  if (percent >= -50) {
    drawBitmap(SCREEN_WIDTH-18, SCREEN_HEIGHT-16, wifi_bits[0], 16, 16, WHITE_COLOR);
  } else if (percent >= -60) {
    drawBitmap(SCREEN_WIDTH-18, SCREEN_HEIGHT-16, wifi_bits[1], 16, 16, WHITE_COLOR);
  } else if (percent >= -70) {
    drawBitmap(SCREEN_WIDTH-18, SCREEN_HEIGHT-16, wifi_bits[2], 16, 16, WHITE_COLOR);
  } else {
    drawBitmap(SCREEN_WIDTH-18, SCREEN_HEIGHT-16, wifi_bits[3], 16, 16, WHITE_COLOR);
  }
}

bool IntCtrlDisplay::drawAlert() {
  char* text = nullptr;
  if (IntCtrlRelay::IsAlarmActive())
    text = "ALARM";
  else if (IntCtrlRelay::IsVideoRecorderAlarmActive())
    text = "KAMERA";
  else if (IntCtrlRelay::IsBellRinging())
    text = "DZWONEK";
  else return false;
  setFont(&ArialMT10pt7b);
  int16_t x1, y1;
  uint16_t w, h;
  getTextBounds(text, 0, 63, &x1, &y1, &w, &h);
  setCursor(((SCREEN_WIDTH - w) / 2), SCREEN_HEIGHT-21+h/2);
  if (frameId%2) {
    fillRect(0, 22, SCREEN_WIDTH-1, 41, WHITE_COLOR);
    setTextColor(BLACK_COLOR);
  } else {
    drawRect(0, 22, SCREEN_WIDTH-1, 41, WHITE_COLOR);
    setTextColor(WHITE_COLOR);
  }
  print(text);
  return true;
}

void IntCtrlDisplay::displayConfig() {
  if (!initialized) return;
  restoreFullContrast();
  int16_t x1, y1;
  uint16_t w, h;
  const char* info[4];
  clearDisplay();
  setTextWrap(false);
  setTextColor(WHITE_COLOR);
  String sh(config.getSSID());
  info[0] = "Tryb konfiguracyjny";
  info[1] = "Po^[cz si] z sieci[ WiFi";
  info[2] = sh.c_str();
  info[3] = "192.168.4.1"; //config.getIp().toString().c_str();
  int32_t y=-4;
  for (int i=0 ; i<4 ; i++) {
    setFont((i<3)? &ArialMT5pt7b : &ArialMT7pt7b);
    getTextBounds(info[i], 0, 0, &x1, &y1, &w, &h);
    y+=h+5;
    if ((frameId%3==0) && i==0) continue;
    setCursor(max((SCREEN_WIDTH - w) / 2 - x1, 0), y);
    print(info[i]);
  }
  display();
}

void IntCtrlDisplay::drawSuplaLogo() {
  if (!initialized) return;
  for (int i = -SCREEN_WIDTH; i<=0; i+=8) {
    clearDisplay();
    drawBitmap(i, 0, supla_logo_bits, 128, 64, WHITE_COLOR);
    setFont(&ArialMT5pt7b);
    setTextColor(WHITE_COLOR);
    char* text = "Powered by";
    int16_t x1, y1;
    uint16_t w, h;
    getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    setCursor(SCREEN_WIDTH - w - 8, 8);
    print(text); 
    text = "SuplaDevice";
    getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    setCursor(SCREEN_WIDTH - w - 8, BOTTOM_ROW-2);
    print(text); 
    display();
    delay(15);
  }
}


bool IntCtrlDisplay::restoreFullContrast() { 
  bool result = screenOff;
  if (screenOff)  {
    ssd1306_command(SSD1306_DISPLAYON);
    screenOff = false;
  }
  if (currentContrast<255) {
    ssd1306_command(SSD1306_SETCONTRAST);
    ssd1306_command(157); //max 157
    currentContrast = 255;
  }
  lastAction = millis();
  return result;
};

void IntCtrlDisplay::dimContrast() {
  if (IntCtrlRelay::IsRestoreDisplayRelayActive()) return; 
  if ((millis()-lastAction>DIM_TIME) && (currentContrast>1)) {
    ssd1306_command(SSD1306_SETCONTRAST);
    ssd1306_command(1); //max 157
    currentContrast = 1;
  }
  if ((millis()-lastAction>OFF_TIME) && !screenOff)
  {
    ssd1306_command(SSD1306_DISPLAYOFF);
    screenOff = true;
  }
};
