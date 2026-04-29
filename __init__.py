"""ESPHome External Component for HLK-LD6002 Radar Sensor.

Provides: Heart Rate, Breath Rate, Distance sensors via UART.
Source:   https://github.com/phuongnamzz/HLK-LD6002
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart, sensor
from esphome.const import (
    CONF_ID,
    STATE_CLASS_MEASUREMENT,
)

DEPENDENCIES = ["uart"]
AUTO_LOAD = ["sensor"]

ld6002_ns = cg.esphome_ns.namespace("ld6002")
LD6002Component = ld6002_ns.class_("LD6002Component", cg.Component, uart.UARTDevice)

CONF_HEART_RATE = "heart_rate"
CONF_BREATH_RATE = "breath_rate"
CONF_DISTANCE = "distance"
CONF_RUNNING = "running"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(LD6002Component),

            cv.Optional(CONF_HEART_RATE): sensor.sensor_schema(
                unit_of_measurement="bpm",
                icon="mdi:heart-pulse",
                accuracy_decimals=1,
                state_class=STATE_CLASS_MEASUREMENT,
            ),

            cv.Optional(CONF_BREATH_RATE): sensor.sensor_schema(
                unit_of_measurement="bpm",
                icon="mdi:lungs",
                accuracy_decimals=1,
                state_class=STATE_CLASS_MEASUREMENT,
            ),

            cv.Optional(CONF_DISTANCE): sensor.sensor_schema(
                unit_of_measurement="cm",
                icon="mdi:radar",
                accuracy_decimals=1,
                state_class=STATE_CLASS_MEASUREMENT,
            ),

            cv.Optional(CONF_RUNNING): sensor.sensor_schema(
                unit_of_measurement="",
                icon="mdi:run",
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config):
    """Convert YAML configuration to C++ code."""
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    if CONF_HEART_RATE in config:
        sens = await sensor.new_sensor(config[CONF_HEART_RATE])
        cg.add(var.set_heart_rate_sensor(sens))

    if CONF_BREATH_RATE in config:
        sens = await sensor.new_sensor(config[CONF_BREATH_RATE])
        cg.add(var.set_breath_rate_sensor(sens))

    if CONF_DISTANCE in config:
        sens = await sensor.new_sensor(config[CONF_DISTANCE])
        cg.add(var.set_distance_sensor(sens))

    if CONF_RUNNING in config:
        sens = await sensor.new_sensor(config[CONF_RUNNING])
        cg.add(var.set_running_sensor(sens))