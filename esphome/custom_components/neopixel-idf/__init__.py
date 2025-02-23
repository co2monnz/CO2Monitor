import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import light
from esphome.const import CONF_ID

AUTO_LOAD = ['light']

NeopixelIDF = cg.esphome_ns.class_('NeopixelIDF', light.LightOutput, light.AddressableLightState, cg.Component)

CONFIG_SCHEMA = light.RGB_LIGHT_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(NeopixelIDF),
})

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
