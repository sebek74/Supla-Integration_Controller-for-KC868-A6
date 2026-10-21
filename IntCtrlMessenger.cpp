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

