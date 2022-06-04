#include <hub75.h>
#include <config.h>
#include <configManager.h>

#include <smileys.h>
#include <digits.h>
#include <message.h>

// Local logging tag
static const char TAG[] = __FILE__;

#define USE_FASTLINES

HUB75::HUB75(Model* _model) {
  this->model = _model;
  if (config.hub75R1 != 0 && config.hub75G1 != 0 && config.hub75B1 != 0 && config.hub75R2 != 0 && config.hub75G2 != 0 && config.hub75B2 != 0 && config.hub75ChA != 0
    && config.hub75ChB != 0 && config.hub75ChC != 0 && config.hub75ChD != 0 && config.hub75Clk != 0 && config.hub75Lat != 0 && config.hub75Oe) {
    HUB75_I2S_CFG::i2s_pins hub75Pins = { config.hub75R1, config.hub75G1, config.hub75B1, config.hub75R2, config.hub75G2, config.hub75B2, config.hub75ChA,
    config.hub75ChB, config.hub75ChC, config.hub75ChD, -1, config.hub75Lat, config.hub75Oe, config.hub75Clk };
    HUB75_I2S_CFG mxconfig(64, 32, 1, hub75Pins, HUB75_I2S_CFG::FM6126A);
    this->matrix = new MatrixPanel_I2S_DMA(mxconfig);
  }
  matrix->begin();
  matrix->setBrightness8(config.brightness);

  cyclicTimer = new Ticker();
  // https://arduino.stackexchange.com/questions/81123/using-lambdas-as-callback-functions
  //  cyclicTimer->attach<typeof this>(1, [](typeof this p) { p->timer(); },
  //  this);

  // https://stackoverflow.com/questions/60985496/arduino-esp8266-esp32-ticker-callback-class-member-function
  cyclicTimer->attach(0.3, +[](HUB75* instance) { instance->timer(); }, this);
  this->toggle = false;
  ESP_LOGD(TAG, "HUB75 initialised");
}

HUB75::~HUB75() {
  if (this->matrix) delete matrix;
}

void HUB75::stopDMA() {
  if (this->matrix) matrix->stopDMAoutput();
}

void HUB75::update(uint16_t mask, TrafficLightStatus oldStatus, TrafficLightStatus newStatus) {
  matrix->setBrightness8(config.brightness);
  //  ESP_LOGD(TAG, "HUB75 update: %u => %i", model->getCo2(), newStatus);
  if (oldStatus != newStatus) {
    // only redraw smiley on status change
    matrix->fillRect(0, 0, 32, 32, 0);
    matrix->drawRGBBitmap(0, 0, smileys[newStatus], 32, 32);
  }
  // clear co2 reading and message
  matrix->fillRect(33, 0, 32, 32, 0);
  // show co2 reading
  if (model->getCo2() > 9999) {
    matrix->drawBitmap(35, 24, digits[9], 10, 8, matrix->color565(255, 255, 255));
    matrix->drawBitmap(35, 16, digits[9], 10, 8, matrix->color565(255, 255, 255));
    matrix->drawBitmap(35, 8, digits[9], 10, 8, matrix->color565(255, 255, 255));
    matrix->drawBitmap(35, 0, digits[9], 10, 8, matrix->color565(255, 255, 255));
  } else {
    if (model->getCo2() > 999)
      matrix->drawBitmap(35, 24, digits[(uint16_t)(model->getCo2() / 1000) % 10], 10, 8, matrix->color565(255, 255, 255));
    if (model->getCo2() > 99)
      matrix->drawBitmap(35, 16, digits[(uint16_t)(model->getCo2() / 100) % 10], 10, 8, matrix->color565(255, 255, 255));
    if (model->getCo2() > 9)
      matrix->drawBitmap(35, 8, digits[(uint16_t)(model->getCo2() / 10) % 10], 10, 8, matrix->color565(255, 255, 255));
    if (model->getCo2() > 0)
      matrix->drawBitmap(35, 0, digits[model->getCo2() % 10], 10, 8, matrix->color565(255, 255, 255));
  }
  // show message
  matrix->drawBitmap(48, 0, messages[newStatus], 16, 32, matrix->color565(255, 255, 255));
}

void HUB75::timer() {
  if (model->getStatus() == DARK_RED) {
    if (toggle)
      matrix->drawRGBBitmap(0, 0, smileys[model->getStatus()], 32, 32);
    else
      matrix->fillRect(0, 0, 32, 32, 0);
    toggle = !toggle;
  }
}