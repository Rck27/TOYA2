#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

// #include "button.h"
// #include "matrix_keyboard.h"
#include "max7219.h"
#include "driver/i2s.h"
#include "esp_mac.h"
#include "esp_spiffs.h"
#include "esp_timer.h"

#include "driver/rmt_tx.h"
#include "led_strip_encoder.h"


#include "esp_random.h"
#include "esp_system.h"
#include "string.h"
#include "main.h"
// #include "led.c"

// static i2s_chan_handle_t tx_chan;        // I2S tx channel handler


#define DEBOUNCE_TIME_MS 50  // Debounce time in milliseconds

// Matrix state tracking
static bool key_states[NUM_ROWS][NUM_COLS] = {false};
static uint32_t last_press_time[NUM_ROWS][NUM_COLS] = {0};

SemaphoreHandle_t audio_semaphore = NULL;

QueueHandle_t keyboard_queue, audio_queue;

GameMode game_mode = MODE_SOLVE_MATH; // Default mode


#define MAX_LEDS 19

const static char *TAG = "TOYA2";

#define RMT_LED_STRIP_RESOLUTION_HZ 10000000 // 10MHz resolution, 1 tick = 0.1us (led strip needs a high resolution)
#define RMT_LED_STRIP_GPIO_NUM      14

// static uint8_t led_strip_pixels[9 * 3];


int num1, num2, correct_answer;
char operator;
char received_char;
bool game_active = true;
bool textChanged = 1;
int display_buffer[4] = {0};


rmt_channel_handle_t led_chan = NULL;
rmt_encoder_handle_t led_encoder = NULL;

    // max7219_t display;

#define SOUND_COOLDOWN_MS 50 // Adjust this based on your needs
static uint32_t last_sound_time = 0;



// Updated to use valid GPIO pins that support both input/output
// const gpio_num_t row_pins[NUM_ROWS] = {GPIO_NUM_21, GPIO_NUM_19, GPIO_NUM_18, GPIO_NUM_5, GPIO_NUM_17};
// const gpio_num_t col_pins[NUM_COLS] = {GPIO_NUM_32, GPIO_NUM_33, GPIO_NUM_25};

void configure_pins(const gpio_num_t *pins, size_t num_pins, gpio_mode_t mode, gpio_pullup_t pull_up, gpio_pulldown_t pull_down, gpio_int_type_t intr_type) {
    for (size_t i = 0; i < num_pins; i++) {
        gpio_config_t config = {
            .pin_bit_mask = (1ULL << pins[i]),
            .mode = mode,
            .pull_up_en = pull_up,
            .pull_down_en = pull_down,
            .intr_type = intr_type
        };
        gpio_config(&config);
    }
}

void gpio_init(){
    // Columns as outputs
    configure_pins(col_pins, NUM_COLS, GPIO_MODE_OUTPUT, GPIO_PULLUP_DISABLE, GPIO_PULLDOWN_ENABLE, GPIO_INTR_DISABLE);
    // Rows as inputs with pull-down
    configure_pins(row_pins, NUM_ROWS, GPIO_MODE_INPUT, GPIO_PULLUP_DISABLE, GPIO_PULLDOWN_ENABLE, GPIO_INTR_DISABLE);
    
    // Initialize all columns to low
    for (int i = 0; i < NUM_COLS; i++) {
        gpio_set_level(col_pins[i], 0);
    }
}

bool audioActive = 0;

void play_sound(char sound_number) {
    if (xSemaphoreTake(audio_semaphore, pdMS_TO_TICKS(10)) != pdTRUE) {
        ESP_LOGI("audio", "Audio busy, skipping playback");
        return;
    }

    audioActive = true;
    
    char full_path[64];
    snprintf(full_path, sizeof(full_path), "/spiffs/%c.pcm", sound_number);

    FILE* f = fopen(full_path, "rb");
    if (f == NULL) {
        ESP_LOGE("I2S", "Failed to open file: %s", full_path);
        audioActive = false;
        xSemaphoreGive(audio_semaphore);
        return;
    }

    // Get file size
    fseek(f, 0, SEEK_END);
    size_t file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    ESP_LOGI("I2S", "Starting playback of file: %s, size: %zu bytes", full_path, file_size);

    // Stop and clear any previous playback
    i2s_stop(I2S_NUM);
    i2s_zero_dma_buffer(I2S_NUM);

    // Start I2S
    esp_err_t err = i2s_start(I2S_NUM);
    if (err != ESP_OK) {
        ESP_LOGE("I2S", "Failed to start I2S: %d", err);
        fclose(f);
        audioActive = false;
        xSemaphoreGive(audio_semaphore);
        return;
    }

    // Use DMA buffer size for our read buffer
    int16_t buffer[1024];  // matches dma_buf_len
    size_t total_bytes_written = 0;

    while (total_bytes_written < file_size) {
        size_t bytes_read = fread(buffer, 1, sizeof(buffer), f);
        if (bytes_read == 0) break;

        size_t bytes_written = 0;
        err = i2s_write(I2S_NUM, buffer, bytes_read, &bytes_written, portMAX_DELAY);
        if (err != ESP_OK) {
            ESP_LOGE("I2S", "Error writing to I2S: %d", err);
            break;
        }

        total_bytes_written += bytes_written;
        
        // Small delay to prevent watchdog triggers
        vTaskDelay(1);
    }

    // Wait for the last buffer to be played
    // For 16kHz audio, calculate delay based on remaining bytes
    int remaining_audio_ms = (file_size - total_bytes_written) * 1000 / (16000 * 2);
    vTaskDelay(pdMS_TO_TICKS(remaining_audio_ms + 20));

    i2s_stop(I2S_NUM);
    i2s_zero_dma_buffer(I2S_NUM);
    fclose(f);
    
    ESP_LOGI("I2S", "Playback finished. Total bytes written: %zu of %zu", 
             total_bytes_written, file_size);
    
    audioActive = false;
    xSemaphoreGive(audio_semaphore);
}


