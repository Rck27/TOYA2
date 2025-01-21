

#define CONFIG_EXAMPLE_SCROLL_DELAY 200
#define CONFIG_EXAMPLE_DELAY 500
#define CONFIG_EXAMPLE_CASCADE_SIZE 4

#define CONFIG_EXAMPLE_PIN_NUM_CLK GPIO_NUM_36
#define CONFIG_EXAMPLE_PIN_NUM_MOSI GPIO_NUM_35
#define CONFIG_EXAMPLE_PIN_CS GPIO_NUM_34

#define HOST SPI2_HOST


int COL_GPIO[] = {12, 11, 9};
int ROW_GPIO[] = {7, 5, 3, }; // 1 2


#define I2S_NUM         I2S_NUM_0
#define I2S_BCK_IO      GPIO_NUM_25
#define I2S_WS_IO       GPIO_NUM_33
#define I2S_DO_IO       GPIO_NUM_26
#define SAMPLE_RATE     16000
#define DMA_BUF_COUNT   3
#define DMA_BUF_LEN     1024

// Audio file configuration
#define SOUND_BLOCK_SIZE (24000 * 2)  // 1 second of audio at 44.1kHz, 16-bit

int Current_LED_INDEX; // LED index only when the button is pressed (temporary)



const uint64_t symbols[] = {
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

// Key matrix// 3x5 Character Matrix
char character[3][5] = {
    {'1', '2', '3', '4', '5'},
    {'6', '7', '8', '9', '0'},
    {'+', '-', 'X', '=', 'M'}
};

// 3x5 LED Index Matrix (Snake Pattern)
int led_index[3][5] = {
    {1, 2, 3, 4, 5},
    {10, 9, 8, 7, 6},
    {11, 12, 13, 14, 15}
};
int get_led_index(int , int);
void play_sound(char sound_number);


static void generate_new_question(int *num1, int *num2, char *operator, int *correct_answer);
static int calculate_answer(int num1, int num2, char operator, int *display_buffer);
static char generate_operator(void);