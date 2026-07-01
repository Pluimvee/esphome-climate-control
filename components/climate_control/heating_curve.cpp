#include "climate_control.h"

// Copyright (c) 2026 Pluimvee

#include "esphome/core/log.h"

namespace esphome {
namespace climate_control {

static const char *const TAG = "climate.curve";

static const float STOOKLIJN_W55    = 1.175f;
static const float DEFAULT_FACTOR_A = 0.55f;
static const float DEFAULT_FACTOR_B = 0.45f;
static const float DEFAULT_FACTOR_C = 0.35f;

HeatingCurve::HeatingCurve()
  : factor_a_(DEFAULT_FACTOR_A), factor_b_(DEFAULT_FACTOR_B), factor_c_(DEFAULT_FACTOR_C), factor_(NAN) {}

void HeatingCurve::update(Temperature &current, Temperature &target, Temperature &outside, Temperature &setpoint) {
  const float t_inside  = current.get();
  const float t_outside = outside.get();
  const float t_target  = target.get();

  if (std::isnan(t_inside) || std::isnan(t_outside) || std::isnan(t_target))
    return;

  const float error_outside = t_target - t_outside;
  const float error_inside  = t_target - t_inside;

  float factor = factor_b_ * error_inside;      // internal correction factor
  if (error_outside < 0.0f) factor *= -1.0f;    // if outside is warmer than target, we need cooling
  if (factor < 0.0f) factor = 0.0f;             // factor should not be negative
  factor += factor_a_;                          // base factor to reach target using outside
  if (factor > STOOKLIJN_W55) factor = STOOKLIJN_W55; // top limit
  factor_ = factor;                             // store the used factor

  float curve = error_outside;
  if (factor_c_ > 0.0f && t_target > 0.0f)
    curve = factor_c_ / t_target * std::pow(error_outside, 2.0f) + (1.0f - factor_c_) * error_outside;

  const float new_setpoint = std::max(t_target + factor * curve, 0.0f);
  setpoint.set(new_setpoint);
  ESP_LOGI(TAG, "target: %.1f/%.1f, factor: %.3f, setpoint: %.1f/%.1f",
           t_target, target.trend(), factor_, setpoint.get(), setpoint.trend());
}

}  // namespace climate_control
}  // namespace esphome
