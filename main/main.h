
int COL_GPIO[] = {7, 9, 11, 12};
int ROW_GPIO[] = {2, 1, 3, 5};

#define I2S_NUM         I2S_NUM_0
#define I2S_BCK_IO      GPIO_NUM_26
#define I2S_WS_IO       GPIO_NUM_21
#define I2S_DO_IO       GPIO_NUM_20
#define SAMPLE_RATE     44100
#define DMA_BUF_COUNT   8
#define DMA_BUF_LEN     1024

// Audio file configuration
#define SOUND_BLOCK_SIZE (44100 * 2)  // 1 second of audio at 44.1kHz, 16-bit
#define PCM_FILE_PATH "/spiffs/sounds.pcm"

int Current_LED_INDEX; // LED index only when the button is pressed (temporary)



// Key matrix
char character[4][4] = {
    {'1', '2', '3', '4'},
    {'5', '6', '7', '8'},
    {'9', '0', '+', '-'},
    {'X', '/', '=', 'M'}
};

// Corresponding LED index matrix
int led_index[4][4] = {
    {1, 2, 3, 4}, 
    {8, 7, 6, 5}, 
    {9, 10, 11, 12}, 
    {16, 15, 14, 13}
};

int get_led_index(int , int);
static void generate_new_question(int *num1, int *num2, char *operator, int *correct_answer);

static int calculate_answer(int num1, int num2, char operator, int *display_buffer);
static char generate_operator(void);