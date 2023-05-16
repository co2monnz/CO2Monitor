#include "esphome.h"

#include "driver/rmt.h"
#include "esp_err.h"
#include "esp_check.h"

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 3, 0)
#define RMT_INT_FLAGS (ESP_INTR_FLAG_LOWMED)
#else
#define RMT_INT_FLAGS (ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LEVEL1)
#endif

#define NUM_LEDS	        3
#define BITS_PER_LED_CMD	24
#define LED_BUFFER_ITEMS	(NUM_LEDS * BITS_PER_LED_CMD)
#define LED_RMT_TX_CHANNEL	RMT_CHANNEL_6

// Timing calculations
const static uint32_t CpuFreq = 80000000L; // 80MHZ
const static uint32_t NsPerSec = 1000000000L;
const static uint32_t TicksPerSec = (CpuFreq / 2);
const static uint32_t NsPerTick = (NsPerSec / TicksPerSec);
inline constexpr static uint32_t NsToTicks(uint32_t ns) { return ns / NsPerTick;}
const static uint32_t T0H = NsToTicks(300);
const static uint32_t T0L = NsToTicks(950);
const static uint32_t T1H = NsToTicks(900);
const static uint32_t T1L = NsToTicks(350);

// Tag for log messages
static const char *TAG = "neopixel-idf";

// esphome neopixel light output only supports the Arduino framework, so we need to implement our own
// neopixel driver as a custom light output here. Urgh.
class NeopixelIDF : public Component, public LightOutput {
    public:
        void setup() override {
            rmt_config_t config = {
                .rmt_mode = RMT_MODE_TX,
                .channel = LED_RMT_TX_CHANNEL,
                .gpio_num = GPIO_NUM_16,
                .clk_div = 2,
                .mem_block_num = 1,
                .tx_config = rmt_tx_config_t{
                    .carrier_level = RMT_CARRIER_LEVEL_LOW,
                    .idle_level = RMT_IDLE_LEVEL_LOW,
                    .carrier_en = false,
                    .loop_en = false,
                    .idle_output_en = true,
                },
            };
            esp_err_t err=rmt_config(&config);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to configure RMT: %d", err);
                return;
            }
            err = rmt_driver_install(config.channel, 0, RMT_INT_FLAGS);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to install RMT driver: %d", err);
                return;
            }
            ESP_LOGD(TAG, "WS2812 init done!");
        }

        LightTraits get_traits() override {
            auto traits = LightTraits();
            traits.set_supported_color_modes({ColorMode::RGB, ColorMode::BRIGHTNESS});
            return traits;
        }

        void write_state(LightState *state) override {
            float red, green, blue;
            state->current_values_as_rgb(&red, &green, &blue);
            // Pack RGB values into a GRB byte "array"
            uint32_t per_led = 0;
            per_led |= (uint32_t)(0xFF * green) << 16;
            per_led |= (uint32_t)(0xFF * red) << 8;
            per_led |= (uint32_t)(0xFF * blue);
            // Convert that array into a RMT item array (aka string of bit sequences, per LED)
            rmt_item32_t led_data_buffer[LED_BUFFER_ITEMS];
            for (uint32_t led = 0; led < NUM_LEDS; led++) {
                uint32_t mask = 1 << (BITS_PER_LED_CMD - 1);
                for (uint32_t bit = 0; bit < BITS_PER_LED_CMD; bit++) {
                    uint32_t bit_is_set = per_led & mask;
                    led_data_buffer[(led * BITS_PER_LED_CMD) + bit] = bit_is_set ?
                                                                      (rmt_item32_t){{{T1H, 1, T1L, 0}}} :
                                                                      (rmt_item32_t){{{T0H, 1, T0L, 0}}};
                    mask >>= 1;
                }
            }
            if (rmt_write_items(LED_RMT_TX_CHANNEL, led_data_buffer, LED_BUFFER_ITEMS, false) != ESP_OK) {
                ESP_LOGE(TAG, "Failed to write RMT items");
                return;
            }
            if (rmt_wait_tx_done(LED_RMT_TX_CHANNEL, portMAX_DELAY) != ESP_OK) {
                ESP_LOGE(TAG, "Failed to wait for RMT transmission to finish");
                return;
            }
        }
};