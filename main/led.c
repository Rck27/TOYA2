#include <driver/rmt.h>
#include <driver/gpio.h>
#include <stdint.h>

// WS2812 timing parameters (in nanoseconds)
#define WS2812_T0H    350    // 0 bit high time
#define WS2812_T0L    900    // 0 bit low time
#define WS2812_T1H    900    // 1 bit high time
#define WS2812_T1L    350    // 1 bit low time

// Configure these as needed
#define RMT_TX_CHANNEL    RMT_CHANNEL_0
#define LED_PIN           GPIO_NUM_14
#define MAX_LEDS         30


// Initialize RMT peripheral for WS2812
esp_err_t ws2812_init(void) {
    rmt_config_t config = {
        .rmt_mode = RMT_MODE_TX,
        .channel = RMT_TX_CHANNEL,
        .gpio_num = LED_PIN,
        .clk_div = 2, // 40MHz => 25ns resolution
        .mem_block_num = 1,
        .tx_config = {
            .carrier_freq_hz = 0,
            .carrier_level = RMT_CARRIER_LEVEL_LOW,
            .idle_level = RMT_IDLE_LEVEL_LOW,
            .carrier_duty_percent = 50,
            .carrier_en = false,
            .loop_en = false,
            .idle_output_en = true,
        }
    };

    esp_err_t ret = rmt_config(&config);
    if (ret != ESP_OK) return ret;

    return rmt_driver_install(config.channel, 0, 0);
}

// Convert a single byte to RMT pulses (8 bits)
void ws2812_byte_to_rmt(uint8_t value, rmt_item32_t* items) {
    for (int i = 0; i < 8; i++) {
        if (value & (1 << (7 - i))) {
            // One bit
            items[i].level0 = 1;
            items[i].duration0 = WS2812_T1H / 25; // Convert ns to RMT ticks
            items[i].level1 = 0;
            items[i].duration1 = WS2812_T1L / 25;
        } else {
            // Zero bit
            items[i].level0 = 1;
            items[i].duration0 = WS2812_T0H / 25;
            items[i].level1 = 0;
            items[i].duration1 = WS2812_T0L / 25;
        }
    }
}

// Set RGB value for LED at specified index
esp_err_t ws2812_set_led(size_t index, int r, int g, int b) {
    if (index >= MAX_LEDS) {
        return ESP_ERR_INVALID_ARG;
    }

    rmt_item32_t led_data[24];
    
    // Convert RGB to GRB (WS2812 format)
    // Fill first 8 items with green data
    ws2812_byte_to_rmt(g, &led_data[0]);
    
    // Fill next 8 items with red data
    ws2812_byte_to_rmt(r, &led_data[8]);
    
    // Fill last 8 items with blue data
    ws2812_byte_to_rmt(b, &led_data[16]);

    return rmt_write_items(RMT_TX_CHANNEL, led_data, 24, true);
}