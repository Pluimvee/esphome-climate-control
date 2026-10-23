#include "climate_control.h"

// Copyright (c) 2026 Pluimvee

#include <algorithm>
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace climate_control {

static const char *const TAG = "climate.temp";
static const size_t STATS_BUFFER_SIZE = 10;

// calculates the median of a deque of floats, returns NAN if the deque is empty
static float deque_median_(std::deque<float> buf) {
  if (buf.empty()) return NAN;
  std::sort(buf.begin(), buf.end());
  const size_t n = buf.size();
  return (n % 2 == 1) ? buf[n / 2] : (buf[n / 2 - 1] + buf[n / 2]) / 2.0f;
}

// least-squares slope in °C/hour of values against their millis() timestamps,
// returns NAN with fewer than 2 points or no time spread. Times are taken
// relative to the oldest entry, so millis() wrap-around is harmless.
static float deque_slope_per_hour_(const std::deque<float> &values, const std::deque<uint32_t> &times) {
  const size_t n = std::min(values.size(), times.size());
  if (n < 2) return NAN;
  const uint32_t t0 = times.front();
  float mean_t = 0.0f, mean_v = 0.0f;
  for (size_t i = 0; i < n; i++) {
    mean_t += (float) (times[i] - t0) / 3600000.0f;
    mean_v += values[i];
  }
  mean_t /= (float) n;
  mean_v /= (float) n;
  float num = 0.0f, den = 0.0f;
  for (size_t i = 0; i < n; i++) {
    const float dt = (float) (times[i] - t0) / 3600000.0f - mean_t;
    num += dt * (values[i] - mean_v);
    den += dt * dt;
  }
  return den > 0.0f ? num / den : NAN;
}

// pushes value to the back of the buffer and pops from the front if the buffer exceeds max_size
static void bounded_push_(std::deque<float> &buffer, float value, size_t max_size) {
  buffer.push_back(value);
  while (buffer.size() > max_size)
    buffer.pop_front();
}

// Temperature class implementation
// configure the class with setup(min, max, max_age_minutes) and then call set(value) to update the temperature.
void Temperature::setup(float min, float max, uint32_t max_age_minutes, float max_delta_psec) {
  this->min_value_ = min;
  this->max_value_ = max;
  this->max_delta_psec_ = max_delta_psec;
  this->max_age_ms_ = max_age_minutes * 60UL * 1000UL;
  this->last_valid_ms_ = 0;
  this->sample_count_ = 0;
  this->short_stats_.clear();
  this->long_stats_.clear();
  this->long_times_.clear();
  this->cached_slope_ = NAN;
}

// returns true if the temperature has expired; clears stats on expiry
bool Temperature::expired() {
  if (this->max_age_ms_ == 0) return false;  // never expires

  if (this->last_valid_ms_ == 0 || (millis() - this->last_valid_ms_) > this->max_age_ms_) {
    this->short_stats_.clear();
    this->long_stats_.clear();
    this->long_times_.clear();
    this->sample_count_ = 0;
    this->cached_median_ = NAN;
    this->cached_slope_ = NAN;
    return true;
  }
  return false;
}

// returns the cached median; NAN until enough samples or after expiry
float Temperature::get() {
  if (this->expired()) return NAN;
  return this->cached_median_;
}

// returns the trend in °C/hour; NAN until the long-term buffer holds 2 entries.
// The slope is fitted over the timestamped long-term medians, so it does not
// depend on the (jittery) last sample interval. The previous version divided
// (median - long mean) by 100 sample intervals, while the two are only ~45
// intervals apart: it reported ~0.45x the real rate.
float Temperature::trend() {
  if (this->expired() || std::isnan(this->cached_slope_))
    return NAN;
  return std::fabs(this->cached_slope_) < 0.2f ? 0.0f : this->cached_slope_;
}

// sets a new temperature value if it's valid and updates the stats, returns true if the value was accepted
bool Temperature::set(float value) {
  if (std::isnan(value))
    return false;
  
  const uint32_t now = millis();
  this->expired();  // clears deques if expired
  if (this->last_valid_ms_ != 0)
    this->sample_interval_ms_ = now - this->last_valid_ms_;

  // validate against limits if they are set (non-zero)
  if (this->max_value_ != 0.0f && value > this->max_value_) {
    ESP_LOGW(TAG, "Rejected %.2f: above max %.2f", value, this->max_value_);
    return false;
  }
  if (this->min_value_ != 0.0f && value < this->min_value_) {
    ESP_LOGW(TAG, "Rejected %.2f: below min %.2f", value, this->min_value_);
    return false;
  }

  // reject if rate of change exceeds max_delta_psec, compared against the cached median
  // only active once the buffer is full (STATS_BUFFER_SIZE entries) so the median is meaningful
  // also skip when the last valid is stale (gap > max_age), e.g. after a sensor outage
  const bool stale = this->max_age_ms_ > 0 && this->sample_interval_ms_ > this->max_age_ms_;
  if (this->max_delta_psec_ > 0.0f && this->short_stats_.size() == STATS_BUFFER_SIZE && this->sample_interval_ms_ > 0 && !stale) {
    float delta_psec = std::fabs(value - this->cached_median_) * 1000.0f / (float) this->sample_interval_ms_;
    if (delta_psec > this->max_delta_psec_) {
      ESP_LOGW(TAG, "Rejected %.2f: %.4f°C/s vs median %.2f exceeds max %.4f°C/s", value, delta_psec, this->cached_median_, this->max_delta_psec_);
      return false;
    }
  }

  // push to short-term stats and immediately update cached median
  bounded_push_(this->short_stats_, value, STATS_BUFFER_SIZE);
  this->cached_median_ = deque_median_(this->short_stats_);
  this->sample_count_++;

  // Every STATS_BUFFER_SIZE samples push the current median, with its timestamp,
  // into long-term stats. The median of short_stats_ (~1.7 min window at 10 s)
  // filters raw sensor outliers; the slope over the long-term entries (~17 min
  // window at 10 s) gives the trend. Every entry lags its push time by the same
  // half short window, so the lag does not bias the slope.
  if (this->sample_count_ % STATS_BUFFER_SIZE == 0 && !std::isnan(this->cached_median_)) {
    bounded_push_(this->long_stats_, this->cached_median_, STATS_BUFFER_SIZE);
    this->long_times_.push_back(now);
    while (this->long_times_.size() > STATS_BUFFER_SIZE)
      this->long_times_.pop_front();
    this->cached_slope_ = deque_slope_per_hour_(this->long_stats_, this->long_times_);
  }
  this->last_valid_ms_ = now;
  return true;
}

}  // namespace climate_control
}  // namespace esphome
