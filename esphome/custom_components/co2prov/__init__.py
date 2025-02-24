import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor, captive_portal
from esphome.components.co2display import display
from esphome.components.esp32 import add_idf_sdkconfig_option
from esphome.components import wifi
from esphome.const import CONF_ID

AUTO_LOAD = ['captive_portal']

co2_prov_ns = cg.esphome_ns.namespace('co2mon')
Co2Prov = co2_prov_ns.class_('Co2Prov', binary_sensor.BinarySensor, cg.Component)

CONF_WIFI_ID = "wifi_id"
CONF_DISPLAY_ID = "display_id"
CONF_PORTAL_ID = "portal_id"

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(Co2Prov),
    cv.GenerateID(CONF_WIFI_ID): cv.use_id(wifi.WiFiComponent),
    cv.GenerateID(CONF_DISPLAY_ID): cv.use_id(display.Co2Display),
    cv.GenerateID(CONF_PORTAL_ID): cv.use_id(captive_portal.CaptivePortal),
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add_define("USE_WIFI_AP")
    add_idf_sdkconfig_option("CONFIG_ESP_WIFI_SOFTAP_SUPPORT", True)
    add_idf_sdkconfig_option("CONFIG_LWIP_DHCPS", True)

    wifi = await cg.get_variable(config[CONF_WIFI_ID])
    cg.add(var.set_wifi(wifi))
    d = await cg.get_variable(config[CONF_DISPLAY_ID])
    cg.add(var.set_display(d))
    p = await cg.get_variable(config[CONF_PORTAL_ID])
    cg.add(var.set_portal(p))
