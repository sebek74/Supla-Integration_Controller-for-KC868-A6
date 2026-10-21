#include <supla/sensor/DS18B20.h>
#include <supla/control/relay.h>
#include <supla/sensor/binary.h>
#include <supla/storage/storage.h>
#include <supla/sensor/thermometer.h>
#include <RTClib.h>
#include "IntCtrlSensors.h"
#include "IntCtrlConfig.h"
#include "IntCtrlRelays.h"

#define TEMP_PARAM_HEADER "DALLAS"
extern IntCtrlConfig config;
DallasTemperature dallas;

void IntCtrlSensors::Initialize() {
  Supla::Sensor::DS18B20* zeroSensor = new Supla::Sensor::DS18B20(ONE_WIRE_GPIO);  // get 1st sensor, initialize pin
  if (zeroSensor->getValue()==TEMPERATURE_NOT_AVAILABLE)  {
    delete zeroSensor;
    return;
  }
  dallas = zeroSensor->getHwSensors();
  
  DeviceAddress address;
  dallas.setWaitForConversion(true);
  dallas.requestTemperatures();
  dallas.setWaitForConversion(false);
  for (int i = 0; i < dallas.getDeviceCount(); i++) {
    if (i==0) 
      sensors[0] = zeroSensor;
    else {
      dallas.getAddress(address, i);
      sensors[i] = new Supla::Sensor::DS18B20(ONE_WIRE_GPIO, address);
    }
    sensors[i]->setInitialCaption(config.getSensorName(i));
    sensors[i]->getChannel()->setChannelNumber(RelayId::_LastRelay * 3 + i);
    sensorAmount++;
    SUPLA_LOG_DEBUG("Temp sensor %d (%02X%02X%02X%02X%02X%02X%02X%02X) added", i,
                    address[0], address[1], address[2], address[3], address[4], address[5], address[6], address[7]);
  }
}

bool IntCtrlSensors::getDeviceAdress(int32_t index, DeviceAddress& address) {
  if (index < dallas.getDeviceCount())
    return dallas.getAddress(address, index);
  return false;
};

bool IntCtrlSensors::getDeviceAdress(int32_t index, char* result, int32_t len) {
  DeviceAddress address;
  if (index < dallas.getDeviceCount()) {
    if (!dallas.getAddress(address, index)) return false;
    snprintf(
      result, len,
      "{0x%02X%02X%02X%02X%02X%02X%02X%02X}",
      address[0],
      address[1],
      address[2],
      address[3],
      address[4],
      address[5],
      address[6],
      address[7]);
    return true;
  } 
  return false;
}

const char* IntCtrlSensors::getDeviceDescription(int32_t index) {
  if (index >= sensorAmount)
    return "";
  return config.getSensorName(index);
}

double IntCtrlSensors::getTemp(int32_t index) {
  if (index < dallas.getDeviceCount())
    return dallas.getTempCByIndex(index);
  return TEMPERATURE_NOT_AVAILABLE;
};

void IntCtrlSensors::setupSensorCloudData() {
  for (int i = 0; i < sensorAmount; i++) {
    char key[16], value[64];
    snprintf(key, 16, "%s%d", TEMP_PARAM_HEADER, i);
    if (!Supla::Storage::ConfigInstance()->getString(key, value, 64))
      sprintf(value, "Termometr %d", i + 1);
    sensors[i]->setInitialCaption(value);
  }
}