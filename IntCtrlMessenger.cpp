#include <stdint.h>
#include <SuplaDevice.h>
#include <supla/control/relay.h>
#include <supla/control/virtual_relay.h>
#include <supla/sensor/binary.h>
#include <supla/log_wrapper.h>
#include "IntCtrlMessenger.h"
#include <supla/device/notifications.h>

void IntCtrlMessenger::initialize() {
  Supla::Notification::RegisterNotification(-1); 
  initialized = true;
}

const char* IntCtrlMessenger::getDateTime() {
    time_t now = time(nullptr);
    struct tm* ti = localtime(&now);
    snprintf(datetime, sizeof(datetime), "%4d-%02d-%02d %d:%02d:%02d", ti->tm_year+1900, ti->tm_mon+1, ti->tm_mday, ti->tm_hour, ti->tm_min, ti->tm_sec);  
    return datetime;
  }

void IntCtrlMessenger::sendMessage(const char* code, const char* title, uint32_t param1,  uint32_t param2) {
  if (!initialized) return;
  sendMessage(-1, code, title, param1, param2);
}

void IntCtrlMessenger::sendMessage(int16_t context, const char* code, const char* title, uint32_t param1,  uint32_t param2) {
  if (!initialized) return;
  char text[64];
  snprintf(text, sizeof(text), code, param1, param2);
  Supla::Notification::Send(context, title, text);
  SUPLA_LOG_DEBUG("Messenger: %s\n", text);
}

void IntCtrlMessenger::sendMessage(const char* code, const char* title, const char* param1) {
  if (!initialized) return;
  sendMessage(-1, code, title, param1);
}

void IntCtrlMessenger::sendMessage(int16_t context, const char* code, const char* title, const char* param1) {
  if (!initialized) return;
  char text[64];
  snprintf(text, sizeof(text), code, param1);
  Supla::Notification::Send(context, title, text);
  SUPLA_LOG_DEBUG("Messenger: %s\n", text);
}

