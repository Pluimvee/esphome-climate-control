# Copyright (c) 2026 Pluimvee
# Climate Control ESPHome component schema

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number, sensor, switch, output
from esphome.const import CONF_ID

# -- Component dependencies --
AUTO_LOAD = ["sensor", "number", "switch", "output"]

# -- YAML keys --
CONF_ROOM_SENSOR = "room_sensor"
CONF_OUTSIDE_SENSOR = "outside_sensor"
CONF_INLET_SENSOR = "inlet_sensor"
CONF_OUTLET_SENSOR = "outlet_sensor"
CONF_TARGET_NUMBER = "target_number"
CONF_FACTOR_A_NUMBER = "factor_a_number"
CONF_FACTOR_B_NUMBER = "factor_b_number"
CONF_FACTOR_C_NUMBER = "factor_c_number"
CONF_ANTI_SHORT_CYCLE = "anti_short_cycle"
CONF_ENABLE_SWITCH = "enable_switch"
CONF_CH_SWITCH = "ch_switch"
CONF_COOLING_SWITCH = "cooling_switch"
CONF_SETPOINT_OUTPUT = "setpoint_output"
CONF_ROOM_OUTPUT = "room_output"
CONF_ROOM_SETPOINT_OUTPUT = "room_setpoint_output"

# -- Namespace and class binding --
climate_control_ns = cg.esphome_ns.namespace("climate_control")
ClimateControl = climate_control_ns.class_("ClimateControl", cg.PollingComponent)

# -- Configuration schema --
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ClimateControl),
        cv.Required(CONF_ROOM_SENSOR): cv.use_id(sensor.Sensor),
        cv.Required(CONF_OUTSIDE_SENSOR): cv.use_id(sensor.Sensor),
        cv.Required(CONF_INLET_SENSOR): cv.use_id(sensor.Sensor),
        cv.Required(CONF_OUTLET_SENSOR): cv.use_id(sensor.Sensor),
        cv.Required(CONF_TARGET_NUMBER): cv.use_id(number.Number),
        cv.Required(CONF_FACTOR_A_NUMBER): cv.use_id(number.Number),
        cv.Required(CONF_FACTOR_B_NUMBER): cv.use_id(number.Number),
        cv.Required(CONF_FACTOR_C_NUMBER): cv.use_id(number.Number),
        cv.Optional(CONF_ANTI_SHORT_CYCLE, default="30min"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_ENABLE_SWITCH): cv.use_id(switch.Switch),
        cv.Optional(CONF_CH_SWITCH): cv.use_id(switch.Switch),
        cv.Optional(CONF_COOLING_SWITCH): cv.use_id(switch.Switch),
        cv.Optional(CONF_SETPOINT_OUTPUT): cv.use_id(output.FloatOutput),
        cv.Optional(CONF_ROOM_OUTPUT): cv.use_id(output.FloatOutput),
        cv.Optional(CONF_ROOM_SETPOINT_OUTPUT): cv.use_id(output.FloatOutput),
    }
).extend(cv.polling_component_schema("10s"))


# -- Code generation --
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_room_sensor(await cg.get_variable(config[CONF_ROOM_SENSOR])))
    cg.add(var.set_outside_sensor(await cg.get_variable(config[CONF_OUTSIDE_SENSOR])))
    cg.add(var.set_inlet_sensor(await cg.get_variable(config[CONF_INLET_SENSOR])))
    cg.add(var.set_outlet_sensor(await cg.get_variable(config[CONF_OUTLET_SENSOR])))
    cg.add(var.set_target_number(await cg.get_variable(config[CONF_TARGET_NUMBER])))
    cg.add(var.set_factor_a_number(await cg.get_variable(config[CONF_FACTOR_A_NUMBER])))
    cg.add(var.set_factor_b_number(await cg.get_variable(config[CONF_FACTOR_B_NUMBER])))
    cg.add(var.set_factor_c_number(await cg.get_variable(config[CONF_FACTOR_C_NUMBER])))
    cg.add(var.set_anti_short_cycle_ms(config[CONF_ANTI_SHORT_CYCLE]))
    if CONF_ENABLE_SWITCH in config:
        cg.add(var.set_enable_switch(await cg.get_variable(config[CONF_ENABLE_SWITCH])))
    if CONF_CH_SWITCH in config:
        cg.add(var.set_ch_switch(await cg.get_variable(config[CONF_CH_SWITCH])))
    if CONF_COOLING_SWITCH in config:
        cg.add(var.set_cooling_switch(await cg.get_variable(config[CONF_COOLING_SWITCH])))
    if CONF_SETPOINT_OUTPUT in config:
        cg.add(var.set_setpoint_output(await cg.get_variable(config[CONF_SETPOINT_OUTPUT])))
    if CONF_ROOM_OUTPUT in config:
        cg.add(var.set_room_output(await cg.get_variable(config[CONF_ROOM_OUTPUT])))
    if CONF_ROOM_SETPOINT_OUTPUT in config:
        cg.add(var.set_room_setpoint_output(await cg.get_variable(config[CONF_ROOM_SETPOINT_OUTPUT])))
