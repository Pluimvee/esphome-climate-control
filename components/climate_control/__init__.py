# Copyright (c) 2026 Pluimvee
# Climate Control ESPHome component schema

# First some basic configuration
#--------------------------------------------
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number, output, sensor, switch
from esphome.const import CONF_ID

# -- Component dependencies --
AUTO_LOAD = ["sensor", "number", "switch", "output"]

# -- YAML keys --
CONF_ROOM_RAW_SENSOR = "room_raw_sensor"
CONF_OUTSIDE_RAW_SENSOR = "outside_raw_sensor"
CONF_INLET_RAW_SENSOR = "inlet_raw_sensor"
CONF_OUTLET_RAW_SENSOR = "outlet_raw_sensor"
CONF_TARGET_NUMBER = "target_number"
CONF_FACTOR_A_NUMBER = "factor_a_number"
CONF_FACTOR_B_NUMBER = "factor_b_number"
CONF_FACTOR_C_NUMBER = "factor_c_number"
CONF_CH_ENABLE_SWITCH = "ch_enable_switch"
CONF_COOLING_ENABLE_SWITCH = "cooling_enable_switch"
CONF_T_SET_OUTPUT = "t_set_output"
CONF_T_ROOM_SET_OUTPUT = "t_room_set_output"
CONF_T_ROOM_OUTPUT = "t_room_output"
CONF_ANTIPENDEL = "antipendel"

# -- Namespace and class binding --
climate_control_ns = cg.esphome_ns.namespace("climate_control")
ClimateControl = climate_control_ns.class_("ClimateControl", cg.PollingComponent)

# -- Configuration schema --
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ClimateControl),
        cv.Required(CONF_ROOM_RAW_SENSOR): cv.use_id(sensor.Sensor),
        cv.Required(CONF_OUTSIDE_RAW_SENSOR): cv.use_id(sensor.Sensor),
        cv.Required(CONF_INLET_RAW_SENSOR): cv.use_id(sensor.Sensor),
        cv.Required(CONF_OUTLET_RAW_SENSOR): cv.use_id(sensor.Sensor),
        cv.Required(CONF_TARGET_NUMBER): cv.use_id(number.Number),
        cv.Required(CONF_FACTOR_A_NUMBER): cv.use_id(number.Number),
        cv.Required(CONF_FACTOR_B_NUMBER): cv.use_id(number.Number),
        cv.Required(CONF_FACTOR_C_NUMBER): cv.use_id(number.Number),
        cv.Required(CONF_CH_ENABLE_SWITCH): cv.use_id(switch.Switch),
        cv.Required(CONF_COOLING_ENABLE_SWITCH): cv.use_id(switch.Switch),
        cv.Required(CONF_T_SET_OUTPUT): cv.use_id(output.FloatOutput),
        cv.Required(CONF_T_ROOM_SET_OUTPUT): cv.use_id(output.FloatOutput),
        cv.Required(CONF_T_ROOM_OUTPUT): cv.use_id(output.FloatOutput),
        cv.Optional(CONF_ANTIPENDEL, default="30min"): cv.positive_time_period_milliseconds,
    }
).extend(cv.polling_component_schema("10s"))


# -- Code generation --
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_room_raw_sensor(await cg.get_variable(config[CONF_ROOM_RAW_SENSOR])))
    cg.add(var.set_outside_raw_sensor(await cg.get_variable(config[CONF_OUTSIDE_RAW_SENSOR])))
    cg.add(var.set_inlet_raw_sensor(await cg.get_variable(config[CONF_INLET_RAW_SENSOR])))
    cg.add(var.set_outlet_raw_sensor(await cg.get_variable(config[CONF_OUTLET_RAW_SENSOR])))
    cg.add(var.set_target_number(await cg.get_variable(config[CONF_TARGET_NUMBER])))
    cg.add(var.set_factor_a_number(await cg.get_variable(config[CONF_FACTOR_A_NUMBER])))
    cg.add(var.set_factor_b_number(await cg.get_variable(config[CONF_FACTOR_B_NUMBER])))
    cg.add(var.set_factor_c_number(await cg.get_variable(config[CONF_FACTOR_C_NUMBER])))
    cg.add(var.set_ch_enable_switch(await cg.get_variable(config[CONF_CH_ENABLE_SWITCH])))
    cg.add(var.set_cooling_enable_switch(await cg.get_variable(config[CONF_COOLING_ENABLE_SWITCH])))
    cg.add(var.set_t_set_output(await cg.get_variable(config[CONF_T_SET_OUTPUT])))
    cg.add(var.set_t_room_set_output(await cg.get_variable(config[CONF_T_ROOM_SET_OUTPUT])))
    cg.add(var.set_t_room_output(await cg.get_variable(config[CONF_T_ROOM_OUTPUT])))
    cg.add(var.set_antipendel_ms(config[CONF_ANTIPENDEL]))
