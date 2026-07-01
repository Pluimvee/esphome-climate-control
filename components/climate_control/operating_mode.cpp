#include "climate_control.h"

// Copyright (c) 2026 Pluimvee

#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace climate_control {

static const char *const TAG = "climate.mode";
static float round1(float value) { return round(10.0f * value) / 10.0f; }

// based on the temperatures we set the operating mdoe (heating on/off, cooling on/off) 
// with some hysteresis to avoid rapid switching
// 
/*  
  COOLING ON LOGIC
    When outside > 25 and inside > target and trend of inside is rising, we switch on cooling

  COOLING OFF LOGIC
    When outside < 25 and trend of inside is falling, we switch off cooling
    
  HEAT ON LOGIC
                            ===================== trend of TSet / hour ===================
    target  current  error  -1,0  -0,8  -0,6  -0,4  -0,2    0   0,2   0,4   0,6   0,8   1,0
    21      22        1.0
    21      21,8      0,8                                                                ON
    21      21,6      0,6                                                          ON    ON
    21      21,4      0,4                                                    ON    ON    ON
    21      21,2      0,2                                              ON    ON    ON    ON
    21      21        0.0                                        ON    ON    ON    ON    ON
    21      20,8     -0,2                                   ON   ON    ON    ON    ON    ON
    21      20,6     -0,4                             ON    ON   ON    ON    ON    ON    ON
    21      20,4     -0,6                       ON    ON    ON   ON    ON    ON    ON    ON
    21      20,2     -0,8               ON      ON    ON    ON   ON    ON    ON    ON    ON
    21      20       -1.0        ON     ON      ON    ON    ON   ON    ON    ON    ON    ON
                            ON   ON     ON      ON    ON    ON   ON    ON    ON    ON    ON
    ==> ON = (trend > error)

  HEAT OFF LOGIC
    When inlet >= setpoint + 0.1, we switch off heating as the floor reached target temperature
    We may add a trend condition here as well, to avoid turning off when the setpoint is still rising

*/
// NOTE: all decision logic is based on a safe result when a value is NaN and compared!
bool OperatingMode::update(Temperature &inside, Temperature &outside,
                            Temperature &inlet,  Temperature &outlet,
                            Temperature &setpoint, Temperature &target) {
  const uint32_t now = millis();

  if ((now - this->last_change_ms_) < this->anti_short_cycle_ms)
    return false;

  const float t_inside  = inside.get();
  const float t_outside = outside.get();
  const float t_inlet   = inlet.get();
  const float t_set     = setpoint.get();
  const float t_target  = target.get();
  const float trend     = setpoint.trend();

  if (!this->enable_Cooling && t_outside > 25.0f && t_inside > t_target && inside.trend() > 0.0f) {

    this->enable_Cooling = true;
    this->enable_CH = false;
    this->last_change_ms_ = now;
    ESP_LOGI(TAG, "Switch ON Cooling, Tout=%.1f>25, Tin=%.2f>Ttarget=%.1f, trend=%.2f/h",
             t_outside, t_inside, t_target, inside.trend());
    return true;
  }
  if (this->enable_Cooling && !(t_outside >= 25.0f) && !(inside.trend() >= 0.0f)) { // note the save format to handle NaN
    this->enable_Cooling = false;
    this->last_change_ms_ = now;
    ESP_LOGI(TAG, "Switch OFF Cooling, Tout=%.1f<25 and Tin trend=%.2f/h downward", t_outside, inside.trend());
    return true;
  }
  if (!this->enable_CH) {
    const float error = round1(t_inside - t_target);
    const float t = round1(trend);
    if (t > error && t_set > t_target) {
      this->enable_CH = true;
      this->enable_Cooling = false;
      this->last_change_ms_ = now;
      ESP_LOGI(TAG, "Switch ON Heating, Tinside error (%0.1f) < trend-TSet (%0.1f/h) AND Tset=%.1f > Ttarget=%.1f", error, t, t_set, t_target);
      return true;
    }
  }
  if (this->enable_CH && round1(t_inlet - t_set) > 0.1f) {
    this->enable_CH = false;
    this->last_change_ms_ = now;
    ESP_LOGI(TAG, "Switch OFF Heating, Tinlet (%0.1f) above Tset (%0.1f)", t_inlet, t_set);
    return true;
  }
  return false;
}

}  // namespace climate_control
}  // namespace esphome
