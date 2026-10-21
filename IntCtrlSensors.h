#ifndef INTCTRLSENSORS_H
#define INTCTRLSENSORS_H

#include "supla/sensor/thermometer.h"
#include <string>
#include "Definitions.h"

class IntCtrlSensors {
  private:
    Supla::Sensor::Thermometer* sensors[MAX_SENSOR_AMOUNT];
    int32_t sensorAmount = 0;
  public:
    void Initialize();
    double getTemp(int32_t index);
    int32_t getSensorAmount() { return sensorAmount; };
    bool getDeviceAdress(int32_t index, DeviceAddress& address);
    bool getDeviceAdress(int32_t index, char* result, int32_t len);
    const char* getDeviceDescription(int32_t index);
    void setupSensorCloudData();
};
#endif //SPRINKLESENSORS