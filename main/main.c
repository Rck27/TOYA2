#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "button.h"
#include "matrix_keyboard.h"
#include "max7219.h"
#include "driver/i2s.h"
#include "esp_mac.h"
#include "esp_spiffs.h"

#include "esp_random.h"
#include "esp_system.h"
#include "string.h"
#include "main.h"
#include "led.c"

int num1, num2, correct_answer;
char operator;
char received_char;
bool game_active = true;


#define CONFIG_EXAMPLE_SCROLL_DELAY 200
#define CONFIG_EXAMPLE_DELAY 500
#define CONFIG_EXAMPLE_CASCADE_SIZE 4

#define CONFIG_EXAMPLE_PIN_NUM_CLK GPIO_NUM_36
#define CONFIG_EXAMPLE_PIN_NUM_MOSI GPIO_NUM_35
#define CONFIG_EXAMPLE_PIN_CS GPIO_NUM_34


#define HOST SPI2_HOST

int display_buffer[4] = {0};

static const uint64_t symbols[] = {
    0x3c66666e76663c00, //0
    0x7e1818181c181800, // digits
    0x7e060c3060663c00,
    0x3c66603860663c00,
    0x30307e3234383000,
    0x3c6660603e067e00,
    0x3c66663e06663c00,
    0x1818183030667e00,
    0x3c66663c66663c00,
    0x3c66607c66663c00, //9
    0x0808087c08080800, //+ 10
    0x0000007c00000000, // - 11
    0x0042241818244200 //* 12
    
};
// static const size_t symbols_size = sizeof(symbols) - sizeof(uint64_t) * CONFIG_EXAMPLE_CASCADE_SIZE;

void task(void *pvParameter)
{
    // Configure SPI bus
    spi_bus_config_t cfg = {
       .mosi_io_num = CONFIG_EXAMPLE_PIN_NUM_MOSI,
       .miso_io_num = -1,
       .sclk_io_num = CONFIG_EXAMPLE_PIN_NUM_CLK,
       .quadwp_io_num = -1,
       .quadhd_io_num = -1,
       .max_transfer_sz = 0,
       .flags = 0
    };
    ESP_ERROR_CHECK(spi_bus_initialize(HOST, &cfg, 1));

    // Configure device
    max7219_t dev = {
       .cascade_size = CONFIG_EXAMPLE_CASCADE_SIZE,
       .digits = 0,
       .mirrored = true
    };
    ESP_ERROR_CHECK(max7219_init_desc(&dev, HOST, MAX7219_MAX_CLOCK_SPEED_HZ, CONFIG_EXAMPLE_PIN_CS));
    ESP_ERROR_CHECK(max7219_init(&dev));
    // char buf[10]; // 8 digits + decimal point + \0
// display_buffer[0] =  num1;
//     display_buffer[1] = operator;
//     display_buffer[2]  = num2;
//     display_buffer[3]  = correct_answer;
    while (1)
    {   
        for(int i = 0; i < CONFIG_EXAMPLE_CASCADE_SIZE; i++){
            max7219_draw_image_8x8(&dev, i * 8, (uint8_t *)&symbols[display_buffer[i]]);
            vTaskDelay(pdMS_TO_TICKS(CONFIG_EXAMPLE_SCROLL_DELAY));

        }
    //    printf("---------- draw\n");
    //    for(int i = 0; i < 9; i++){
    //     // max7219_clear(&dev);
    //     max7219_draw_image_8x8(&dev, 2 * 8, (uint8_t *)&symbols[i]);
    //    }
        // max7219_draw_image_8x8(&dev, 1, (uint8_t *)symbols + 2 * 8);
        // for (uint8_t c = 0; c < CONFIG_EXAMPLE_CASCADE_SIZE; c ++)
            // max7219_draw_image_8x8(&dev, c * 8, (uint8_t *)symbols + c * 8 + offs);

    }
}


const static char *TAG = "TOYA2";

QueueHandle_t keyboard_queue;


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

esp_err_t init_spiffs(void) {
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true
    };
    return esp_vfs_spiffs_register(&conf);
}

void play_sound(uint8_t sound_number) {
    FILE* f = fopen(PCM_FILE_PATH, "rb");
    if (f == NULL) {
        printf("Failed to open file\n");
        return;
    }

    // Seek to the correct position in the file
    fseek(f, sound_number * SOUND_BLOCK_SIZE, SEEK_SET);

    // Read and play the sound
    int16_t buffer[DMA_BUF_LEN];
    size_t bytes_read;
    size_t bytes_written;
    size_t bytes_remaining = SOUND_BLOCK_SIZE;

    while (bytes_remaining > 0) {
        // Read chunk from file
        size_t chunk_size = (bytes_remaining < DMA_BUF_LEN * 2) ? 
                            bytes_remaining : DMA_BUF_LEN * 2;
        bytes_read = fread(buffer, 1, chunk_size, f);
        
        if (bytes_read == 0) break;

        // Write to I2S
        i2s_write(I2S_NUM, buffer, bytes_read, &bytes_written, portMAX_DELAY);
        bytes_remaining -= bytes_read;
    }

    fclose(f);
}


int generate_random(int min, int max) {
    return min + esp_random() % (max - min + 1);
}

// Math game task
void math_game_task(void *pvParameters) {
    
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
                if(received_char == 'M') {
                    generate_new_question(&num1, &num2, &operator, &correct_answer);
                    printf("new question is generated");
                }
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
        *correct_answer = calculate_answer(*num1, *num2, *operator, display_buffer);
        display_buffer[0] = *num1;
        // if(operator == '+') display_buffer[1] = 10;
        // if(operator == '-') display_buffer[1] = 11;
        // if(operator == '*') display_buffer[1] = 12;
        // display_buffer[1] =  12;
        display_buffer[2] = *num2;
        display_buffer[3]  = 0;
    } while (*correct_answer > 9 && *correct_answer < 0); // Ensure single-digit and positive answers
    
}

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

// play_sound(0)


void app_main(void)
{   
    // ESP_ERROR_CHECK(init_spiffs());
    // ESP_ERROR_CHECK(init_i2s());
    keyboard_init();
    led_task();
    xTaskCreate(task, "task", configMINIMAL_STACK_SIZE * 3, NULL, 5, NULL);

    xTaskCreate(math_game_task, "game_task", 2048, NULL, 5, NULL);
}
