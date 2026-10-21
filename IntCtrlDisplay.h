#ifndef INTCTRLDISPLAY_H
#define INTCTRLDISPLAY_H

#include <Adafruit_GFX.h>
#include <Wire.h>
#include "Definitions.h"

#define TOP_ROW 24
#define MIDDLE_ROW 38
#define BOTTOM_ROW 63

enum ActionIconId {
  NO_ACTION,
  LIGHTS_ON,
  LIGHTS_OFF,
  FRONT_DOOR_OPEN,
  GATE_OPEN,
  GARAGE_DOOR_OPEN
};

#define ACTION_DISPLAY_TIME_S 10

#ifdef USE_SH1106G
#include <Adafruit_SH110X.h>
class IntCtrlDisplay : public Adafruit_SH1106G {
  public:
    IntCtrlDisplay() : Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET) {
#endif

#ifdef USE_SSD1306
#include <Adafruit_SSD1306.h>
class IntCtrlDisplay : public Adafruit_SSD1306 {
  public:
    IntCtrlDisplay() : Adafruit_SSD1306(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET) {};
#endif
  private:
    bool initialized = false;
    int32_t lastAction = millis();
    int32_t lastDisplayUpdate = 0;
    int32_t currentContrast = 255;
    bool screenOff = false;
    int8_t sensorPage = 0;
    int32_t enterPageTime = 0;
    int32_t actionIconStart = 0;
    ActionIconId actionIcon = NO_ACTION;
  protected:
    bool drawAlert();
    void drawNetworkIcon();
    void displayConfig();
    void drawDisplay();
    void drawSensor();
    void drawHood();
    void drawIcons();
    void drawActionIcon();
    bool getInitialized() { return initialized; };
  public:
    bool Initialize();
    void iterate();
    void drawSuplaLogo();
    bool restoreFullContrast();
    void dimContrast();
    void setupAction(ActionIconId actionId);

    String P(const char* txt) {
      String temp(txt==nullptr?"":txt);
      temp.replace("Ą", "A"); temp.replace("ą", "[");
      temp.replace("Ć", "C"); temp.replace("ć", "\\");
      temp.replace("Ę", "E"); temp.replace("ę", "]");
      temp.replace("Ł", "L"); temp.replace("ł", "^");
      temp.replace("Ń", "N"); temp.replace("ń", "_");
      temp.replace("Ó", "O"); temp.replace("ó", "`");
      temp.replace("Ś", "S"); temp.replace("ś", "{");
      temp.replace("Ź", "Z"); temp.replace("ź", "|");
      temp.replace("Ż", "Z"); temp.replace("ż", "}");
      return temp;
    } 
};

#endif // #ifndef INTCTRLRELAYS_H
