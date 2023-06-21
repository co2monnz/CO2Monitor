#pragma once

#include "esphome.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/display/display_buffer.h"
#include "esphome/components/globals/globals_component.h"
#include "esphome/components/light/addressable_light.h"
#include "esphome/components/qr_code/qr_code.h"
#include "esphome/components/ssd1306_i2c/ssd1306_i2c.h"

#define LOGTAG "co2display"

namespace esphome {
namespace co2mon {

static const char *SETUP_URL = "https://co2mon.nz/setup";

// Wraps an SSD1306 to synchronize state with the associated LEDs as
// Co2 and associated state is passed in.
class Co2Display : public ssd1306_i2c::I2CSSD1306 {
public:

  void setup() {
    // Create a closure to capture our writer method and pass it down for the
    // base DisplayBuffer class to use as it's writer.
    this->set_writer([this] (display::DisplayBuffer &it) -> void { this->writer(it); });

    ssd1306_i2c::I2CSSD1306::setup();
  }

  void set_co2(float v) {
    this->co2 = v;
    this->updateLeds(-1.0f);
  }
  void set_temperature(float v) { this->temperature = v; }
  void set_humidity(float v) { this->humidity = v; }
  void set_wifi(bool v) { this->wifi = v; }
  void set_improv(bool v) {
    if (v == this->improv) {
      return;
    }
    this->improv = v;
    // Prep for showing QR code, and dim the LEDs to improve scannability.
    if (this->improv) {
      this->oldBrightness = this->leds->current_values.get_brightness();
      ESP_LOGD(LOGTAG, "Stored %.1f as pre improv brightness", this->oldBrightness);
      this->updateLeds(0.4);
      // Prep a QR code
      this->qr = new qr_code::QrCode();
      this->qr->set_value(SETUP_URL);
      this->qr->set_ecc(::qrcodegen_Ecc_LOW);
    } else {
      ESP_LOGD(LOGTAG, "Restoring %.1f as pre improv brightness", this->oldBrightness);
      this->updateLeds(this->oldBrightness);
      free(this->qr);
      this->qr = NULL;
    }
  }
  void set_leds(light::AddressableLightState *l) { leds = l; }
  void set_fonts(display::Font *f, display::Font *f10, display::Font *f30) { this->font = f; this->font10 = f10; this->font37 = f30; }
  void set_thresholds(globals::RestoringGlobalsComponent<int> *tG, globals::RestoringGlobalsComponent<int> *tO, globals::RestoringGlobalsComponent<int> *tR) {
    this->co2Green = tG;
    this->co2Orange = tO;
    this->co2Red = tR;
  }
  void set_wifi_icon(display::Image *i) { this->wifi_icon = i; }

protected:

  void updateLeds(float brightness) {
    if (leds == NULL) {
      return;
    }
    auto action = leds->make_call();
    action.set_save(false);
    action.set_color_mode(light::ColorMode::RGB);
    if (brightness != -1.0f) {
      action.set_brightness(brightness);
    }
    if (improv) {
        action.set_state(true);
        action.set_rgb(0.0f, 0.0f, 1.0f);
    } else if (this->co2 < co2Green->value()) {
        action.set_state(false);
    } else if (this->co2 < co2Orange->value()) {
        action.set_state(true);
        action.set_rgb(0.0f, 1.0f, 0.0f);
    } else if (this->co2 < co2Red->value()) {
        action.set_state(true);
        action.set_rgb(1.0f, 0.7f, 0.0f);
    } else if (this->co2 >= co2Red->value()) {
        action.set_state(true);
        action.set_rgb(1.0f, 0.0f, 0.0f);
    }
    action.perform();
  }

  void writer(display::DisplayBuffer &it) {
    if (improv) {
        this->write_improv(it);
    } else {
        this->write_co2(it);
    }
  }
  void write_improv(DisplayBuffer &it) {
      it.printf(0, 0, this->font10, SETUP_URL);
      it.printf(0, 30, this->font10, "WiFi");
      it.printf(98, 30, this->font10, "Setup");
      if (this->qr != NULL) {
        it.qr_code(35, 13, this->qr, Color(255,255,255), 2);
      }
  }

  void write_co2(DisplayBuffer &it) {
      if (wifi) {
          it.image(110, 0, this->wifi_icon);
      }
      it.printf(64, 32, this->font37, display::TextAlign::CENTER, "%.0f", this->co2);
      it.printf(0, 53, this->font10, "temp:  %.1f", this->temperature);
      it.printf(128, 53, this->font10, display::TextAlign::RIGHT, "hum: %.0f%%", this->humidity);
  }
private:

  float co2;
  float temperature;
  float humidity;
  bool wifi;
  bool improv;
  light::AddressableLightState *leds;
  // cached LED brightness before improv mode changes it.
  float oldBrightness;

  qr_code::QrCode *qr;

  display::Font *font;
  display::Font *font10;
  display::Font *font37;
  globals::RestoringGlobalsComponent<int> *co2Green;
  globals::RestoringGlobalsComponent<int> *co2Orange;
  globals::RestoringGlobalsComponent<int> *co2Red;
  display::Image *wifi_icon;
};

}  // namespace co2mon
}  // namespace esphome