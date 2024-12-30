#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "button.h"
#include "matrix_keyboard.h"
#include "max7219.h"

#include "esp_random.h"
#include "esp_system.h"
#include "string.h"
#include "main.h"
#include "led.c"

const static char *TAG = "TOYA2";

QueueHandle_t keyboard_queue;


int generate_random(int min, int max) {
    return min + esp_random() % (max - min + 1);
}

// Math game task
void math_game_task(void *pvParameters) {
    int num1, num2, correct_answer;
    char operator;
    char received_char;
    bool game_active = true;
    
    keyboard_queue = xQueueCreate(10, sizeof(char));

    // Generate initial question
    generate_new_question(&num1, &num2, &operator, &correct_answer);

    while(game_active) {
        // Display current question
        printf("\nSolve: %d %c %d = ?\n", num1, operator, num2);
        
        bool question_active = true;
        while(question_active) {
            // Wait for character from keyboard queue
            if(xQueueReceive(keyboard_queue, &received_char, portMAX_DELAY) == pdTRUE) {
                int user_answer = received_char - '0'; // Convert ASCII to integer
                
                if(user_answer == correct_answer) {
                    printf("\nCorrect! Well done!\n");
                    // Generate new question only after correct answer
                    generate_new_question(&num1, &num2, &operator, &correct_answer);
                    question_active = false;
                } else {
                    printf("\nIncorrect. Try again!\n");
                }
                
                // Small delay for readability
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
        }
    }
}

// Helper function to generate a new question
static void generate_new_question(int *num1, int *num2, char *operator, int *correct_answer) {
    do {
        *num1 = generate_random(0, 9);
        *num2 = generate_random(0, 9);
        *operator = generate_operator();
        *correct_answer = calculate_answer(*num1, *num2, *operator);
    } while (*correct_answer > 9); // Ensure single-digit answers
}

// Calculate answer based on operator
static int calculate_answer(int num1, int num2, char operator) {
    switch(operator) {
        case '+':
            return num1 + num2;
        case '-':
            return num1 - num2;
        case '*':
            return num1 * num2;
        default:
            return 0;
    }
}

// Generate random operator
static char generate_operator(void) {
    char operators[] = {'+', '-', '*'};
    return operators[generate_random(0, 2)];
}


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
        xQueueSend(keyboard_queue, &character[row][col], portMAX_DELAY);
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
    xTaskCreate(math_game_task, "game_task", 2048, NULL, 5, NULL);
}
