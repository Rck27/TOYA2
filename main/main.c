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
#define NUM_ROWS 3
#define NUM_COLS 5
// static i2s_chan_handle_t tx_chan;        // I2S tx channel handler


#define DEBOUNCE_TIME_MS 50  // Debounce time in milliseconds

// Matrix state tracking
static bool key_states[NUM_ROWS][NUM_COLS] = {false};
static uint32_t last_press_time[NUM_ROWS][NUM_COLS] = {0};


// Updated to use valid GPIO pins that support both input/output
// const gpio_num_t row_pins[NUM_ROWS] = {GPIO_NUM_21, GPIO_NUM_19, GPIO_NUM_18, GPIO_NUM_5, GPIO_NUM_17};
// const gpio_num_t col_pins[NUM_COLS] = {GPIO_NUM_32, GPIO_NUM_33, GPIO_NUM_25};

const gpio_num_t col_pins[NUM_COLS] = {18, 21,15,4, 14};
const gpio_num_t row_pins[NUM_ROWS]= {19, 17, 16};

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

void play_sound(char sound_number) {
    // Create the full path: /spiffs/[number].pcm
    char full_path[64];  // Adjust size as needed
    snprintf(full_path, sizeof(full_path), "/spiffs/%c.pcm", sound_number);

    FILE* f = fopen(full_path, "rb");
    if (f == NULL) {
        printf("Failed to open file: %s\n", full_path);
        return;
    }

    // Get file size
    fseek(f, 0, SEEK_END);
    size_t file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    ESP_LOGI("I2S", "File size: %zu bytes", file_size);

    // Prepare playback buffer
    int16_t buffer[DMA_BUF_LEN];  // DMA_BUF_LEN should match your I2S configuration
    size_t bytes_read = 0;
    size_t bytes_written = 0;
    size_t bytes_remaining = file_size;

    // Start I2S playback
    i2s_start(I2S_NUM);

    // Playback loop
    while (bytes_remaining > 0) {
        // Read a chunk from the file
        size_t chunk_size = (bytes_remaining < sizeof(buffer)) ? 
                            bytes_remaining : sizeof(buffer);
        bytes_read = fread(buffer, 1, chunk_size, f);

        if (bytes_read == 0) {
            ESP_LOGE("I2S", "Error or end of file reached.");
            break;
        }

        // Optional: Adjust volume or process data
        for (size_t i = 0; i < bytes_read / sizeof(int16_t); i++) {
            buffer[i] = buffer[i] << 1;  // Simple volume scaling (adjust as needed)
        }

        // Write to I2S
        i2s_write(I2S_NUM, buffer, bytes_read, &bytes_written, portMAX_DELAY);

        // Update bytes remaining
        bytes_remaining -= bytes_read;
    }

    // Send silence to flush buffers
    memset(buffer, 0, sizeof(buffer));
    // for (int i = 0; i < 3; i++) {
    //     i2s_write(I2S_NUM, buffer, sizeof(buffer), &bytes_written, portMAX_DELAY);
    // }

    // Stop I2S playback
    i2s_stop(I2S_NUM);

    // Cleanup
    fclose(f);
    ESP_LOGI("I2S", "Playback finished for file: %s", full_path);
}



// void play_sound(char sound_number) {
//     // Create the full path: /spiffs/[number].pcm
//     char full_path[64];  // Adjust size as needed
//     snprintf(full_path, sizeof(full_path), "/spiffs/%c.pcm", sound_number);

//     FILE* f = fopen(full_path, "rb");
//     if (f == NULL) {
//         printf("Failed to open file: %s\n", full_path);
//         return;
//     }

//     // Get file size
//     fseek(f, 0, SEEK_END);
//     size_t file_size = ftell(f);
//     fseek(f, 0, SEEK_SET);

//     ESP_LOGI("I2S", "size is %zu", file_size);

//     // Prepare buffer and playback variables
//     int16_t buffer[DMA_BUF_LEN];  // DMA_BUF_LEN should match your I2S configuration
//     size_t bytes_read = 0;
//     size_t bytes_written = 0;
//     size_t bytes_remaining = file_size;

//     // Start playback loop
//     // i2s_channel_enable(tx_chan);
//     i2s_start(I2S_NUM);
//     while (bytes_remaining > 0) {
//         // Read chunk from the file
//         size_t chunk_size = (bytes_remaining < sizeof(buffer)) ? 
//                             bytes_remaining : sizeof(buffer);
//         bytes_read = fread(buffer, 1, chunk_size, f);