void gpio_task(void *pvParameters) {
    keyboard_queue = xQueueCreate(10, sizeof(char));

    int level;
    gpio_init();

    while(1) {
        for (int col = 0; col < NUM_COLS; col++) {
            gpio_set_level(col_pins[col], 1);
            vTaskDelay(10 / portTICK_PERIOD_MS);
            
            for (int row = 0; row < NUM_ROWS; row++) {
                level = gpio_get_level(row_pins[row]);
                uint32_t current_time = esp_timer_get_time() / 1000; // Convert to milliseconds
                
                // If key is pressed (level == 1)
                if (level) {
                    // If key wasn't pressed before or enough time has passed since last press
                    if (!key_states[row][col] && 
                        (current_time - last_press_time[row][col]) > DEBOUNCE_TIME_MS) {
                        
                        key_states[row][col] = true;
                        last_press_time[row][col] = current_time;
                        ESP_LOGI("gpio", "Key released: %c", character[row][col] );
                        
                        
                        xQueueSend(keyboard_queue, &character[row][col], (TickType_t)200);
                        vTaskDelay(pdMS_TO_TICKS(500)); // 50 milliseconds delay
                        xQueueSend(audio_queue, &character[row][col], (TickType_t)100);

                        
                    }
                } else {
                    // Key is released
                    if (key_states[row][col]) {
                        key_states[row][col] = false;
                        // Optional: Handle key release event
                        
                    }
                }
            }
            
            gpio_set_level(col_pins[col], 0);
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}





void play_sound_with_debounce(char sound_number) {
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    if ((current_time - last_sound_time) >= SOUND_COOLDOWN_MS) {
        last_sound_time = current_time;
        play_sound(sound_number);
    } else {
        ESP_LOGI("audio", "Skipped sound: %c, due to cooldown", sound_number);
    }
}


void display_task()
{
    // Configure SPI bus
    spi_bus_config_t cfg = {
       .mosi_io_num = CONFIG_PIN_NUM_MOSI,
       .miso_io_num = -1,
       .sclk_io_num = CONFIG_PIN_NUM_CLK,
       .quadwp_io_num = -1,
       .quadhd_io_num = -1,
       .max_transfer_sz = 0,
       .flags = 0
    };
    ESP_ERROR_CHECK(spi_bus_initialize(HOST, &cfg, 1));

    // Configure device
    
    max7219_t display = {
       .cascade_size = CONFIG_CASCADE_SIZE,
       .digits = 0,
       .mirrored = true
    };
 
    ESP_ERROR_CHECK(max7219_init_desc(&display, HOST, MAX7219_MAX_CLOCK_SPEED_HZ, CONFIG_PIN_CS));
    ESP_ERROR_CHECK(max7219_init(&display));

    while (1)
    {   
        if(textChanged){
        for(int i = 0; i < CONFIG_CASCADE_SIZE; i++){
            max7219_draw_image_8x8(&display, i * 8, (uint8_t *)&symbols[display_buffer[i]]);
            vTaskDelay(pdMS_TO_TICKS(CONFIG_SCROLL_DELAY));
        }
        textChanged = 0;
        }
        else {
            vTaskDelay(pdMS_TO_TICKS(200));
        }
    }
}


esp_err_t init_i2s(void) {
    i2s_config_t i2s_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_TX,
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .dma_buf_count = DMA_BUF_COUNT,
        .dma_buf_len = DMA_BUF_LEN,
        .use_apll = false,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_BCK_IO,
        .ws_io_num = I2S_WS_IO,
        .data_out_num = I2S_DO_IO,
        .data_in_num = I2S_PIN_NO_CHANGE
    };
audio_semaphore = xSemaphoreCreateBinary();
xSemaphoreGive(audio_semaphore); // Initialize as available


    esp_err_t ret = i2s_driver_install(I2S_NUM, &i2s_config, 0, NULL);
    if (ret != ESP_OK) return ret;


    return i2s_set_pin(I2S_NUM, &pin_config);
}





int generate_random(int min, int max) {
    return min + esp_random() % (max - min + 1);
}


void math_game_task(void *pvParameters) {
    audio_queue = xQueueCreate(10, sizeof(char));

    // int num1, num2, correct_answer;
    // char operator;
    // char received_char;

    // Generate initial question
    generate_new_question(&num1, &num2, &operator, &correct_answer, game_mode);
while (game_active) {
        switch (game_mode) {
            case MODE_SOLVE_MATH:
                printf("\nSolve: %d %c %d = ?\n", num1, operator, num2);
                break;
            case MODE_FIND_NUMBER:
                printf("\nFind the number: %d\n", correct_answer);
                break;
            case MODE_FREE:
                printf("\nFree mode: Press any key to play sound and display character\n");
                break;
        }

        while (1) {
            // Wait for character from keyboard queue
            if (xQueueReceive(keyboard_queue, &received_char, portMAX_DELAY) == pdTRUE) {
                if (received_char == 'M') {
                    // Switch to the next game mode
                    game_mode = (game_mode + 1) % 3; // Cycle through modes (0: SOLVE_MATH, 1: FIND_NUMBER, 2: FREE)
                    printf("Switched to mode: %d\n", game_mode);
                    generate_new_question(&num1, &num2, &operator, &correct_answer, game_mode);
                    xQueueSend(audio_queue, (char[]){ 'n' }, (TickType_t)0);
                    break; // Exit inner loop to display new question
                }

                switch (game_mode) {
                    case MODE_SOLVE_MATH: {
                        int user_answer = received_char - '0'; // Convert ASCII to integer
                        if (user_answer == correct_answer) {
                            printf("\nCorrect! Well done!\n");
                            xQueueSend(audio_queue, (char[]){ 'c' }, (TickType_t)0);
                            generate_new_question(&num1, &num2, &operator, &correct_answer, game_mode);
                            break; // Exit inner loop after correct answer
                        } else {
                            printf("\nIncorrect. Try again!\n");
                            xQueueSend(audio_queue, (char[]){ 'w' }, (TickType_t)0);
                        }
                        break;
                    }

                    case MODE_FIND_NUMBER: {
                        int user_answer = received_char - '0'; // Convert ASCII to integer
                        if (user_answer == correct_answer) {
                            printf("\nCorrect! You found the number!\n");
                            xQueueSend(audio_queue, (char[]){ 'c' }, (TickType_t)0);
                            generate_new_question(&num1, &num2, &operator, &correct_answer, game_mode);
                            break; // Exit inner loop after correct answer
                        } else {
                            printf("\nIncorrect. Try again!\n");
                            xQueueSend(audio_queue, (char[]){ 'w' }, (TickType_t)0);
                        }
                        break;
                    }

                    case MODE_FREE:
                        // Play sound for the pressed button
                        // xQueueSend(audio_queue, &received_char, (TickType_t)0);
                        display_buffer[0] = (int) received_char - '0';
                        // display_buffer[1] = BLANK_CHAR_INDEX,
                        // displa
                        textChanged = 1;
                        // Display the pressed character
                        printf("Pressed: %c\n", received_char);
                        break;
                }

                // Small delay to prevent tight looping
                vTaskDelay(pdMS_TO_TICKS(100));
            }
        }
    }

}

void generate_new_question(int *pnum1, int *pnum2, char *pOperator, int *pCorrect_answer, GameMode game_mode) {
    // Update the display buffer for non math solve mode
    if(game_mode != MODE_SOLVE_MATH){
    display_buffer[0] =  BLANK_CHAR_INDEX ;
    display_buffer[1] = BLANK_CHAR_INDEX;
    display_buffer[2] =  BLANK_CHAR_INDEX ;
    display_buffer[3] =  BLANK_CHAR_INDEX;
    }
    
    switch (game_mode) {
        case MODE_SOLVE_MATH:
            // Generate a math problem
            do {
                *pnum1 = generate_random(0, 9);
                *pnum2 = generate_random(0, 9);
                *pOperator = generate_operator();
                *pCorrect_answer = calculate_answer(*pnum1, *pnum2, *pOperator, display_buffer); //also update display_buffer for operator
            } while (*pCorrect_answer > 9 || *pCorrect_answer < 0);

            display_buffer[0] =  *pnum1 ;
            display_buffer[2] =  *pnum2 ;
            display_buffer[3] =  *pCorrect_answer;
            break;

        case MODE_FIND_NUMBER:
            // Generate a random number for the user to find
            *pCorrect_answer = generate_random(0, 9);
            *pnum1 = *pCorrect_answer; // Store the correct answer in pnum1 for display purposes
            *pnum2 = 0; // Not used in this mode
            *pOperator = ' '; // Not used in this mode
            display_buffer[0] = *pCorrect_answer;
            break;

        case MODE_FREE:
            // No question generation needed for free mode
            *pnum1 = 0;
            *pnum2 = 0;
            *pOperator = ' ';
            *pCorrect_answer = 0;
            break;
    }

    ESP_LOGI("game", "%d %c %d = %d mode %d", *pnum1, *pOperator, *pnum2, *pCorrect_answer, game_mode);

    
    textChanged =1;
}

// Math game task
// void math_game_task(void *pvParameters) {
    
//     keyboard_queue = xQueueCreate(10, sizeof(char));

//     // Generate initial question
//     generate_new_question(&num1, &num2, &operator, &correct_answer);
    
// while(game_active) {
//     // Display current question
//     printf("\nSolve: %d %c %d = %d ?\n", num1, operator, num2, correct_answer);
//     // char status;
//     while(1) {
//         // Wait for character from keyboard queue
//         if(xQueueReceive(keyboard_queue, &received_char, portMAX_DELAY) == pdTRUE) {
//             if(received_char == 'M') {
//                 generate_new_question(&num1, &num2, &operator, &correct_answer);
//                 printf("new question is generated");
//                 xQueueSend(audio_queue, (char[]){ 'n' }, (TickType_t)0);

//                 // play_sound_with_debounce('n');
//                 continue;
//             }
            
//             int user_answer = received_char - '0'; // Convert ASCII to integer
            
//             if(user_answer == correct_answer) {
//                 printf("\nCorrect! Well done!\n");
//                 // play_sound_with_debounce('c');
//                 xQueueSend(audio_queue, (char[]){ 'c' }, (TickType_t)0);

//                 generate_new_question(&num1, &num2, &operator, &correct_answer);
//                 break; // Exit inner loop after correct answer
//             } else {
//                 printf("\nIncorrect. Try again!\n");
//                 // play_sound_with_debounce('f');
//                 xQueueSend(audio_queue, (char[]){ 'f' }, (TickType_t)0);

//             }
            
//             // Small delay to prevent tight looping
//             vTaskDelay(pdMS_TO_TICKS(100));
//         }
//     }
// }
// }
// Helper function to generate a new question
// static void generate_new_question(int *num1, int *num2, char *operator, int *correct_answer) {
//      // Ensure single-digit and positive answers 
//     do {   
//         *num1 = generate_random(0, 9);
//         *num2 = generate_random(0, 9);
//         *operator = generate_operator();
//         *correct_answer = calculate_answer(*num1, *num2, *operator, display_buffer);
//         display_buffer[0] = *num1;
//         display_buffer[2] = *num2;
//         display_buffer[3]  = *correct_answer;

//     } while (*correct_answer > 9 || *correct_answer < 0);
//     textChanged = 1;

// }

// Calculate answer based on operator
static int calculate_answer(int num1, int num2, char operator, int *code_operator) {
    switch(operator) {
        case '+':
            code_operator[1] = 10;
            return num1 + num2;
            break;
        case '-':
            code_operator[1] = 11;
            return num1 - num2;
            break;
        case '*':
            code_operator[1] = 12;
            return num1 * num2;
            break;
        default:
            return 0;
            break;
    }
}

// Generate random operator
static char generate_operator(void) {
    char operators[] = {'+', '-', '*'};
    return operators[generate_random(0, 2)];
}

// play_sound(0)

void init_spiffs(){
    esp_vfs_spiffs_conf_t conf = {
    .base_path = "/spiffs",
    .partition_label = NULL,
    .max_files = 5,
    .format_if_mount_failed = false
    };
    esp_vfs_spiffs_register(&conf);


    size_t total = 0, used = 0;
    esp_err_t ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }

}

void audio_task(){
    char received_char;
    audio_queue = xQueueCreate(10, sizeof(char));

    while(1){
    if(xQueueReceive(audio_queue, &received_char,pdMS_TO_TICKS(10))){
        ESP_LOGI("audio", "%c", received_char);
        play_sound_with_debounce(received_char);
    }
    vTaskDelay(pdMS_TO_TICKS(50));
}
}

void app_main(void)
{   
    init_spiffs();
    init_i2s();
    gpio_init();


  
    xTaskCreate(gpio_task, "matrix task", 4096, NULL, 10, NULL);
    
    xTaskCreate(display_task, "display_task", 4096, NULL, 6, NULL);
    xTaskCreate(audio_task, "audio_task", 4096, NULL, 4, NULL);
    xTaskCreate(math_game_task, "game_task", 4096 * 2, NULL, 5, NULL);
}
