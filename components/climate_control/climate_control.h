#pragma once

// Copyright (c) 2026 Pluimvee
// Climate Control runtime interface

#include <cmath>
#include <cstdint>
#include <deque>
#include <map>
#include <string>

#include "esphome/core/component.h"
#include "esphome/components/number/number.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/output/float_output.h"

namespace esphome {
namespace climate_control {

// -- Temperature measurement with statistics --
class Temperature {
 public:
  void setup(float min, float max, uint32_t max_age_minutes, float max_delta_psec = 0.0f);
  bool expired();
  float get();           // cached median; NAN if expired or insufficient samples
  float trend();         // normalized to °C/30min based on actual sample interval
  float source() const { return short_stats_.empty() ? NAN : short_stats_.back(); }
  uint32_t sample_count() const { return sample_count_; }
  bool set(float value);

 private:
  float min_value_{0.0f};
  float max_value_{0.0f};
  float max_delta_psec_{0.0f};
  float cached_median_{NAN};
  float cached_long_mean_{NAN};
  uint32_t max_age_ms_{0};
  uint32_t last_valid_ms_{0};
  uint32_t sample_interval_ms_{0};
  uint32_t sample_count_{0};
  std::deque<float> short_stats_;
  std::deque<float> long_stats_;
};

// -- Weather-compensated heating curve --
class HeatingCurve {
 public:
  HeatingCurve();
  float factor() const { return factor_; }
  void factorA(float v) { factor_a_ = std::clamp(v, 0.0f, 1.5f); }
  void factorB(float v) { factor_b_ = std::clamp(v, 0.0f, 1.5f); }
  void factorC(float v) { factor_c_ = std::clamp(v, 0.0f, 1.5f); }
  void update(Temperature &current, Temperature &target, Temperature &outside, Temperature &setpoint);
 private:
  float factor_a_, factor_b_, factor_c_, factor_;
};

// -- OpenTherm operating mode --
class OperatingMode {
 public:
  bool enable_CH{false};
  bool enable_DHW{false};
  bool enable_Cooling{false};
  bool enable_OTC{false};
  uint32_t anti_short_cycle_ms{30UL * 60UL * 1000UL};

  bool update(Temperature &inside, Temperature &outside,
              Temperature &inlet, Temperature &outlet,
              Temperature &setpoint, Temperature &target);
 private:
  uint32_t last_change_ms_{0};
};

// -- Climate control component --
class ClimateControl : public PollingComponent {
 public:
  void setup() override;
  void update() override;

  // -- Input bindings --
  void set_room_sensor(sensor::Sensor *value)    { this->room_sensor_ = value; }
  void set_outside_sensor(sensor::Sensor *value) { this->outside_sensor_ = value; }
  void set_inlet_sensor(sensor::Sensor *value)   { this->inlet_sensor_ = value; }
  void set_outlet_sensor(sensor::Sensor *value)  { this->outlet_sensor_ = value; }
  void set_target_number(number::Number *value)      { this->target_number_ = value; }
  void set_factor_a_number(number::Number *value)    { this->factor_a_number_ = value; }
  void set_factor_b_number(number::Number *value)    { this->factor_b_number_ = value; }
  void set_factor_c_number(number::Number *value)    { this->factor_c_number_ = value; }
  void set_anti_short_cycle_ms(uint32_t value) { this->operating.anti_short_cycle_ms = value; }

  // -- Output sensor registration --
  void register_sensor(const std::string &key, sensor::Sensor *sensor) { this->sensors_[key] = sensor; }

  // -- Switch + output wiring --
  void set_enable_switch(switch_::Switch *sw)           { this->enable_switch_ = sw; }
  void set_ch_switch(switch_::Switch *sw)               { this->ch_switch_ = sw; }
  void set_cooling_switch(switch_::Switch *sw)          { this->cooling_switch_ = sw; }
  void set_setpoint_output(output::FloatOutput *o)      { this->setpoint_output_ = o; }
  void set_room_output(output::FloatOutput *o)          { this->room_output_ = o; }
  void set_room_setpoint_output(output::FloatOutput *o) { this->room_setpoint_output_ = o; }

  // -- Public state --
  Temperature inside;
  Temperature outside;
  Temperature inlet;
  Temperature outlet;
  Temperature setpoint;
  Temperature target;

  HeatingCurve curve;
  OperatingMode operating;

 protected:
  sensor::Sensor *room_sensor_{nullptr};
  sensor::Sensor *outside_sensor_{nullptr};
  sensor::Sensor *inlet_sensor_{nullptr};
  sensor::Sensor *outlet_sensor_{nullptr};

  // Timestamps set by on_state callbacks; update() only calls Temperature::set()
  // when the sensor published since the previous update() run.
  uint32_t last_update_ms_{0};
  uint32_t room_pub_ms_{0};
  uint32_t outside_pub_ms_{0};
  uint32_t inlet_pub_ms_{0};
  uint32_t outlet_pub_ms_{0};

  number::Number *target_number_{nullptr};
  number::Number *factor_a_number_{nullptr};
  number::Number *factor_b_number_{nullptr};
  number::Number *factor_c_number_{nullptr};

  std::map<std::string, sensor::Sensor *> sensors_;
  switch_::Switch *enable_switch_{nullptr};
  switch_::Switch *ch_switch_{nullptr};
  switch_::Switch *cooling_switch_{nullptr};
  output::FloatOutput *setpoint_output_{nullptr};
  output::FloatOutput *room_output_{nullptr};
  output::FloatOutput *room_setpoint_output_{nullptr};
};

}  // namespace climate_control
}  // namespace esphome
