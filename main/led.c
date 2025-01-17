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
    ESP_LOGI(TAG, "Create RMT TX channel");
    rmt_channel_handle_t led_chan = NULL;
    rmt_tx_channel_config_t tx_chan_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT, // select source clock
        .gpio_num = RMT_LED_STRIP_GPIO_NUM,
        .mem_block_symbols = 64, // increase the block size can make the LED less flickering
        .resolution_hz = RMT_LED_STRIP_RESOLUTION_HZ,
        .trans_queue_depth = 4, // set the number of transactions that can be pending in the background
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &led_chan));

    ESP_LOGI(TAG, "Install led strip encoder");
    rmt_encoder_handle_t led_encoder = NULL;
    led_strip_encoder_config_t encoder_config = {
        .resolution = RMT_LED_STRIP_RESOLUTION_HZ,
    };
    ESP_ERROR_CHECK(rmt_new_led_strip_encoder(&encoder_config, &led_encoder));

    ESP_LOGI(TAG, "Enable RMT TX channel");
    ESP_ERROR_CHECK(rmt_enable(led_chan));
    
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