#include <model.h>
#include <configManager.h>

Model::Model(modelUpdatedEvt_t _modelUpdatedEvt) {
  this->temperature = NaN;
  this->humidity = NaN;
  this->co2 = 0;
  this->pressure = 0;
  this->iaq = 0;
  this->modelUpdatedEvt = _modelUpdatedEvt;
  this->status = UNDEFINED;
}

Model::~Model() {}

void Model::updateStatus() {
  TrafficLightStatus co2Status = UNDEFINED;
  if (this->co2 != 0) {
    if (this->co2 < config.yellowThreshold) {
      co2Status = GREEN;
    } else if (this->co2 < config.redThreshold) {
      co2Status = YELLOW;
    } else if (this->co2 < config.darkRedThreshold) {
      co2Status = RED;
    } else if (this->co2 >= config.darkRedThreshold) {
      co2Status = DARK_RED;
    }
  }
  TrafficLightStatus iaqStatus = UNDEFINED;
  if (iaq != 0) {
    if (iaq <= 50) {
      iaqStatus = GREEN;
    } else if (iaq <= 100) {
      iaqStatus = YELLOW;
    } else if (iaq <= 200) {
      iaqStatus = RED;
    } else {
      iaqStatus = DARK_RED;
    }
  }
  this->status = max(co2Status, iaqStatus);
  //  ESP_LOGD(TAG, "UpdateStatus CO2: %i (%u), IAQ: %i (%u) ==> %i", co2Status, this->co2, iaqStatus, this->iaq, this->status);
}

void Model::updateModel(uint16_t _co2, float _temperature, float _humidity) {
  this->co2 = _co2;
  this->temperature = _temperature;
  this->humidity = _humidity;
  TrafficLightStatus oldStatus = this->status;
  this->updateStatus();
  modelUpdatedEvt((_co2 != 0 ? M_CO2 : M_NONE) | M_TEMPERATURE | M_HUMIDITY, oldStatus, this->status);
}

void Model::updateModel(float _temperature, float _humidity, uint16_t _pressure, uint16_t _iaq) {
  this->temperature = _temperature;
  this->humidity = _humidity;
  this->pressure = _pressure;
  this->iaq = _iaq;
  TrafficLightStatus oldStatus = this->status;
  this->updateStatus();
  modelUpdatedEvt(M_TEMPERATURE | M_HUMIDITY | M_PRESSURE | (_iaq != 0 ? M_IAQ : M_NONE), oldStatus, this->status);
}

TrafficLightStatus Model::getStatus() {
  return this->status;
}

void Model::updateModel(uint16_t pm0_5, uint16_t pm1_0, uint16_t pm2_5, uint16_t pm4, uint16_t pm10) {
  modelUpdatedEvt(M_PM0_5 || M_PM1_0 || M_PM2_5 || M_PM4 || M_PM10);
}

uint16_t Model::getCo2() {
  return this->co2;
}

float Model::getTemperature() {
  return this->temperature;
}

float Model::getHumidity() {
  return this->humidity;
}

uint16_t Model::getPressure() {
  return this->pressure;
}

uint16_t Model::getIAQ() {
  return this->iaq;
}

uint16_t Model::getPM0_5() {
  return this->pm0_5;
}

uint16_t Model::getPM1_0() {
  return this->pm10;
}

uint16_t Model::getPM2_5() {
  return this->pm2_5;
}

uint16_t Model::getPM4() {
  return this->pm4;
}

uint16_t Model::getPM10() {
  return this->pm10;
}

