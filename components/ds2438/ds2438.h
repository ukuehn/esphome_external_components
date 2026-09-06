#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/one_wire/one_wire.h"


/*
 * Component to interface with DS2438 smart battery monitor
 * Data sheet at
 * https://www.analog.com/media/en/technical-documentation/data-sheets/DS2438.pdf
 */




namespace esphome::ds2438 {

class DallasSmartBatteryMonitor :
      public PollingComponent,
      public one_wire::OneWireDevice {

 public:
  void setup() override;
  void update() override;
  void dump_config() override;

  void set_vsense_sensor(sensor::Sensor *vsense_sensor) {
    this->vsense_sensor_ = vsense_sensor;
  }
  void set_temperature_sensor(sensor::Sensor *temperature_sensor) {
    this->temperature_sensor_ = temperature_sensor;
  }
  void set_vad_sensor(sensor::Sensor *vad_sensor) {
    this->vad_sensor_ = vad_sensor;
  }
  void set_vdd_sensor(sensor::Sensor *vdd_sensor) {
    this->vdd_sensor_ = vdd_sensor;
  }

 protected:
  uint8_t scratchpad_[9] = {0};
  sensor::Sensor *vsense_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *vad_sensor_{nullptr};
  sensor::Sensor *vdd_sensor_{nullptr};

  bool read_scratchpad_(uint8_t page);
  void write_config_(uint8_t newconfig);
  void publish_sensor_data_();
  void publish_vsense_();
  void publish_temperature_();
  void publish_vdd_();
  void publish_vad_();

  void update_stage1_();
  void update_stage2_();
  void update_stage3_();

  uint32_t timeoutbaseid = 0; 

};

}  // namespace esphome::ds2438
