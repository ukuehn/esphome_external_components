import esphome.codegen as cg
from esphome.components import one_wire, sensor
import esphome.config_validation as cv

from esphome.const import (
    CONF_ID,
    CONF_NAME,
    CONF_TEMPERATURE,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    UNIT_MILLIVOLT,
    UNIT_VOLT,
    UNIT_CELSIUS,
)

DEPENDENCIES = [ "one_wire" ]

ds2438_ns = cg.esphome_ns.namespace("ds2438")

DallasSmartBatteryMonitor = ds2438_ns.class_(
    "DallasSmartBatteryMonitor",
    cg.PollingComponent,
    one_wire.OneWireDevice,
)


CONF_VSENSE = "vsense"
CONF_VAD = "vad"
CONF_VDD = "vdd"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(DallasSmartBatteryMonitor),
        cv.Optional(
            CONF_TEMPERATURE,
        ): sensor.sensor_schema(
            unit_of_measurement = UNIT_CELSIUS,
            accuracy_decimals = 1,
            state_class = STATE_CLASS_MEASUREMENT,
            device_class = DEVICE_CLASS_TEMPERATURE,
        ),
        cv.Optional(
            CONF_VSENSE,
            ): sensor.sensor_schema(
                unit_of_measurement = UNIT_MILLIVOLT,
                accuracy_decimals = 4,
                state_class = STATE_CLASS_MEASUREMENT,
                device_class = DEVICE_CLASS_VOLTAGE,
            ),
        cv.Optional(
            CONF_VAD,
            ): sensor.sensor_schema(
                unit_of_measurement = UNIT_VOLT,
                accuracy_decimals = 2,
                state_class = STATE_CLASS_MEASUREMENT,
                device_class = DEVICE_CLASS_VOLTAGE,
            ),
        cv.Optional(
            CONF_VDD,
            ): sensor.sensor_schema(
                unit_of_measurement = UNIT_VOLT,
                accuracy_decimals = 2,
                state_class = STATE_CLASS_MEASUREMENT,
                device_class = DEVICE_CLASS_VOLTAGE,
            ),
    }
).extend(one_wire.one_wire_device_schema()).extend(cv.polling_component_schema("60s"))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await one_wire.register_one_wire_device(var, config)

    if CONF_TEMPERATURE in config:
        sens = await sensor.new_sensor(config.get(CONF_TEMPERATURE))
        cg.add(var.set_temperature_sensor(sens))

    if CONF_VSENSE in config:
        sens = await sensor.new_sensor(config.get(CONF_VSENSE))
        cg.add(var.set_vsense_sensor(sens))

    if CONF_VAD in config:
        sens = await sensor.new_sensor(config.get(CONF_VAD))
        cg.add(var.set_vad_sensor(sens))

    if CONF_VDD in config:
        sens = await sensor.new_sensor(config.get(CONF_VDD))
        cg.add(var.set_vdd_sensor(sens))

    return var
