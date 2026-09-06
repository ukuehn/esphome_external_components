#include "ds2438.h"
#include "esphome/core/log.h"


/*
 * Component to interface with DS2438 smart battery monitor
 * Data sheet at
 * https://www.analog.com/media/en/technical-documentation/data-sheets/DS2438.pdf
 */


namespace esphome::ds2438 {

static const char *const TAG = "dallas.ds2438";

static const uint8_t DALLAS_MODEL_DS2438 = 0x26;
static const uint8_t DALLAS_COMMAND_WRITE_SCRATCHPAD = 0x4E;
static const uint8_t DALLAS_COMMAND_READ_SCRATCHPAD = 0xBE;
static const uint8_t DALLAS_COMMAND_COPY_SCRATCHPAD = 0x48;
static const uint8_t DALLAS_COMMAND_RECALL_MEMORY = 0xB8;
static const uint8_t DALLAS_COMMAND_CONVERT_T = 0x44;
static const uint8_t DALLAS_COMMAND_CONVERT_V = 0xB4;


static const uint8_t STATUS_CONFIG_IAD = 0x01;
static const uint8_t STATUS_CONFIG_CA = 0x02;
static const uint8_t STATUS_CONFIG_EE = 0x04;
static const uint8_t STATUS_CONFIG_AD = 0x08;
static const uint8_t STATUS_CONFIG_TB = 0x10;
static const uint8_t STATUS_CONFIG_NVB = 0x20;
static const uint8_t STATUS_CONFIG_ADB = 0x40;



// Timing of DS2438 according to page 22 of data sheet:
// Temperature conversion takes up to 10ms
// A/D conversion takes up to 4ms
// EEPROM writes take up to 10ms
//
// from page 7 of data sheet
// current sense (Vsens) conversion run every 27.46ms, equivalent
// to 36.41 measurements per second.
//



void DallasSmartBatteryMonitor::dump_config() {
  ESP_LOGCONFIG(TAG, "Dallas Smart Battery Monitor (DS2438):");
  if (this->address_ == 0) {
    ESP_LOGW(TAG, "  Unable to select an address");
    return;
  }
  LOG_ONE_WIRE_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("   ", "Vsense:", this->vsense_sensor_);
  LOG_SENSOR("   ", "Temperature:", this->temperature_sensor_);
  LOG_SENSOR("   ", "Vad", this->vad_sensor_);
  LOG_SENSOR("   ", "Vdd",  this->vdd_sensor_);
}


void DallasSmartBatteryMonitor::setup() {
  if (!this->check_address_or_index_()) {
    return;
  }

  // to make sure that the timeout IDs are unique per instance
  timeoutbaseid = reinterpret_cast<uint32_t>(this);

  ESP_LOGD(TAG, "setup(): recall memory command on page 0");
  if (this->send_command_(DALLAS_COMMAND_RECALL_MEMORY)) {
    this->bus_->write8(0);  // place status page (0) in scratchpad
  } else {
    return;
  }
  ESP_LOGD(TAG, "setup(): read scratchpad command on page 0");
  if (this->send_command_(DALLAS_COMMAND_READ_SCRATCHPAD)) {
    this->bus_->write8(0);  // read scratchpad page 0
    for (uint8_t i = 0;  i < sizeof(this->scratchpad_);  i++) {
      this->scratchpad_[i] = this->bus_->read8();
    }
    uint8_t crc = crc8(this->scratchpad_, 8);
    if (crc != this->scratchpad_[8]) {
      ESP_LOGW(TAG, "setup(): crc error on scratchpad data");
      return;
    }
  }

  this->scratchpad_[0] &= ~(STATUS_CONFIG_CA|STATUS_CONFIG_EE);

  // enable continuous current / Vsense measurement
  ESP_LOGD(TAG, "setup(): enabling current measurement");
  this->scratchpad_[0] |= STATUS_CONFIG_IAD;
  
  // configure voltage AD to measure VDD
  if (this->vad_sensor_) {
    ESP_LOGD(TAG, "setup(): switching to Vad measurement");
    this->scratchpad_[0] &= ~STATUS_CONFIG_AD;
  } else {
    ESP_LOGD(TAG, "setup(): switching to Vdd measurement");
    this->scratchpad_[0] |= STATUS_CONFIG_AD;
  }

  ESP_LOGD(TAG, "setup(): status/conf reg = 0x%02x", this->scratchpad_[0]);
  // write back scratchpad data
  // since the status/config register is at byte 0 of page 0 only
  // a single byte needs to be written back to the device
  if (this->send_command_(DALLAS_COMMAND_WRITE_SCRATCHPAD)) {
    this->bus_->write8(0);  // write to scratchpad page 0
    this->bus_->write8(this->scratchpad_[0]); // write first byte of page
  } else {
    return;
  }

  ESP_LOGD(TAG, "setup(): copy scratchpad to activate");
  if (this->send_command_(DALLAS_COMMAND_COPY_SCRATCHPAD)) {
    this->bus_->write8(0);  // read scratchpad page 0
  } else {
    return;
  }
  ESP_LOGD(TAG, "setup(): done.");
}


bool DallasSmartBatteryMonitor::read_scratchpad_(uint8_t page) {
  ESP_LOGD(TAG, "read_scratchpad_(%u)", page);
  if (this->send_command_(DALLAS_COMMAND_RECALL_MEMORY)) {
    this->bus_->write8(page);  // place status page (0) in scratchpad
  } else {
    return false;
  }
  if (this->send_command_(DALLAS_COMMAND_READ_SCRATCHPAD)) {
    this->bus_->write8(page);  // read scratchpad page 0
    for (uint8_t i = 0;  i < sizeof(this->scratchpad_);  i++) {
      this->scratchpad_[i] = this->bus_->read8();
    }
    uint8_t crc = crc8(this->scratchpad_, 8);
    if (crc != this->scratchpad_[8]) {
      ESP_LOGW(TAG, "read_scratchpad_(): crc error on scratchpad data");
      return false;
    }
    ESP_LOGD(TAG, "   got scratchpad = "
             "[ %02x %02x %02x %02x %02x %02x %02x %02x ]",
             this->scratchpad_[0], this->scratchpad_[1],
             this->scratchpad_[2], this->scratchpad_[3],
             this->scratchpad_[4], this->scratchpad_[5],
             this->scratchpad_[6], this->scratchpad_[7]);
  } else {
    return false;
  }
  return true;
}


void DallasSmartBatteryMonitor::write_config_(uint8_t newconfig) {
  ESP_LOGD(TAG, "write_config_(%02x) ... ", newconfig);
  if (!this->read_scratchpad_(0)) {
    ESP_LOGD(TAG, "write_config_(%02x): failed to read scratchpad", newconfig);
    return;
  }
  this->scratchpad_[0] = newconfig;
  this->send_command_(DALLAS_COMMAND_WRITE_SCRATCHPAD);
  this->bus_->write8(0);  // page 0 to scratchpad
  for (int i = 0;  i < 8;  i++) {
    this->bus_->write8(this->scratchpad_[i]);
  }
  this->send_command_(DALLAS_COMMAND_COPY_SCRATCHPAD);
  this->bus_->write8(0);  // page 0 to scratchpad
  ESP_LOGD(TAG, "write_config_(%02x): copy triggered, config = %02x.",
           newconfig, this->scratchpad_[0]);
}


void DallasSmartBatteryMonitor::publish_vsense_() {
  if (!this->vsense_sensor_) {
    return;
  }
  // get reading from current ADC, which is a 10-bit 2's complement
  // value with scratchpad[5] as LSB value and scratchpad[6] as MSB.
  // Caveat: the datasheet (page 6) indicates a 10 bit value, but the
  // formula gives 4096 as multiplier, hinting to 12 bits (wrongly).
  // each digital value is given as 0.2441mV, and total range of the
  // current AD is listed (page 28) as 250mV, which is 1014 *
  // 0.2441mV.  So we use 10 bits here, 8 bits from LSB, 2 bits from
  // MSB, sign from top 6 bits of MSB.
  //
  int16_t current_reg_val = (this->scratchpad_[6] & 0x03) << 8
                            | this->scratchpad_[5];
  if (this->scratchpad_[6] & 0x04) {
    current_reg_val = current_reg_val-1024;
  }
  float vsense = 0.2441 * current_reg_val;
  ESP_LOGD(TAG, "Vsense >> %0.4f mV", vsense);
  this->vsense_sensor_->publish_state(vsense);
}


void DallasSmartBatteryMonitor::publish_temperature_() {
  if (!this->temperature_sensor_) {
    return;
  }
  // Temperature register is in pad[1] as fractions and pad[2] as
  // 7-bit value plus sign. Unit is °C. LSB has lowest 3 bits
  // always 0.
  int16_t regval = (this->scratchpad_[2] << 8) | this->scratchpad_[1];
  float temp_c = regval / 256.0;
  ESP_LOGD(TAG, "temperature >> %0.1f °C", temp_c);
  this->temperature_sensor_->publish_state(temp_c);
}


void DallasSmartBatteryMonitor::publish_vdd_() {
  if (!this->vdd_sensor_) {
    return;
  }
  // 10 bit value in voltage register, units of 10mV
  int16_t regval = ((this->scratchpad_[4] & 0x03) << 8)
                   | (this->scratchpad_[3]);
  float voltage = 0.01 * regval;
  ESP_LOGD(TAG, "Vdd >> %0.2f V", voltage);
  this->vdd_sensor_->publish_state(voltage);
}


void DallasSmartBatteryMonitor::publish_vad_() {
  if (!this->vad_sensor_) {
    return;
  }
  // 10 bit value in voltage register, units of 10mV
  int16_t regval = ((this->scratchpad_[4] & 0x03) << 8)
                   | (this->scratchpad_[3]);
  float voltage = 0.01 * regval;
  ESP_LOGD(TAG, "Vad >> %0.2f V", voltage);
  this->vad_sensor_->publish_state(voltage);
}



/* 
 * Since the ds2438 chip takes some time for the execution of
 * temperature and voltage conversion as well as the EEPROM/SRAM write
 * to reconfigure the ADC voltage input channel, the update function
 * by itself cannot do this if both voltage values are configured. It would
 * take much longer than accepted for the update() function to last.
 *
 * The approach taken here is to break the execution of all the actions
 * into 4 stages, with stage 0 being the call to the update() function.
 *
 * Stage 0 (update()) has dual logic: if only vsense is configured, which
 *     is continuously measured, take a direct read and be done. If more is
 *     configured, i.e. voltage and/or temperature measurement, trigger
 *     these conversions and defer the readout to state 1.
 * 
 * Stage 1 reads out vsense, temperature and first voltage conversion
 *     (if configured). If a second voltage conversion is required,
 *     the other voltage channel is selected (takes an EEPROM/SRAM
 *     write, and triggering a deferred stage 2.
 *
 * Stage 2 triggers the voltage conversion and deferred stage 3.
 *
 * Stage 3 finally can read the second voltage, switch back to first
 *     voltage channel (always Vad if both channels are configured).
 *     This last time a deferred execution is not needed, as next
 *     access to the chip will only happen during next update(), which
 *     is far enough in the future.
 *
 * Requirement: minimum update interval is about 0.5 sec.
 */

void DallasSmartBatteryMonitor::update() {
  ESP_LOGD(TAG, "update() called for %llx", this->address_);
  if (this->address_ == 0) {
    return;
  }
  
  this->status_clear_warning();

  uint16_t millis_for_conversion = 0;
  if (this->temperature_sensor_) {
    ESP_LOGD(TAG, "update(): triggering temperature conversion...");
    this->send_command_(DALLAS_COMMAND_CONVERT_T);
    millis_for_conversion = 10;
  }
  if (this->vad_sensor_ || this->vdd_sensor_) {
    ESP_LOGD(TAG, "update(): triggering voltage conversion...");
    this->send_command_(DALLAS_COMMAND_CONVERT_V);
    if (millis_for_conversion < 4) {
      millis_for_conversion = 4;
    }
  }

  if (millis_for_conversion == 0) {
    if (this->vsense_sensor_) {
      // only vsense sensor configured, data does arrive automatically
      // anyway, so not timeout necessary.
      ESP_LOGD(TAG, "update(): no wait required, reading directly");
      if (!this->read_scratchpad_(0)) {
        return;
      }
      this->publish_vsense_();
    }
  } else {
    this->set_timeout(this->timeoutbaseid, 50, [this]() {update_stage1_();});
  }
}

void DallasSmartBatteryMonitor::update_stage1_() {
  ESP_LOGD(TAG, "update_stage1_() for %llx...", this->address_);
  if (!this->read_scratchpad_(0)) {
    return;
  }
  if (this->vsense_sensor_) {
    this->publish_vsense_();
  }
  if (this->temperature_sensor_) {
    this->publish_temperature_();
  }
  uint8_t vsrc = this->scratchpad_[0] & STATUS_CONFIG_AD;
  ESP_LOGD(TAG, "update_stage1_() vsrc = %02x", vsrc);
  if (vsrc) {
    if (this->vdd_sensor_) {
      this->publish_vdd_();
    }
  } else {
    if (this->vad_sensor_) {
      this->publish_vad_();
    }
  }
  if (this->vad_sensor_ && this->vdd_sensor_) {
    // If both voltage sensors are configured stage 2 (and consequently
    // also stage 3 are required. So switch the ADC channel and trigger
    // stage 2.
    uint8_t newconfig = this->scratchpad_[0];
    newconfig ^= STATUS_CONFIG_AD;
    write_config_(newconfig);
    this->set_timeout(this->timeoutbaseid, 50, 
                      [this]() {update_stage2_(); }
                      );
  }
}


void DallasSmartBatteryMonitor::update_stage2_() {
  ESP_LOGD(TAG, "update_stage2_(%llx): triggering voltage conversion...",
           this->address_);
  this->send_command_(DALLAS_COMMAND_CONVERT_V);
  this->set_timeout(this->timeoutbaseid, 50, 
                    [this]() {update_stage3_(); }
                    );
}


void DallasSmartBatteryMonitor::update_stage3_() {
  ESP_LOGD(TAG, "update_stage3_() for %llx...", this->address_);
  if (!this->read_scratchpad_(0)) {
    return;
  }
  uint8_t vsrc = this->scratchpad_[0] & STATUS_CONFIG_AD;
  ESP_LOGD(TAG, "update_stage3_(): vsrc = %02x", vsrc);
  if (vsrc) {
    if (this->vdd_sensor_) {
      this->publish_vdd_();
    }
  } else {
    if (this->vad_sensor_) {
      this->publish_vad_();
    }
  }
  if (this->vad_sensor_ && this->vdd_sensor_) {
    // Make sure to read Vad next, i.e. first voltage in next update()
    // if the current channel is Vdd (AD bit is 1 in status/config register)
    // invert it. Otherwise just do nothing, as by chance the intended
    // order did not happen this round.
    if (vsrc) {
      uint8_t newconfig = this->scratchpad_[0];
      newconfig ^= STATUS_CONFIG_AD;
      write_config_(newconfig);
    }
  }
  // No further delayed action, as we simply wait for next round's
  // update() to do the job after sufficient waiting time.
  // This assumption limits the udpate interval to about >= 0.5 sec.
}



} // namespace esphome::ds2438
