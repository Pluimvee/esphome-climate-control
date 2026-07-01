#include "climate_control.h"

// Copyright (c) 2026 Pluimvee
// ESPHome component wiring — setup() and update()

#include <algorithm>
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace climate_control {

static const char *const TAG = "climate.control";

void ClimateControl::setup() {
  //                   min,    max, max_age(min), max_delta(°C/s)
  this->inside.setup(  10.0f,  40.0f,  5, 0.05f );  // room temp changes slowly
  this->outside.setup(-15.0f,  40.0f,  5, 0.05f );  // outside temp changes slowly
  this->inlet.setup(   10.0f,  55.0f,  5 );         // no delta check (defrost spikes)
  this->outlet.setup(  10.0f,  55.0f,  5 );         // no delta check (defrost spikes)
  this->setpoint.setup( 0.0f,  55.0f,  5 );         // computed value, no delta check (0 = no minimum)
  this->target.setup(  18.0f,  25.0f,  0 );         // never expires, no delta check

  if (this->room_sensor_)
    this->room_sensor_->add_on_state_callback([this](float) { this->room_pub_ms_ = millis(); });
  if (this->outside_sensor_)
    this->outside_sensor_->add_on_state_callback([this](float) { this->outside_pub_ms_ = millis(); });
  if (this->inlet_sensor_)
    this->inlet_sensor_->add_on_state_callback([this](float) { this->inlet_pub_ms_ = millis(); });
  if (this->outlet_sensor_)
    this->outlet_sensor_->add_on_state_callback([this](float) { this->outlet_pub_ms_ = millis(); });

  ESP_LOGI(TAG, "ClimateControl initialized");
}

void ClimateControl::update() {
  // 1. Sync input sensors → members
  // Numbers are always polled (user-set, no publish event).
  // Sensor inputs are only fed when the source has published since our last update()
  // so that last_valid_ms_ is not refreshed during OT comm errors / sensor outages.
  if (this->target_number_)   this->target.set(this->target_number_->state);
  if (this->factor_a_number_) this->curve.factorA(this->factor_a_number_->state);
  if (this->factor_b_number_) this->curve.factorB(this->factor_b_number_->state);
  if (this->factor_c_number_) this->curve.factorC(this->factor_c_number_->state);
  const uint32_t prev = this->last_update_ms_;
  this->last_update_ms_ = millis();
  if (this->room_sensor_)    { if (this->room_pub_ms_    > prev) this->inside.set(this->room_sensor_->state);    else if (prev) ESP_LOGW(TAG, "No update since last cycle: room"); }
  if (this->outside_sensor_) { if (this->outside_pub_ms_ > prev) this->outside.set(this->outside_sensor_->state); else if (prev) ESP_LOGW(TAG, "No update since last cycle: outside"); }
  if (this->inlet_sensor_)   { if (this->inlet_pub_ms_   > prev) this->inlet.set(this->inlet_sensor_->state);    else if (prev) ESP_LOGW(TAG, "No update since last cycle: inlet"); }
  if (this->outlet_sensor_)  { if (this->outlet_pub_ms_  > prev) this->outlet.set(this->outlet_sensor_->state);  else if (prev) ESP_LOGW(TAG, "No update since last cycle: outlet"); }
  ESP_LOGI(TAG, "inside: %.1f/%.1f, outside: %.1f/%.1f, inlet: %.1f/%.1f, outlet: %.1f/%.1f",
    this->inside.get(),  this->inside.trend(),
    this->outside.get(), this->outside.trend(),
    this->inlet.get(),   this->inlet.trend(),
    this->outlet.get(),  this->outlet.trend());

  // 2. Update heating curve → setpoint
  this->curve.update(this->inside, this->target, this->outside, this->setpoint);

  // 3. Update operating mode
  this->operating.update(this->inside, this->outside, this->inlet, this->outlet, this->setpoint, this->target);

  // 3b. When system disabled: override operating mode — force CH and Cooling off
  if (this->enable_switch_ && !this->enable_switch_->state) {
    this->operating.enable_CH = false;
    this->operating.enable_Cooling = false;
  }

  // 4. Publish output sensors
  auto publish = [&](const std::string &key, float value) {
    auto it = this->sensors_.find(key);
    if (it != this->sensors_.end()) it->second->publish_state(value);
  };
  auto publish_temp = [&](const std::string &key, Temperature &temp) {
    publish(key,              temp.get());
    publish(key + "_source",  temp.source());
    publish(key + "_trend",   temp.trend());
    publish(key + "_samples", (float) temp.sample_count());
  };
  publish_temp("inside",   this->inside);
  publish_temp("outside",  this->outside);
  publish_temp("inlet",    this->inlet);
  publish_temp("outlet",   this->outlet);
  publish_temp("setpoint", this->setpoint);
  publish_temp("target",   this->target);
  publish("factor",  this->curve.factor());

  // 5. Direct switch control
  if (this->ch_switch_) {
    if (this->operating.enable_CH) this->ch_switch_->turn_on();
    else                           this->ch_switch_->turn_off();
  }
  if (this->cooling_switch_) {
    if (this->operating.enable_Cooling) this->cooling_switch_->turn_on();
    else                                this->cooling_switch_->turn_off();
  }

  // 6. Room temperature and setpoint — informational to boiler; skip when NaN
  if (this->room_output_) {
    float t = this->inside.get();
    if (!std::isnan(t)) this->room_output_->set_level(t / 100.0f);
  }
  if (this->room_setpoint_output_) {
    // When disabled: send room temp as setpoint so OT slave sees no demand
    bool disabled = this->enable_switch_ && !this->enable_switch_->state;
    float t = disabled ? this->inside.get() : this->target.get();
    if (!std::isnan(t)) this->room_setpoint_output_->set_level(t / 100.0f);
  }

  // 7. Setpoint output — only when heating or cooling is active
  if (this->setpoint_output_) {
    if (this->operating.enable_CH || this->operating.enable_Cooling) {
      float sp = this->setpoint.get();
      this->setpoint_output_->set_level(std::isnan(sp) ? 0.0f : sp / 100.0f);
    } else {
      this->setpoint_output_->set_level(0.0f);
    }
  }
}

}  // namespace climate_control
}  // namespace esphome
