#include "esphome.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/co2display/co2display.h"
#include "esphome/components/esp32_ble/ble.h"
#include "esphome/components/esp32_improv/esp32_improv_component.h"
#include "esphome/components/wifi/wifi_component.h"

#include <esp_timer.h>

#define USEC_SEC 1000000  // Number of usecs in a second

namespace esphome {
namespace co2mon {

static const char *TAG = "co2improv";
static const char *STOP_TIMER = "co2improv::stop";

void stopAdvertising() {
  ESP_LOGD(TAG, "Turning off BLE advertisements (aka improv)!");
  if (esp32_ble::global_ble != NULL) {
    esp32_ble::global_ble->get_advertising()->stop();
  }
}

class Co2Improv : public binary_sensor::BinarySensor, public Component {
public:
  void setup() override {
    inImprov = false;
    this->publish_initial_state(inImprov);
    // Make sure BT is off (after a delay to let the stack init)
    this->set_timeout(STOP_TIMER, 10000, stopAdvertising);
  }

  void loop() override {
    if (!inImprov) {
      return;
    }
    // Check if improv succeeded
    if (wc->is_connected()) {
      ESP_LOGD(TAG, "Improv looks to have succeeded! Leaving improve mode.");
      this->set_timeout(STOP_TIMER, 2000, stopAdvertising);
      set_state(false);
      return;
    }
    // Check for timeout
    int64_t now = esp_timer_get_time();
    int64_t ageS = (now - improvStart) / USEC_SEC;
    if (ageS > 120) {
      // Give up.
      ESP_LOGD(TAG, "Improv timed out after 120s. Disabling advertisements!");
      stopAdvertising();
      set_state(false);
      wc->enable(); // re-enable WiFi.
    }
  }

  void set_state(bool s) {
    inImprov = s;
    this->publish_state(inImprov);
    if (this->display != NULL) {
      this->display->set_improv(inImprov);
    }
    if (inImprov) {
      improvStart = esp_timer_get_time();
    }
  }

  void set_wifi(wifi::WiFiComponent *w) { wc = w; }
  void set_display(co2mon::Co2Display *d) { display = d; }

  void startImprov() {
    if (esp32_ble::global_ble == NULL) {
      ESP_LOGD(TAG, "Cannot start improv: BLE not available!");
      return;
    }
    if (inImprov) {
      ESP_LOGD(TAG, "Turning off improv!");
      stopAdvertising();
      set_state(false);
      return;
    }
    // Make sure we're not about to cancel on a timer
    this->cancel_timeout(STOP_TIMER);
    // Turn off WiFi (improv doesn't seem to work if its already active)
    wc->disable();
    // Start improv
    ESP_LOGD(TAG, "Starting improv!");
    esp32_improv::global_improv_component->start();
    esp32_ble::global_ble->get_advertising()->start();
    set_state(true);
  }

private:
  bool inImprov;
  int64_t improvStart;
  wifi::WiFiComponent *wc;
  co2mon::Co2Display *display;
};

}  // namespace co2mon
}  // namespace esphome