//         if (bytes_read == 0) {
//             printf("Error or end of file reached.\n");
//             break;
//         }

//         // Write to I2S
//         // i2s_channel_write(tx_chan, buffer, bytes_read * 2, &bytes_written, portMAX_DELAY);
//         i2s_write(I2S_NUM, buffer, bytes_read, &bytes_written, portMAX_DELAY);

//         // Update bytes remaining
//         bytes_remaining -= bytes_read;
//     }
//     i2s_stop(I2S_NUM);
//     // i2s_channel_disable(tx_chan);

//     // Cleanup
//     fclose(f);
//     printf("Playback finished for file: %s\n", full_path);
// }



void gpio_task(void *pvParameters) {
    int level;
    gpio_init();

    while(1) {
        for (int col = 0; col < NUM_COLS; col++) {
            gpio_set_level(col_pins[col], 1);
            vTaskDelay(1 / portTICK_PERIOD_MS);
            
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
                        play_sound(character[row][col]);
                        
                        // Add your key press handling code here
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
            vTaskDelay(5 / portTICK_PERIOD_MS);
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}




#define MAX_LEDS 19

const static char *TAG = "TOYA2";

#define RMT_LED_STRIP_RESOLUTION_HZ 10000000 // 10MHz resolution, 1 tick = 0.1us (led strip needs a high resolution)
#define RMT_LED_STRIP_GPIO_NUM      14

// static uint8_t led_strip_pixels[9 * 3];


int num1, num2, correct_answer;
char operator;
char received_char;
bool game_active = true;
int display_buffer[4] = {0};


rmt_channel_handle_t led_chan = NULL;
rmt_encoder_handle_t led_encoder = NULL;




// void init_i2s(void) {
//     i2s_chan_config_t tx_chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
//     ESP_ERROR_CHECK(i2s_new_channel(&tx_chan_cfg, &tx_chan, NULL));


//     i2s_std_config_t tx_std_cfg = {
//             .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
//             .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
//                                                         I2S_SLOT_MODE_MONO),

//             .gpio_cfg = {
//                     .mclk = I2S_GPIO_UNUSED,    // some codecs may require mclk signal, this example doesn't need it
//                     .bclk = I2S_BCK_IO,
//                     .ws   = I2S_WS_IO,
//                     .dout = I2S_DO_IO,
//                     .din  = GPIO_NUM_NC,
//                     .invert_flags = {
//                             .mclk_inv = false,
//                             .bclk_inv = false,
//                             .ws_inv   = false,
//                     },
//             },
//     };
//     ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_chan, &tx_std_cfg));

//     ESP_ERROR_CHECK(i2s_channel_enable(tx_chan));
// }



// void display_task(void *pvParameter)
// {
//     // Configure SPI bus
//     spi_bus_config_t cfg = {
//        .mosi_io_num = CONFIG_EXAMPLE_PIN_NUM_MOSI,
//        .miso_io_num = -1,
//        .sclk_io_num = CONFIG_EXAMPLE_PIN_NUM_CLK,
//        .quadwp_io_num = -1,
//        .quadhd_io_num = -1,
//        .max_transfer_sz = 0,
//        .flags = 0
//     };
//     ESP_ERROR_CHECK(spi_bus_initialize(HOST, &cfg, 1));

//     // Configure device
//     max7219_t dev = {
//        .cascade_size = CONFIG_EXAMPLE_CASCADE_SIZE,
//        .digits = 0,
//        .mirrored = true
//     };
//     ESP_ERROR_CHECK(max7219_init_desc(&dev, HOST, MAX7219_MAX_CLOCK_SPEED_HZ, CONFIG_EXAMPLE_PIN_CS));
//     ESP_ERROR_CHECK(max7219_init(&dev));

//     while (1)
//     {   
//         for(int i = 0; i < CONFIG_EXAMPLE_CASCADE_SIZE; i++){
//             max7219_draw_image_8x8(&dev, i * 8, (uint8_t *)&symbols[display_buffer[i]]);
//             vTaskDelay(pdMS_TO_TICKS(CONFIG_EXAMPLE_SCROLL_DELAY));
//         }
//     }
// }


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

    esp_err_t ret = i2s_driver_install(I2S_NUM, &i2s_config, 0, NULL);
    if (ret != ESP_OK) return ret;
    return i2s_set_pin(I2S_NUM, &pin_config);
}





// int generate_random(int min, int max) {
//     return min + esp_random() % (max - min + 1);
// }

// // Math game task
// void math_game_task(void *pvParameters) {
    
//     keyboard_queue = xQueueCreate(10, sizeof(char));

//     // Generate initial question
//     generate_new_question(&num1, &num2, &operator, &correct_answer);
    

//     while(game_active) {
//         // Display current question
//         printf("\nSolve: %d %c %d = ?\n", num1, operator, num2);
        
//         bool question_active = true;
//         while(question_active) {
//             // Wait for character from keyboard queue
//             if(xQueueReceive(keyboard_queue, &received_char, portMAX_DELAY) == pdTRUE) {
//                 if(received_char == 'M') {
//                     generate_new_question(&num1, &num2, &operator, &correct_answer);
//                     printf("new question is generated");
//                 }
//                 int user_answer = received_char - '0'; // Convert ASCII to integer
                
//                 if(user_answer == correct_answer) {
//                     printf("\nCorrect! Well done!\n");
//                     // Generate new question only after correct answer
//                     generate_new_question(&num1, &num2, &operator, &correct_answer);
//                     question_active = false;
//                 } else {
//                     printf("\nIncorrect. Try again!\n");
//                 }
                
//                 // Small delay for readability
//                 vTaskDelay(pdMS_TO_TICKS(1000));
//             }
//         }
//     }
// }

// // Helper function to generate a new question
// static void generate_new_question(int *num1, int *num2, char *operator, int *correct_answer) {
//      // Ensure single-digit and positive answers 
//     do {   
//         *num1 = generate_random(0, 9);
//         *num2 = generate_random(0, 9);
//         *operator = generate_operator();
//         *correct_answer = calculate_answer(*num1, *num2, *operator, display_buffer);
//         display_buffer[0] = *num1;
//         display_buffer[2] = *num2;
//         display_buffer[3]  = 0;

//     } while (*correct_answer > 9 || *correct_answer < 0);
    
// }

// // Calculate answer based on operator
// static int calculate_answer(int num1, int num2, char operator, int *code_operator) {
//     switch(operator) {
//         case '+':
//             code_operator[1] = 10;
//             return num1 + num2;
//             break;
//         case '-':
//             code_operator[1] = 11;
//             return num1 - num2;
//             break;
//         case '*':
//             code_operator[1] = 12;
//             return num1 * num2;
//             break;
//         default:
//             return 0;
//             break;
//     }
// }

// // Generate random operator
// static char generate_operator(void) {
//     char operators[] = {'+', '-', '*'};
//     return operators[generate_random(0, 2)];
// }

// int get_led_index(int row, int col){
//     return led_index[row][col];
// }

// esp_err_t init_led_strip(void) {
//     // Configure RMT TX channel

//     rmt_tx_channel_config_t tx_chan_config = {
//         .clk_src = RMT_CLK_SRC_DEFAULT,
//         .gpio_num = RMT_LED_STRIP_GPIO_NUM,
//         .mem_block_symbols = 64,
//         .resolution_hz = RMT_LED_STRIP_RESOLUTION_HZ,
//         .trans_queue_depth = 4,
//     };
//     ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &led_chan));

//     // Configure LED strip encoder
//     led_strip_encoder_config_t encoder_config = {
//         .resolution = RMT_LED_STRIP_RESOLUTION_HZ,
//     };
//     ESP_ERROR_CHECK(rmt_new_led_strip_encoder(&encoder_config, &led_encoder));

//     // Enable RMT TX channel
//     ESP_ERROR_CHECK(rmt_enable(led_chan));

//     return ESP_OK;
// }

// esp_err_t set_led(uint32_t index, uint8_t red, uint8_t green, uint8_t blue) {
//     if (index >= MAX_LEDS) {
//         return ESP_ERR_INVALID_ARG;
//     }

//     // Clear buffer first
//     memset(led_strip_pixels, 0, sizeof(led_strip_pixels));

//     // Set RGB values for specified LED
//     led_strip_pixels[index * 3 + 0] = green;  // GRB format
//     led_strip_pixels[index * 3 + 1] = blue;
//     led_strip_pixels[index * 3 + 2] = red;

//     // Transmit configuration
//     rmt_transmit_config_t tx_config = {
//         .loop_count = 0,
//     };

//     // Send data to LED strip
//     ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &tx_config));
//     ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));

//     return ESP_OK;
// }

// // void led_task(){
// //     init_led_strip();

// //     ESP_LOGI(TAG, "Start blinking LED strip");
// //     while (1) {
// //     if(Current_LED_INDEX > 0){
// //     // ws2812_set_led(Current_LED_INDEX, 100, 0, 0);  // Set first LED
// //     ESP_LOGI("LED", "LED-%d ON", Current_LED_INDEX);
// //     }
// //     vTaskDelay(pdMS_TO_TICKS(500));
// //     }
// // }


// char kbd_handler(matrix_kbd_handle_t mkbd_handle, matrix_kbd_event_id_t event, void *event_data, void *handler_args)
// {   

//     uint32_t key_code = (uint32_t)event_data;
//     int col = key_code >> 8;    // Get first 2 digits (01)
//     int row = key_code & 0xFF;  // Get last 2 digits (04)

    
//     switch (event) {
//     case MATRIX_KBD_EVENT_DOWN:
//         // Current_LED_INDEX = get_led_index(row, col);
//         ESP_LOGI(TAG, " press : %c %d %d, LED-%d",character[row][col], col, row, 0);
//         // set_led(Current_LED_INDEX, 50, 0, 0);  // Set the LED on after pressed
//         // xQueueSend(keyboard_queue, &character[row][col], portMAX_DELAY);
//         break;
//     case MATRIX_KBD_EVENT_UP:
//             ESP_LOGI(TAG, " release : %c %d %d, LED-%d",character[row][col], col, row, 0);

//         // set_led(Current_LED_INDEX, 0, 0, 0);
//         // Current_LED_INDEX = 0;


//         // ESP_LOGI("IDK", "%s", xTaskGetCurrentTaskHandle());
//         // ESP_LOGI(TAG, "release event,key %c, key code = %04"PRIx32,character[row][col], key_code);
//         break;
//     }

//     return character[row][col];
// }



// char kbd_handler(matrix_kbd_handle_t mkbd_handle, matrix_kbd_event_id_t event, void *event_data, void *handler_args)
// {   

//     uint32_t key_code = (uint32_t)event_data;
//     int col = key_code >> 8;    // Get first 2 digits (01)
//     int row = key_code & 0xFF;  // Get last 2 digits (04)

    
//     switch (event) {
//     case MATRIX_KBD_EVENT_DOWN:
//         Current_LED_INDEX = get_led_index(row, col);
//         ESP_LOGI(TAG, " press : %c %d %d, LED-%d",character[row][col], col, row, Current_LED_INDEX);
//         set_led(Current_LED_INDEX, 50, 0, 0);  // Set the LED on after pressed
//         // xQueueSend(keyboard_queue, &character[row][col], portMAX_DELAY);
//         break;
//     case MATRIX_KBD_EVENT_UP:
//         set_led(Current_LED_INDEX, 0, 0, 0);
//         Current_LED_INDEX = 0;


//         // ESP_LOGI("IDK", "%s", xTaskGetCurrentTaskHandle());
//         // ESP_LOGI(TAG, "release event,key %c, key code = %04"PRIx32,character[row][col], key_code);
//         break;
//     }

//     return character[row][col];
// }

// static void keyboard_init(){
//     matrix_kbd_handle_t kbd = NULL;
//     matrix_kbd_config_t config = MATRIX_KEYBOARD_DEFAULT_CONFIG();
 
//     config.row_gpios = ROW_GPIO;
//     config.nr_row_gpios = 3;
//     config.col_gpios = COL_GPIO;
//     config.nr_col_gpios = 3;
//     matrix_kbd_install(&config, &kbd);
//     matrix_kbd_register_event_handler(kbd, kbd_handler, NULL);
//     matrix_kbd_start(kbd);
// }

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

void app_main(void)
{   
    init_spiffs();
    init_i2s();
    gpio_init();

  
    xTaskCreate(gpio_task, "matrix task", 4096, NULL, 5, NULL);
    // init_led_strip();
    // led_task();
    // xTaskCreate(display_task, "display_task", configMINIMAL_STACK_SIZE * 3, NULL, 5, NULL);

    // xTaskCreate(math_game_task, "game_task", 2048, NULL, 5, NULL);
}
