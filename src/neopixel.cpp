#include <neopixel.h>
#include <config.h>
#include <configManager.h>

Neopixel::Neopixel(Model* _model, uint8_t _pin, uint8_t numPixel) {
  this->model = _model;
  strip = new Adafruit_NeoPixel(numPixel, _pin, NEO_GRB + NEO_KHZ800);
  toggle = false;
  cyclicTimer = new Ticker();

  // https://stackoverflow.com/questions/60985496/arduino-esp8266-esp32-ticker-callback-class-member-function
  cyclicTimer->attach(0.3, +[](Neopixel* instance) { instance->timer(); }, this);

  this->strip->begin();
  this->strip->setBrightness(20);
  this->strip->show(); // Initialize all pixels to 'off'
  fill(this->strip->Color(255, 0, 0)); // Red
  delay(500);
  fill(this->strip->Color(255, 255, 0)); // Yellow
  delay(500);
  fill(this->strip->Color(0, 255, 0)); // Green
  delay(500);
  fill(this->strip->Color(0, 0, 0)); // Green

  this->status = UNDEFINED;
}

Neopixel::~Neopixel() {
  if (this->cyclicTimer) delete cyclicTimer;
  if (this->strip) delete strip;
}

void Neopixel::fill(uint32_t c) {
  for (uint16_t i = 0; i < this->strip->numPixels(); i++) {
    this->strip->setPixelColor(i, c);
  }
  this->strip->show();
}

void Neopixel::update() {
  if (model->getCo2() < config.yellowThreshold) {
    this->status = GREEN;
  } else if (model->getCo2() < config.redThreshold) {
    this->status = YELLOW;
  } else if (model->getCo2() < config.darkRedThreshold) {
    this->status = RED;
  } else {
    this->status = DARK_RED;
  }

  if (this->status == GREEN) {
    fill(this->strip->Color(0, 255, 0)); // Green
  } else if (this->status == YELLOW) {
    fill(this->strip->Color(255, 255, 0)); // Yellow
  } else if (this->status == RED) {
    fill(this->strip->Color(255, 0, 0)); // Red
  } else if (this->status == DARK_RED) {
    fill(this->strip->Color(255, 0, 0)); // Red
  } else {
    fill(this->strip->Color(0, 0, 0));
  }
}

void Neopixel::timer() {
  this->toggle = !(this->toggle);
  if (this->status == DARK_RED) {
    if (toggle)
      fill(this->strip->Color(255, 0, 0)); // Red
    else
      fill(this->strip->Color(0, 0, 255));
  }
}