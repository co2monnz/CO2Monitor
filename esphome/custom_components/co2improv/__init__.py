import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.components import wifi
from esphome.const import CONF_ID

AUTO_LOAD = []

co2_improv_ns = cg.esphome_ns.namespace('co2mon')
Co2Improv = co2_improv_ns.class_('Co2Improv', binary_sensor.BinarySensor, cg.Component)

CONF_WIFI_ID = "wifi_id"

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(Co2Improv),
    cv.GenerateID(CONF_WIFI_ID): cv.use_id(wifi.WiFiComponent),
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    wifi = await cg.get_variable(config[CONF_WIFI_ID])
    cg.add(var.set_wifi(wifi))
