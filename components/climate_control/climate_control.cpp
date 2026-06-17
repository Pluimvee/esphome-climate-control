#include "climate_control.h"

// Copyright (c) 2026 Pluimvee
// Climate Control runtime implementation

// First some basic includes
//--------------------------------------------
#include <algorithm>

#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace climate_control {

// -- Internal constants --
static const char *const TAG = "climate_control";
static const size_t STATS_BUFFER_SIZE = 10;
static const uint32_t SHORT_STATS_MS = 30UL * 1000UL;
static const uint32_t LONG_STATS_MS = SHORT_STATS_MS * STATS_BUFFER_SIZE;

static const float STOOKLIJN_W55 = 1.175f;
static const float DEFAULT_FACTOR_A = 0.55f;
static const float DEFAULT_FACTOR_B = 0.45f;
static const float DEFAULT_FACTOR_C = 0.35f;

// -- TempStats helpers --
void ClimateControl::TempStats::configure(float initial_value, uint32_t max_age_minutes, float min, float max,
                                           float max_diff, float k) {
  this->current = initial_value;
  this->min_value = min;
  this->max_value = max;
  this->max_diff_per_sec = max_diff;
  this->spike_k = k;
  this->max_age_ms = max_age_minutes * 60UL * 1000UL;
  this->last_valid_ms = 0;
  this->last_sample_ms = 0;
  this->last_short_stat_ms = 0;
  this->last_long_stat_ms = 0;
  this->short_stats.clear();
  this->long_stats.clear();
}

bool ClimateControl::TempStats::valid(uint32_t now) const {
  if (this->max_age_ms == 0)
    return true;
  if (this->last_valid_ms == 0)
    return false;
  return (now - this->last_valid_ms) <= this->max_age_ms;
}

float ClimateControl::TempStats::value(uint32_t now) const {
  if (!this->valid(now))
    return NAN;
  return this->current;
}

float ClimateControl::TempStats::deque_average_(const std::deque<float> &buffer) const {
  if (buffer.empty())
    return NAN;
  float sum = 0.0f;
  for (float item : buffer)
    sum += item;
  return sum / static_cast<float>(buffer.size());
}

float ClimateControl::TempStats::deque_stddev_(const std::deque<float> &buffer, float mean) const {
  if (buffer.size() < 2)
    return 0.0f;
  float sum = 0.0f;
  for (float item : buffer) {
    const float d = item - mean;
    sum += d * d;
  }
  return std::sqrt(sum / static_cast<float>(buffer.size()));
}

void ClimateControl::TempStats::bounded_push_(std::deque<float> &buffer, float value, size_t max_size) {
  buffer.push_back(value);
  while (buffer.size() > max_size)
    buffer.pop_front();
}

float ClimateControl::TempStats::average() const {
  if (this->last_valid_ms == 0)
    return NAN;  // No valid measurement yet
  const float avg = this->deque_average_(this->short_stats);
  if (!std::isnan(avg))
    return avg;
  return this->current;
}

float ClimateControl::TempStats::trend() const {
  // Need ≥2 entries in both buffers for a meaningful trend (~5 minutes of data).
  if (this->short_stats.size() < 2 || this->long_stats.size() < 2)
    return NAN;

  const float avg_short = this->deque_average_(this->short_stats);
  const float avg_long = this->deque_average_(this->long_stats);
  if (std::isnan(avg_short) || std::isnan(avg_long))
    return 0.0f;

  const float delta = avg_short - avg_long;
  if (std::fabs(delta) < 0.1f)
    return 0.0f;
  return delta;
}

bool ClimateControl::TempStats::add_measurement(float value, uint32_t now_ms) {
  if (std::isnan(value))
    return false;

  if (!this->valid(now_ms)) {
    this->short_stats.clear();
    this->long_stats.clear();
  }

  if (this->max_value != 0.0f && value > this->max_value)
    return false;
  if (this->min_value != 0.0f && value < this->min_value)
    return false;

  if (this->max_diff_per_sec > 0.0f && this->last_valid_ms != 0 && this->valid(now_ms)) {
    const uint32_t dt_ms = now_ms - this->last_valid_ms;
    if (dt_ms > 0) {
      const float diff_per_sec = std::fabs(value - this->current) * 1000.0f / static_cast<float>(dt_ms);
      if (diff_per_sec > this->max_diff_per_sec)
        return false;
    }
  }

  bool spike = false;
  if (this->spike_k > 0.0f && this->short_stats.size() >= STATS_BUFFER_SIZE) {
    const float avg = this->deque_average_(this->short_stats);
    const float stddev = this->deque_stddev_(this->short_stats, avg);
    if (stddev > 0.0f && std::fabs(value - avg) > (this->spike_k * stddev))
      spike = true;
  }

  if (this->last_short_stat_ms == 0 || (now_ms - this->last_short_stat_ms) >= SHORT_STATS_MS) {
    this->bounded_push_(this->short_stats, value, STATS_BUFFER_SIZE);
    this->last_short_stat_ms = now_ms;
  }

  if (this->last_long_stat_ms == 0 || (now_ms - this->last_long_stat_ms) >= LONG_STATS_MS) {
    const float avg_short = this->deque_average_(this->short_stats);
    if (!std::isnan(avg_short))
      this->bounded_push_(this->long_stats, avg_short, STATS_BUFFER_SIZE);
    this->last_long_stat_ms = now_ms;
  }

  if (!spike) {
    this->current = value;
    this->last_valid_ms = now_ms;
  }
  this->last_sample_ms = now_ms;
  return true;
}

