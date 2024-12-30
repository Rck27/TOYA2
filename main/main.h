
int COL_GPIO[] = {7, 9, 11, 12};
int ROW_GPIO[] = {2, 1, 3, 5};

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