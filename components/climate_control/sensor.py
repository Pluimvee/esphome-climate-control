# Copyright (c) 2026 Pluimvee
from esphome.components import sensor
from esphome import config_validation as cv
import esphome.codegen as cg
from esphome.const import (
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
)
from . import ClimateControl, climate_control_ns

CONF_CLIMATE_CONTROL_ID = "climate_control_id"

_temp_schema = sensor.sensor_schema(
    unit_of_measurement=UNIT_CELSIUS,
    device_class=DEVICE_CLASS_TEMPERATURE,
    state_class=STATE_CLASS_MEASUREMENT,
    accuracy_decimals=1,
)
_trend_schema = sensor.sensor_schema(
    unit_of_measurement="°C/h",
    state_class=STATE_CLASS_MEASUREMENT,
    accuracy_decimals=2,
)
_count_schema = sensor.sensor_schema(
    state_class=STATE_CLASS_MEASUREMENT,
    accuracy_decimals=0,
)
_factor_schema = sensor.sensor_schema(
    state_class=STATE_CLASS_MEASUREMENT,
    accuracy_decimals=2,
)

# Temperature sensor with optional sub-sensors as attributes
_temp_ext_schema = _temp_schema.extend({
    cv.Optional("source"):  _temp_schema,
    cv.Optional("trend"):   _trend_schema,
    cv.Optional("samples"): _count_schema,
})

SENSOR_DEFINITIONS = {
    "inside":       _temp_ext_schema,
    "outside":      _temp_ext_schema,
    "inlet":        _temp_ext_schema,
    "outlet":       _temp_ext_schema,
    "setpoint":     _temp_ext_schema,
    "target":       _temp_ext_schema,
    "factor":       _factor_schema,
}

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_CLIMATE_CONTROL_ID): cv.use_id(ClimateControl),
    **{cv.Optional(key): schema for key, schema in SENSOR_DEFINITIONS.items()}
})

async def to_code(config):
    parent = await cg.get_variable(config[CONF_CLIMATE_CONTROL_ID])
    for key in SENSOR_DEFINITIONS:
        if key not in config:
            continue
        entry = config[key]
        sens = await sensor.new_sensor(entry)
        cg.add(parent.register_sensor(key, sens))
        for sub in ("source", "trend", "samples"):
            if sub in entry:
                sub_sens = await sensor.new_sensor(entry[sub])
                cg.add(parent.register_sensor(key + "_" + sub, sub_sens))
