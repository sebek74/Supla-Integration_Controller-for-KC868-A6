#ifndef INTCTRLMESSENGER_h
#define INTCTRLMESSENGER_h

#include <stdint.h>
#include <SuplaDevice.h>

#define MSG_ALARM_GARAGE_LOCKED "Alarm blokuje otwarcie drzwi garażowych"
#define MSG_ALARM_ARMED "Alarm uzbrojony o %s"
#define MSG_ALARM_DISARMED "Alarm rozbrojony o %s"
#define MSG_ALARM_BLOCADES "Załączono blokady - wewnętrzne czujniki ruchu nieaktywne"
#define MSG_VIDEORECORDER_ALARM_TRIGGER "Alarm z rejestratora. Włączenie świateł zewnętrznych."
#define MSG_INTEGRA_ALARM_TRIGGER "Alarm z centrali alarmowej. Włączenie świateł zewnętrznych."
#define MSG_HOOD_TIME_WARNING "Funkcja okapu rekuperatora włączona od %d minut"
#define MSG_HEATERS_OFF "Grzejniki wyłączone po %d godzinach"
#define MSG_GATE_BELL "Dzwonek do furtki o %s"
#define MSG_AIRVENT_BOOST_ACTIVATED "Uruchomiono ręcznie przewietrzanie"

#define MSG_TITLE_ALARM "Alarm"
#define MSG_TITLE_HOOD "Okap"
#define MSG_TITLE_HEATERS "Grzejniki"
#define MSG_TITLE_DOORSTATION "Domofon"

class IntCtrlMessenger {
private:
  bool initialized = false;
  char datetime[32];
public: 
  void initialize();
  void sendMessage(const char* code, const char* title, uint32_t param1=0,  uint32_t param2=0);
  void sendMessage(int16_t context, const char* code, const char* title, uint32_t param1=0,  uint32_t param2=0);
  void sendMessage(const char* code, const char* title, const char* param1);
  void sendMessage(int16_t context, const char* code, const char* title, const char* param1);
  const char* getDateTime();
};

#endif // INTCTRLMESSENGER_h