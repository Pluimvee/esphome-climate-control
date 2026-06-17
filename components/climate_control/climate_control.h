#pragma once

// Copyright (c) 2026 Pluimvee
// Climate Control runtime interface

// First some basic includes
//--------------------------------------------
#include <cmath>
#include <cstdint>
#include <deque>
#include <vector>

#include "esphome/core/component.h"
#include "esphome/components/number/number.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"

namespace esphome {
namespace climate_control {

// -- Climate control component --
class ClimateControl : public PollingComponent {
 public:
  void setup() override;
  void update() override;

  void set_room_raw_sensor(sensor::Sensor *value) { this->room_raw_sensor_ = value; }
  void set_outside_raw_sensor(sensor::Sensor *value) { this->outside_raw_sensor_ = value; }
  void set_inlet_raw_sensor(sensor::Sensor *value) { this->inlet_raw_sensor_ = value; }
  void set_outlet_raw_sensor(sensor::Sensor *value) { this->outlet_raw_sensor_ = value; }
  void set_target_number(number::Number *value) { this->target_number_ = value; }
  void set_factor_a_number(number::Number *value) { this->factor_a_number_ = value; }
  void set_factor_b_number(number::Number *value) { this->factor_b_number_ = value; }
  void set_factor_c_number(number::Number *value) { this->factor_c_number_ = value; }
  void set_ch_enable_switch(switch_::Switch *value) { this->ch_enable_switch_ = value; }
  void set_cooling_enable_switch(switch_::Switch *value) { this->cooling_enable_switch_ = value; }
  void set_t_set_output(output::FloatOutput *value) { this->t_set_output_ = value; }
  void set_t_room_set_output(output::FloatOutput *value) { this->t_room_set_output_ = value; }
  void set_t_room_output(output::FloatOutput *value) { this->t_room_output_ = value; }
  void set_antipendel_ms(uint32_t value) { this->antipendel_ms_ = value; }

  float inside_temperature() const;
  float outside_temperature() const;
  float inlet_temperature() const;
  float outlet_temperature() const;
  float target_temperature() const;
  float setpoint_temperature() const;

  float inside_trend() const;
  float outside_trend() const;
  float inlet_trend() const;
  float outlet_trend() const;
  float setpoint_trend() const;

  bool heating_enabled() const { return this->enable_ch_; }
  bool cooling_enabled() const { return this->enable_cooling_; }
  float current_factor() const { return this->current_factor_; }

 protected:
  // -- Temperature statistics helper --
  struct TempStats {
    float current{NAN};
    float min_value{0.0f};
    float max_value{0.0f};
    float max_diff_per_sec{0.0f};
    float spike_k{0.0f};
    uint32_t max_age_ms{0};
    uint32_t last_valid_ms{0};
    uint32_t last_sample_ms{0};
    uint32_t last_short_stat_ms{0};
    uint32_t last_long_stat_ms{0};
    std::deque<float> short_stats;
    std::deque<float> long_stats;

    void configure(float initial_value, uint32_t max_age_minutes, float min, float max, float max_diff, float k);
    bool valid(uint32_t now) const;
    float value(uint32_t now) const;
    float average() const;
    float trend() const;
    bool add_measurement(float value, uint32_t now_ms);

   private:
    float deque_average_(const std::deque<float> &buffer) const;
    float deque_stddev_(const std::deque<float> &buffer, float mean) const;
    void bounded_push_(std::deque<float> &buffer, float value, size_t max_size);
  };

  float calculate_setpoint_() const;
  bool update_operating_mode_(float setpoint_value, uint32_t now);
  void write_outputs_(float setpoint_value);
  float clamp_(float value, float min_value, float max_value) const;

  // -- Input bindings --
  sensor::Sensor *room_raw_sensor_{nullptr};
  sensor::Sensor *outside_raw_sensor_{nullptr};
  sensor::Sensor *inlet_raw_sensor_{nullptr};
  sensor::Sensor *outlet_raw_sensor_{nullptr};

  number::Number *target_number_{nullptr};
  number::Number *factor_a_number_{nullptr};
  number::Number *factor_b_number_{nullptr};
  number::Number *factor_c_number_{nullptr};

  switch_::Switch *ch_enable_switch_{nullptr};
  switch_::Switch *cooling_enable_switch_{nullptr};

  output::FloatOutput *t_set_output_{nullptr};
  output::FloatOutput *t_room_set_output_{nullptr};
  output::FloatOutput *t_room_output_{nullptr};

  // -- Runtime state --
  TempStats inside_;
  TempStats outside_;
  TempStats inlet_;
  TempStats outlet_;
  TempStats setpoint_;

  uint32_t antipendel_ms_{30UL * 60UL * 1000UL};
  uint32_t last_mode_change_ms_{0};

  bool enable_ch_{false};
  bool enable_cooling_{false};
  mutable float current_factor_{NAN};
};

}  // namespace climate_control
}  // namespace esphome
