#include <globals.h>
#include <Arduino.h>
#include <config.h>
#include <configManager.h>
#include <WiFi.h>

#include <mqtt.h>
#include <scd30.h>
#include <scd40.h>
#include <housekeeping.h>
#include <Wire.h>
#include <lcd.h>
#include <trafficLight.h>
#include <neopixel.h>
#include <featherMatrix.h>
#include <hub75.h>
#include <bme680.h>
#include <i2c.h>
#include <wifiManager.h>

Model* model;
LCD* lcd;
TrafficLight* trafficLight;
Neopixel* neopixel;
FeatherMatrix* featherMatrix;
HUB75* hub75;
SCD30* scd30;
SCD40* scd40;
BME680* bme680;
TaskHandle_t scd30Task;
TaskHandle_t scd40Task;
TaskHandle_t bme680Task;

void updateMessage(char const* msg) {
  if (lcd) {
    lcd->updateMessage(msg);
  }
}

void modelUpdatedEvt() {
  if (lcd) lcd->update();
  if (trafficLight) trafficLight->update();
  if (neopixel) neopixel->update();
  if (featherMatrix) featherMatrix->update();
  if (hub75) hub75->update();
  mqtt::publishSensors();
}

void calibrateCo2SensorCallback(uint16_t co2Reference) {
  if (I2C::scd30Present() && scd30) scd30->calibrateScd30ToReference(co2Reference);
  if (I2C::scd40Present() && scd40) scd40->calibrateScd40ToReference(co2Reference);
}

void setTemperatureOffsetCallback(float temperatureOffset) {
  if (I2C::scd30Present() && scd30) scd30->setTemperatureOffset(temperatureOffset);
  if (I2C::scd40Present() && scd40) scd40->setTemperatureOffset(temperatureOffset);
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  pinMode(TRIGGER_PIN, INPUT_PULLUP);
  Serial.begin(115200);
  esp_log_level_set("*", ESP_LOG_VERBOSE);
  ESP_LOGI(TAG, "Starting...");

  esp_chip_info_t chip_info;
  esp_chip_info(&chip_info);
  ESP_LOGI(TAG,
    "This is ESP32 chip with %d CPU cores, WiFi%s%s, silicon revision "
    "%d, %dMB %s Flash",
    chip_info.cores,
    (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
    (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "",
    chip_info.revision, spi_flash_get_chip_size() / (1024 * 1024),
    (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded"
    : "external");
  ESP_LOGI(TAG, "Internal Total heap %d, internal Free Heap %d",
    ESP.getHeapSize(), ESP.getFreeHeap());
#ifdef BOARD_HAS_PSRAM
  ESP_LOGI(TAG, "SPIRam Total heap %d, SPIRam Free Heap %d",
    ESP.getPsramSize(), ESP.getFreePsram());
#endif
  ESP_LOGI(TAG, "ChipRevision %d, Cpu Freq %d, SDK Version %s",
    ESP.getChipRevision(), ESP.getCpuFreqMHz(), ESP.getSdkVersion());
  ESP_LOGI(TAG, "Flash Size %d, Flash Speed %d", ESP.getFlashChipSize(),
    ESP.getFlashChipSpeed());

  setupConfigManager();
  printFile();
  if (!loadConfiguration(config)) {
    getDefaultConfiguration(config);
    saveConfiguration(config);
    printFile();
  }

  Wire.begin(SDA, SCL, I2C_CLK);

  I2C::initI2C();

  model = new Model(modelUpdatedEvt);

  if (I2C::scd30Present()) scd30 = new SCD30(&Wire, model, updateMessage);
  if (I2C::scd40Present()) scd40 = new SCD40(&Wire, model, updateMessage);
  if (I2C::bme680Present()) bme680 = new BME680(&Wire, model, updateMessage);
  if (I2C::lcdPresent()) lcd = new LCD(&Wire, model);

#if defined(GREEN_LED_PIN) && defined(YELLOW_LED_PIN) && defined(RED_LED_PIN)
  trafficLight = new TrafficLight(model, RED_LED_PIN, YELLOW_LED_PIN, GREEN_LED_PIN);
#endif

#if defined(NEOPIXEL_PIN) && defined(NEOPIXEL_NUM)
  neopixel = new Neopixel(model, NEOPIXEL_PIN, NEOPIXEL_NUM);
#endif

#if defined(FEATHER_MATRIX_DATAPIN) && defined(FEATHER_MATRIX_CLOCKPIN)
  featherMatrix = new FeatherMatrix(model, FEATHER_MATRIX_DATAPIN, FEATHER_MATRIX_CLOCKPIN);
#endif

#if defined(HUB75_R1) && defined(HUB75_G1) && defined(HUB75_B1) && defined(HUB75_R2) && defined(HUB75_G2) && defined(HUB75_B2) && defined(HUB75_CH_A) && defined(HUB75_CH_B) && defined(HUB75_CH_C) && defined(HUB75_CH_D) && defined(HUB75_CLK) && defined(HUB75_LAT) && defined(HUB75_OE)
  hub75 = new HUB75(model);
#endif

  mqtt::setupMqtt(model, calibrateCo2SensorCallback, setTemperatureOffsetCallback);

  xTaskCreatePinnedToCore(mqtt::mqttLoop,  // task function
    "mqttLoop",         // name of task
    4096,               // stack size of task
    (void*)1,           // parameter of the task
    2,                  // priority of the task
    &mqtt::mqttTask,          // task handle
    0);                 // CPU core

  if (I2C::scd30Present()) {
    scd30Task = scd30->start(
      "scd30Loop",        // name of task
      4096,               // stack size of task
      2,                  // priority of the task
      1);                 // CPU core
  }

  if (I2C::scd40Present()) {
    scd40Task = scd40->start(
      "scd40Loop",        // name of task
      4096,               // stack size of task
      2,                  // priority of the task
      1);                 // CPU core
  }

  if (I2C::bme680Present()) {
    bme680Task = bme680->start(
      "bme680Loop",       // name of task
      4096,               // stack size of task
      2,                  // priority of the task
      1);                 // CPU core
  }

  housekeeping::cyclicTimer.attach(30, housekeeping::doHousekeeping);

  WifiManager::setupWifi();

  ESP_LOGI(TAG, "Setup done.");
  if (I2C::lcdPresent()) {
    lcd->updateMessage("Setup done.");
  }
}

void loop() {

  if ((digitalRead(TRIGGER_PIN) == LOW)) {
    while (digitalRead(TRIGGER_PIN) == LOW);
    digitalWrite(LED_PIN, LOW);
    WifiManager::startConfigPortal(updateMessage);
  }

  if (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_PIN, LOW);
    WifiManager::setupWifi();
  } else if (WiFi.status() == WL_CONNECTED) {
    digitalWrite(LED_PIN, HIGH);
  }
  vTaskDelay(pdMS_TO_TICKS(1000));
}