// -- Component lifecycle --
void ClimateControl::setup() {
  this->inside_.configure(20.0f, 5, 10.0f, 40.0f, 0.02f, 3.0f);
  this->outside_.configure(10.0f, 5, -15.0f, 40.0f, 0.02f, 3.0f);
  this->inlet_.configure(20.0f, 5, 10.0f, 55.0f, 1.0f, 3.0f);
  this->outlet_.configure(20.0f, 5, 10.0f, 55.0f, 1.0f, 3.0f);
  this->setpoint_.configure(20.0f, 5, 10.0f, 55.0f, 0.0f, 0.0f);

  this->enable_ch_ = false;
  this->enable_cooling_ = false;
  this->last_mode_change_ms_ = millis();

  ESP_LOGI(TAG, "ClimateControl initialized");
}

// -- Control calculations --
float ClimateControl::clamp_(float value, float min_value, float max_value) const {
  return std::max(min_value, std::min(max_value, value));
}

float ClimateControl::calculate_setpoint_() const {
  const float inside_avg = this->inside_.average();
  const float outside_avg = this->outside_.average();
  const float target = this->target_temperature();

  if (std::isnan(inside_avg) || std::isnan(outside_avg) || std::isnan(target))
    return NAN;

  float factor_a = DEFAULT_FACTOR_A;
  float factor_b = DEFAULT_FACTOR_B;
  float factor_c = DEFAULT_FACTOR_C;

  if (this->factor_a_number_ != nullptr && !std::isnan(this->factor_a_number_->state))
    factor_a = this->clamp_(this->factor_a_number_->state, 0.0f, 1.5f);
  if (this->factor_b_number_ != nullptr && !std::isnan(this->factor_b_number_->state))
    factor_b = this->clamp_(this->factor_b_number_->state, 0.0f, 1.5f);
  if (this->factor_c_number_ != nullptr && !std::isnan(this->factor_c_number_->state))
    factor_c = this->clamp_(this->factor_c_number_->state, 0.0f, 1.5f);

  const float delta_outside = target - outside_avg;
  const float delta_inside = target - inside_avg;

  float factor = factor_b * delta_inside;
  if (delta_outside < 0.0f)
    factor *= -1.0f;
  if (factor < 0.0f)
    factor = 0.0f;
  factor += factor_a;
  if (factor > STOOKLIJN_W55)
    factor = STOOKLIJN_W55;
  this->current_factor_ = factor;

  float curve = delta_outside;
  if (factor_c > 0.0f && target > 0.0f)
    curve = factor_c / target * std::pow(delta_outside, 2.0f) + (1.0f - factor_c) * delta_outside;

  return target + factor * curve;
}

////////////////////////////////////////////////////////////////////////////////////////////
/*  SWITCHING ON LOGIC
                            ==================== trend of TSet / 30 min ===================
    target	current	error 	-0,5  -0,4  -0,3	-0,2	-0,1	0  0,1   0,2   0,3   0,4   0,5
    21	    22	     1.0											
    21	    21,8	   0,8										                                          ON
    21	    21,6	   0,6									                                      ON	  ON
    21	    21,4	   0,4								                                  ON	  ON	  ON
    21	    21,2	   0,2							                              ON	  ON	  ON	  ON
    21	    21	     0.0					                            ON	  ON	  ON	  ON	  ON
    21	    20,8	  -0,2						                      ON	ON	  ON	  ON	  ON	  ON
    21	    20,6	  -0,4					                  ON	  ON	ON	  ON	  ON	  ON	  ON
    21	    20,4	  -0,6				                ON  ON	  ON	ON	  ON	  ON	  ON  	ON
    21	    20,2	  -0,8			            ON	  ON  ON	  ON	ON	  ON	  ON	  ON	  ON
    21	    20	    -1.0	        	ON	  ON	  ON  ON	  ON	ON	  ON	  ON  	ON    ON
                              ON		ON	  ON	  ON  ON	  ON	ON	  ON	  ON	  ON    ON
    ==> ON = (2*trend > error)
*/
////////////////////////////////////////////////////////////////////////////////////////////
// use 1 decimal precision
float round1(float value) {
  return round(10.0f * value) / 10.0f;  // rounding to 1 decimal
}

