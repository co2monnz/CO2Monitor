import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import font, image
from esphome.components import globals
from esphome.components import light
from esphome.components import ssd1306_base, i2c
from esphome.components.ssd1306_base import _validate
from esphome.components.ssd1306_i2c import display
from esphome.const import CONF_ID

AUTO_LOAD = ["ssd1306_base", "ssd1306_i2c", "qr_code"]
DEPENDENCIES = ["i2c"]

co2_display_ns = cg.esphome_ns.namespace('co2mon')
Co2Display = co2_display_ns.class_('Co2Display', display.I2CSSD1306)

CONF_LEDS_ID = "led_id"
CONF_F_ID = "font_id"
CONF_F10_ID = "font10_id"
CONF_F30_ID = "font30_id"
CONF_THOLD_G_ID = "threshold_green_id"
CONF_THOLD_O_ID = "threshold_orange_id"
CONF_THOLD_R_ID = "threshold_red_id"
CONF_WIFI_ICON_ID = "wifi_icon"

CONFIG_SCHEMA = cv.All(
    ssd1306_base.SSD1306_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(Co2Display),
            cv.Optional(CONF_LEDS_ID): cv.use_id(light.AddressableLightState),
            cv.Required(CONF_F_ID): cv.use_id(font.Font),
            cv.Required(CONF_F10_ID): cv.use_id(font.Font),
            cv.Required(CONF_F30_ID): cv.use_id(font.Font),
            # Should be RestoringGlobalsComponent but globals/__init__.py:23 doesn't permit that!
            cv.Required(CONF_THOLD_G_ID): cv.use_id(globals.GlobalsComponent),
            cv.Required(CONF_THOLD_O_ID): cv.use_id(globals.GlobalsComponent),
            cv.Required(CONF_THOLD_R_ID): cv.use_id(globals.GlobalsComponent),
            cv.Required(CONF_WIFI_ICON_ID): cv.use_id(image.Image_),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x3C)),
    _validate,
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await ssd1306_base.setup_ssd1306(var, config)
    await i2c.register_i2c_device(var, config)

    leds = await cg.get_variable(config[CONF_LEDS_ID])
    cg.add(var.set_leds(leds))
    f = await cg.get_variable(config[CONF_F_ID])
    f10 = await cg.get_variable(config[CONF_F10_ID])
    f30 = await cg.get_variable(config[CONF_F30_ID])
    cg.add(var.set_fonts(f, f10, f30))
    tG = await cg.get_variable(config[CONF_THOLD_G_ID])
    tO = await cg.get_variable(config[CONF_THOLD_O_ID])
    tR = await cg.get_variable(config[CONF_THOLD_R_ID])
    cg.add(var.set_thresholds(tG, tO, tR))
    wI = await cg.get_variable(config[CONF_WIFI_ICON_ID])
    cg.add(var.set_wifi_icon(wI))
