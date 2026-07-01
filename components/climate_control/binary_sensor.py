# Copyright (c) 2026 Pluimvee
from esphome.components import binary_sensor
from esphome import config_validation as cv
import esphome.codegen as cg
from . import ClimateControl, climate_control_ns

CONF_CLIMATE_CONTROL_ID = "climate_control_id"

BIN_SENSOR_DEFINITIONS = {
    "heating": binary_sensor.binary_sensor_schema(icon="mdi:fire",     device_class="heat"),
    "cooling": binary_sensor.binary_sensor_schema(icon="mdi:snowflake", device_class="cold"),
}

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_CLIMATE_CONTROL_ID): cv.use_id(ClimateControl),
    **{cv.Optional(key): schema for key, schema in BIN_SENSOR_DEFINITIONS.items()}
})

async def to_code(config):
    parent = await cg.get_variable(config[CONF_CLIMATE_CONTROL_ID])
    for key in BIN_SENSOR_DEFINITIONS:
        if key in config:
            sens = await binary_sensor.new_binary_sensor(config[key])
            cg.add(parent.register_binary_sensor(key, sens))
