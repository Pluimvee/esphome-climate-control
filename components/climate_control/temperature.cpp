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

// calculates the mean of a deque of floats, returns NAN if the deque is empty
static float deque_mean_(const std::deque<float> &buf) {
  if (buf.empty()) return NAN;
  float sum = 0.0f;
  for (float v : buf) sum += v;
  return sum / (float) buf.size();
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
}

// returns true if the temperature has expired; clears stats on expiry
bool Temperature::expired() {
  if (this->max_age_ms_ == 0) return false;  // never expires

  if (this->last_valid_ms_ == 0 || (millis() - this->last_valid_ms_) > this->max_age_ms_) {
    this->short_stats_.clear();
    this->long_stats_.clear();
    this->sample_count_ = 0;
    this->cached_median_ = NAN;
    this->cached_long_mean_ = NAN;
    return true;
  }
  return false;
}

// returns the cached median; NAN until enough samples or after expiry
float Temperature::get() {
  if (this->expired()) return NAN;
  return this->cached_median_;
}

// returns the cached trend (°C/30min); NAN until both buffers have enough data
float Temperature::trend() {
  if (this->expired() || this->sample_interval_ms_ == 0)
    return NAN;
  if (std::isnan(this->cached_median_) || std::isnan(this->cached_long_mean_))
    return NAN;
  const float horizon_ms = (float)(STATS_BUFFER_SIZE * STATS_BUFFER_SIZE) * this->sample_interval_ms_;
  const float delta = (this->cached_median_ - this->cached_long_mean_) / horizon_ms * (60.0f * 60.0f * 1000.0f);
  return std::fabs(delta) < 0.2f ? 0.0f : delta;
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

  // Every STATS_BUFFER_SIZE samples push the current median into long-term stats.
  // We use median for short_stats_ (~1.4 min window) to filter raw sensor outliers.
  // We use mean for long_stats_ (~17 min window) because its entries are already
  // median-filtered (outlier-free). Mean properly weights the full window: median
  // would pick only the middle sample (~8 min ago), causing a decelerating trend
  // (fast early drop, plateau later) to be underestimated — mean pulls the baseline
  // toward the older high values and thus gives a trend closer to the true rate.
  if (this->sample_count_ % STATS_BUFFER_SIZE == 0 && !std::isnan(this->cached_median_)) {
    bounded_push_(this->long_stats_, this->cached_median_, STATS_BUFFER_SIZE);
    this->cached_long_mean_ = deque_mean_(this->long_stats_);
  }
  this->last_valid_ms_ = now;
  return true;
}

}  // namespace climate_control
}  // namespace esphome
