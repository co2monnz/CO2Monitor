#include "esphome.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/captive_portal/captive_portal.h"
#include "esphome/components/co2display/co2display.h"
#include "esphome/components/esp32_ble/ble.h"
#include "esphome/components/esp32_improv/esp32_improv_component.h"
#include "esphome/components/wifi/wifi_component.h"

#include <esp_timer.h>

#define USEC_SEC 1000000  // Number of usecs in a second
#define IMPROV_TIMEOUT_SECS 120

namespace esphome {
namespace co2mon {

static const char *TAG = "co2prov";
static const char *STOP_TIMER = "co2prov::stop";

class Co2Prov : public binary_sensor::BinarySensor, public Component {
public:
  void setup() override {
    inImprov = false;
    inAP = false;
    provStart = 0;
    this->publish_initial_state(inImprov);
    // Make sure BT is off (after a delay to let the stack init)
    this->set_timeout(STOP_TIMER, 10000, [this]() { this->stopImprov(); });
  }

  void loop() override {
    if (!inImprov && !inAP) {
      return;
    }
    // Check for timeout
    if (provRunningSeconds() > IMPROV_TIMEOUT_SECS) {
      // Give up.
      ESP_LOGD(TAG, "Provisioning timed out. Disabling!");
      set_state(false, false);
    }
    if (inImprov) {
      // Check if improv succeeded
      if (wc->is_connected()) {
        ESP_LOGD(TAG, "Improv looks to have succeeded! Leaving improve mode.");
        set_state(false, false, 2000);
        return;
      }
    } else if (inAP) {
      // Check if AP is still needed
      if (wc->has_sta()) {
        ESP_LOGD(TAG, "STA connected, disabling AP!");
        set_state(false, false);
      }
    }
  }

  void set_state(bool enableImprov, bool enableAP) {
    this->set_state(enableImprov, enableAP, -1);
  }

  void set_state(bool enableImprov, bool enableAP, int stop_delay) {
    if (enableImprov && enableAP) {
      ESP_LOGW(TAG, "Cannot enable both improv and AP at the same time. Ignoring!");
      return;
    }
    // Maintain state for improv component.
    inImprov = enableImprov;
    inAP = enableAP;
    this->publish_state(inImprov);
    // Keep the display component updated on what's happening.
    if (this->display != NULL) {
      this->display->set_improv(inImprov, inAP);
    }
    // Enable or disable improv as required.
    if (inImprov) {
      if (provStart == 0) {
        startImprov();
      }
    } else {
      // Stop BLE advertisements
      if (stop_delay != -1) {
        this->set_timeout(STOP_TIMER, stop_delay, [this]() { this->stopImprov(); });
      } else {
        this->stopImprov();
      }
    }
    // Enable or disable AP as required.
    if (inAP) {
      if (wc->is_connected() && wc->has_sta()) {
        wc->clear_sta();
        wc->disable();
      }
      if (!wc->has_ap() || wc->is_disabled()) {
        // wait just a couple of loops before starting the AP in case disable was called above.
        this->set_timeout(STOP_TIMER, 200, [this]() { this->startAP(); });
      }
    } else {
      // Turn off captive portal
      if (captive_portal::global_captive_portal != nullptr) {
        ESP_LOGD(TAG, "Turning off captive portal.");
        //captive_portal::global_captive_portal->end();
        captive_portal::global_captive_portal = nullptr;
      }
      // Only way to get rid of the AP is reboot!
      if (wc->has_ap()) {
        ESP_LOGD(TAG, "Rebooting to turn off config AP!");
        // Wait a second before doing so in case log messages want to go out.
        this->set_timeout(STOP_TIMER, 2000, [this]() { esp_restart(); });
      }
    }
  }

  void set_wifi(wifi::WiFiComponent *w) { wc = w; }
  void set_portal(captive_portal::CaptivePortal *p) { portal = p; }
  void set_display(co2mon::Co2Display *d) { display = d; }

  void toggleImprov() {
    if (inImprov) {
      ESP_LOGD(TAG, "Turning off improv!");
      set_state(false, false);
      return;
    }
    // Make sure we're not about to cancel on a timer
    this->cancel_timeout(STOP_TIMER);
    ESP_LOGD(TAG, "Starting improv!");
    set_state(true, false);
  }

  void toggleAP() {
    if (inAP) {
      ESP_LOGD(TAG, "Turning off AP!");
      set_state(false, false);
    }
    ESP_LOGD(TAG, "Starting AP!");
    set_state(false, true);
  }

private:
  bool inImprov;
  bool inAP;
  int64_t provStart;
  wifi::WiFiComponent *wc;
  co2mon::Co2Display *display;
  captive_portal::CaptivePortal *portal;

  void stopImprov() {
    if (esp32_ble::global_ble == NULL) {
      return;
    }
    if (esp32_ble::global_ble->is_active()) {
      ESP_LOGD(TAG, "Turning off BLE advertisements (aka improv)!");
      esp32_ble::global_ble->disable();
      // Make sure WiFi is re-enabled
      wc->enable();
    }
    provStart = 0;
  }

  void startImprov() {
    if (esp32_improv::global_improv_component == NULL || esp32_ble::global_ble == NULL) {
      ESP_LOGD(TAG, "Cannot start improv: improv/BLE not available!");
      return;
    }

    ESP_LOGD(TAG, "Turning on BLE advertisements (aka improv)!");
    // Turn off WiFi (improv doesn't seem to work if its already active)
    wc->disable();
    // Start improv and BLE advertisements
    esp32_improv::global_improv_component->start();
    esp32_ble::global_ble->enable();
    provStart = esp_timer_get_time();
  }

  // Returns time in seconds since prov started.
  int64_t provRunningSeconds() {
    int64_t now = esp_timer_get_time();
    return (now - provStart) / USEC_SEC;
  }

  void startAP() {
    ESP_LOGD(TAG, "Turning on config AP and captive portal!");
    captive_portal::global_captive_portal = portal;
    if (!wc->has_ap()) {
      wifi::WiFiAP ap = wifi::WiFiAP();
      ap.set_password("co2monitor");
      wc->set_ap(ap);
    }
    wc->enable();
  }
};

}  // namespace co2mon
}  // namespace esphome