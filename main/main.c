#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "encoder.h"
#include "button.h"
#include "matrix_keyboard.h"
#include "max7219.h"

#include "main.h"
#include "led.c"

const static char *TAG = "TOYA2";


int get_led_index(int row, int col){
    return led_index[row][col];
}



void led_task(){
    esp_err_t err = ws2812_init();
    if(err) ESP_LOGE("errr", "err");


    ESP_LOGI(TAG, "Start blinking LED strip");
    // while (1) {
    // if(Current_LED_INDEX > 0){
    // ws2812_set_led(Current_LED_INDEX, 50, 50, 50);  // Set first LED
    // ESP_LOGI("LED", "LED-%d ON", Current_LED_INDEX);
    // }
    // vTaskDelay(pdMS_TO_TICKS(500));
    // }
}



/**
 * @brief Matrix keyboard event handler
 * @note This function is run under OS timer task context
 */


char kbd_handler(matrix_kbd_handle_t mkbd_handle, matrix_kbd_event_id_t event, void *event_data, void *handler_args)
{   

    uint32_t key_code = (uint32_t)event_data;
    int col = key_code >> 8;    // Get first 2 digits (01)
    int row = key_code & 0xFF;  // Get last 2 digits (04)

    
    switch (event) {
    case MATRIX_KBD_EVENT_DOWN:
        Current_LED_INDEX = get_led_index(row, col);
        ESP_LOGI(TAG, " press : %c %d %d, LED-%d",character[row][col], col, row, Current_LED_INDEX);
        ws2812_set_led(Current_LED_INDEX, 50, 50, 50);  // Set first LED

        break;
    case MATRIX_KBD_EVENT_UP:
        ws2812_set_led(Current_LED_INDEX, 0, 0, 0);  // Set first LED
        Current_LED_INDEX = 0;
        // ESP_LOGI("IDK", "%s", xTaskGetCurrentTaskHandle());
        // ESP_LOGI(TAG, "release event,key %c, key code = %04"PRIx32,character[row][col], key_code);
        break;
    }

    return character[row][col];
}

static void keyboard_init(){
    matrix_kbd_handle_t kbd = NULL;
    matrix_kbd_config_t config = MATRIX_KEYBOARD_DEFAULT_CONFIG();
 
    config.row_gpios = ROW_GPIO;
    config.nr_col_gpios = 4;
    config.col_gpios = COL_GPIO;
    config.nr_row_gpios = 4;
    matrix_kbd_install(&config, &kbd);
    matrix_kbd_register_event_handler(kbd, kbd_handler, NULL);
    matrix_kbd_start(kbd);
}




void app_main(void)
{   

    keyboard_init();
    led_task();

}