////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////
bool ClimateControl::update_operating_mode_(float setpoint_value, uint32_t now) {
  if ((now - this->last_mode_change_ms_) < this->antipendel_ms_)
    return false;

  const float inside = this->inside_.value(now);
  const float outside = this->outside_.value(now);
  const float inlet = this->inlet_.value(now);
  const float target = this->target_temperature();
  const float trend = this->setpoint_.trend();

  if (!this->enable_cooling_ && !std::isnan(inside) && !std::isnan(outside) && inside > 25.0f && outside > 25.0f) {
    this->enable_cooling_ = true;
    this->last_mode_change_ms_ = now;
    ESP_LOGI(TAG, "Cooling enabled");
    return true;
  }

  if (this->enable_cooling_ && !std::isnan(inside) && inside < 25.0f) {
    this->enable_cooling_ = false;
    this->last_mode_change_ms_ = now;
    ESP_LOGI(TAG, "Cooling disabled");
    return true;
  }

  if (!this->enable_ch_ && !std::isnan(inside) && !std::isnan(target) && !std::isnan(setpoint_value)) {
    const float error = round1(inside - target);
    const float rounded_trend = round1(trend);
    if ((2.0f * rounded_trend) > error) {
      this->enable_ch_ = true;
      this->last_mode_change_ms_ = now;
      ESP_LOGI(TAG, "Heating enabled (error=%.2f trend=%.2f)", error, rounded_trend);
      return true;
    }
  }

  if (this->enable_ch_ && !std::isnan(inlet) && !std::isnan(setpoint_value)) {
    const float delta = round1(inlet - setpoint_value);
    if (delta > 0.1f) {
      this->enable_ch_ = false;
      this->last_mode_change_ms_ = now;
      ESP_LOGI(TAG, "Heating disabled (inlet above setpoint)");
      return true;
    }
  }

  return false;
}

// -- OpenTherm output writes --
void ClimateControl::write_outputs_(float setpoint_value) {
  const float room = this->inside_temperature();
  const float target = this->target_temperature();

  // ESPHome FloatOutput::set_level() expects a normalized 0-1 value;
  // the OpenTherm output converts it back via lerp(min, max, level).
  if (this->t_room_output_ != nullptr && !std::isnan(room))
    this->t_room_output_->set_level(this->clamp_(room, 0.0f, 40.0f) / 40.0f);

  if (this->t_room_set_output_ != nullptr && !std::isnan(target))
    this->t_room_set_output_->set_level((this->clamp_(target, 18.0f, 25.0f) - 18.0f) / 7.0f);

  if (this->t_set_output_ != nullptr) {
    float ot_setpoint = 0.0f;
    if ((this->enable_ch_ || this->enable_cooling_) && !std::isnan(setpoint_value))
      ot_setpoint = this->clamp_(setpoint_value, 0.0f, 55.0f) / 55.0f;
    this->t_set_output_->set_level(ot_setpoint);
  }

  if (this->ch_enable_switch_ != nullptr) {
    if (this->enable_ch_)
      this->ch_enable_switch_->turn_on();
    else
      this->ch_enable_switch_->turn_off();
  }

  if (this->cooling_enable_switch_ != nullptr) {
    if (this->enable_cooling_)
      this->cooling_enable_switch_->turn_on();
    else
      this->cooling_enable_switch_->turn_off();
  }
}

// -- Main update loop --
void ClimateControl::update() {
  const uint32_t now = millis();

  if (this->room_raw_sensor_ != nullptr)
    this->inside_.add_measurement(this->room_raw_sensor_->state, now);
  if (this->outside_raw_sensor_ != nullptr)
    this->outside_.add_measurement(this->outside_raw_sensor_->state, now);
  if (this->inlet_raw_sensor_ != nullptr)
    this->inlet_.add_measurement(this->inlet_raw_sensor_->state, now);
  if (this->outlet_raw_sensor_ != nullptr)
    this->outlet_.add_measurement(this->outlet_raw_sensor_->state, now);

  const float setpoint_value = this->calculate_setpoint_();
  this->setpoint_.add_measurement(setpoint_value, now);

  this->update_operating_mode_(setpoint_value, now);
  this->write_outputs_(setpoint_value);
}

// -- Public getters --
float ClimateControl::inside_temperature() const { return this->inside_.value(millis()); }
float ClimateControl::outside_temperature() const { return this->outside_.value(millis()); }
float ClimateControl::inlet_temperature() const { return this->inlet_.value(millis()); }
float ClimateControl::outlet_temperature() const { return this->outlet_.value(millis()); }
float ClimateControl::setpoint_temperature() const { return this->setpoint_.value(millis()); }

float ClimateControl::target_temperature() const {
  if (this->target_number_ == nullptr || std::isnan(this->target_number_->state))
    return NAN;
  return this->target_number_->state;
}

float ClimateControl::inside_trend() const { return this->inside_.trend(); }
float ClimateControl::outside_trend() const { return this->outside_.trend(); }
float ClimateControl::inlet_trend() const { return this->inlet_.trend(); }
float ClimateControl::outlet_trend() const { return this->outlet_.trend(); }
float ClimateControl::setpoint_trend() const { return this->setpoint_.trend(); }

}  // namespace climate_control
}  // namespace esphome
