// Code by Nguyen Minh Tri
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_err.h"

#include "nvs.h"
#include "nvs_flash.h"

#include "buzzer.h"
#include "pitch.h"
#include "melody.h"

#include "ssd1306.h"
#include "font8x8_basic.h"

#define TAG "SnakeGame"

// #define BUTTON_UP_PIN 4
// #define BUTTON_DOWN_PIN 5
// #define BUTTON_LEFT_PIN 15
// #define BUTTON_RIGHT_PIN 18
// #define BUTTON_RESET_PIN 2

#define BUTTON_UP_PIN 4
#define BUTTON_DOWN_PIN 2
#define BUTTON_LEFT_PIN 5
#define BUTTON_RIGHT_PIN 18
#define BUTTON_RESET_PIN 15
#define BUZZER_PIN 19

#define RIGHT 0
#define UP 1
#define LEFT 2
#define DOWN 3

#define ESP_INTR_FLAG_DEFAULT 0

// Game varible
int score = 0;
int highScore;
int gamespeed;
int isGameOver = false;
int i;
int dir;
SSD1306_t dev;
uint8_t buffer[128 * 64 / 8];

typedef struct FOOD
{
	int x;
	int y;
} FOOD;

typedef struct SNAKE
{
	int x[200];
	int y[200];
	int node;
	int dir;
} SNAKE;

FOOD food;
SNAKE snake;

uint8_t SnakeHead[] = {
	0b11111111,
	0b10011001,
	0b10011001,
	0b11111111,
	0b11111111,
	0b11111111,
	0b11111111,
	0b11111111
};

uint8_t SnakeTail[] = {
	0b00000000,
	0b00000000,
	0b00001000,
	0b00001000,
	0b00011000,
	0b00111100,
	0b00111110,
	0b01111111
};

uint8_t SnakeBody[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

uint8_t foodEle[] = {
	0b00001111,
	0b00110001,
	0b01001100,
	0b10000010,
	0b10000001,
	0b10000001,
	0b01000010,
	0b00111100,
};

void foodElement(int x, int y, uint8_t *buffer)
{
	for (int i = 0; i < 8; i++)
	{
		for (int j = 0; j < 8; j++)
		{
			if (foodEle[j] & (1 << i))
			{
				int buffer_index = (x + (7 - i)) + ((y + j) / 8) * 128;
				buffer[buffer_index] |= (1 << ((y + j) % 8));
			}
		}
	}
}

void updateBuffer(uint8_t *buffer, int x, int y, uint8_t *charBitmap)
{
	for (int i = 0; i < 8; i++)
	{
		for (int j = 0; j < 8; j++)
		{
			if (charBitmap[j] & (1 << i))
			{
				int buffer_index = (x + j) + ((y + i) / 8) * 128;
				buffer[buffer_index] |= (1 << ((y + i) % 8));
			}
		}
	}
}

void updateHead(uint8_t *buffer, int x, int y, uint8_t *charBitmap, int dir)
{
	uint8_t rotatedBitmap[8] = {0};

	// Rotate bitmap based on direction
	switch (dir)
	{
	case LEFT:
		// No rotation needed for RIGHT
		for (int i = 0; i < 8; i++)
		{
			rotatedBitmap[i] = charBitmap[i];
		}
		break;
	case DOWN:
		// Rotate 90 degrees clockwise
		for (int i = 0; i < 8; i++)
		{
			for (int j = 0; j < 8; j++)
			{
				if (charBitmap[j] & (1 << i))
				{
					rotatedBitmap[i] |= (1 << (7 - j));
				}
			}
		}
		break;
	case RIGHT:
		// Rotate 180 degrees
		for (int i = 0; i < 8; i++)
		{
			for (int j = 0; j < 8; j++)
			{
				if (charBitmap[i] & (1 << j))
				{
					rotatedBitmap[7 - i] |= (1 << (7 - j));
				}
			}
		}
		break;
	case UP:
		// Rotate 90 degrees counter-clockwise
		for (int i = 0; i < 8; i++)
		{
			for (int j = 0; j < 8; j++)
			{
				if (charBitmap[j] & (1 << i))
				{
					rotatedBitmap[7 - i] |= (1 << j);
				}
			}
		}
		break;
	}

	for (int i = 0; i < 8; i++)
	{
		for (int j = 0; j < 8; j++)
		{
			int buffer_index = (x + j) + ((y + i) / 8) * 128;

			if (rotatedBitmap[j] & (1 << i))
				buffer[buffer_index] |= (1 << ((y + i) % 8));
			else
				buffer[buffer_index] &= ~(1 << ((y + i) % 8));
		}
	}
}

void updateTail(uint8_t *buffer, int x, int y, uint8_t *charBitmap){
	int d = RIGHT;
	int width = 128;
int height = 56;

// Determine the direction of the tail
if (snake.y[snake.node - 1] == snake.y[snake.node - 2]) {
    // Tail is in the same row as the second last node
    if ((snake.x[snake.node - 1] < snake.x[snake.node - 2] && 
         snake.x[snake.node - 2] - snake.x[snake.node - 1] < width / 2) ||
        (snake.x[snake.node - 1] > snake.x[snake.node - 2] && 
         snake.x[snake.node - 1] - snake.x[snake.node - 2] > width / 2)) {
        d = RIGHT;
    } else {
        d = LEFT;
    }
} else {
    // Tail is in the same column as the second last node
    if ((snake.y[snake.node - 1] > snake.y[snake.node - 2] && 
         snake.y[snake.node - 1] - snake.y[snake.node - 2] < height / 2) ||
        (snake.y[snake.node - 1] < snake.y[snake.node - 2] && 
         snake.y[snake.node - 2] - snake.y[snake.node - 1] > height / 2)) {
        d = UP;
    } else {
        d = DOWN;
    }
}
	uint8_t rotatedBitmap[8] = {0};

	// Rotate bitmap based on direction
	switch (d)
	{
	case RIGHT:
		// No rotation needed for RIGHT
		for (int i = 0; i < 8; i++)
			rotatedBitmap[i] = charBitmap[i];
		break;
	case UP:
		// Rotate 90 degrees clockwise
		for (int i = 0; i < 8; i++)
			for (int j = 0; j < 8; j++)
				if (charBitmap[j] & (1 << i))
					rotatedBitmap[i] |= (1 << (7 - j));
		break;
	case LEFT:
		// Rotate 180 degrees
		for (int i = 0; i < 8; i++)
			for (int j = 0; j < 8; j++)
				if (charBitmap[i] & (1 << j))
					rotatedBitmap[7 - i] |= (1 << (7 - j));
		break;
	case DOWN:
		// Rotate 90 degrees counter-clockwise
		for (int i = 0; i < 8; i++)
			for (int j = 0; j < 8; j++)
				if (charBitmap[j] & (1 << i))
					rotatedBitmap[7 - i] |= (1 << j);
		break;
	}

	for (int i = 0; i < 8; i++)
	{
		for (int j = 0; j < 8; j++)
		{
			int buffer_index = (x + j) + ((y + i) / 8) * 128;

			if (rotatedBitmap[j] & (1 << i))
				buffer[buffer_index] |= (1 << ((y + i) % 8));
			else
				buffer[buffer_index] &= ~(1 << ((y + i) % 8));
		}
	}
}

void drawCharToBuffer(uint8_t *buffer, int x, int y, char c)
{
	uint8_t *charBitmap;
	if (c >= 'A' && c <= 'Z') // Uppercase letters
		charBitmap = font8x8_basic_tr[c - 'A' + 65];
	else if (c >= 'a' && c <= 'z') // Lowercase letters
		charBitmap = font8x8_basic_tr[c - 'a' + 97];
	else if (c >= '0' && c <= '9') // Digits
		charBitmap = font8x8_basic_tr[c - '0' + 48];
	else if (c == ':')
		charBitmap = font8x8_basic_tr[58];
	else if (c == ' ')
		charBitmap = font8x8_basic_tr[32]; // Space
	else
		return; // Character not supported

	updateBuffer(buffer, x, y, charBitmap);
}

void drawStringToBuffer(uint8_t *buffer, int x, int y, const char *str)
{
	while (*str)
	{
		drawCharToBuffer(buffer, x, y, *str);
		x += 8; // Move to the next character position
		str++;
	}
}

void button_init(gpio_num_t pin, gpio_isr_t isr_handler, void *isr_arg)
{
	gpio_config_t io_conf = {};
	io_conf.intr_type = GPIO_INTR_NEGEDGE;
	io_conf.pin_bit_mask = (1ULL << pin);
	io_conf.mode = GPIO_MODE_INPUT;
	io_conf.pull_up_en = 1;
	io_conf.pull_down_en = 0;
	gpio_config(&io_conf);

	gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
	gpio_isr_handler_add(pin, isr_handler, isr_arg);
}

static void IRAM_ATTR buttonUpISR(void *arg)
{
	dir = UP;
}

static void IRAM_ATTR buttonDownISR(void *arg)
{
	dir = DOWN;
}

static void IRAM_ATTR buttonLeftISR(void *arg)
{
	dir = LEFT;
}

static void IRAM_ATTR buttonRightISR(void *arg)
{
	dir = RIGHT;
}

static void IRAM_ATTR buttonResetISR(void *arg)
{
	isGameOver = false;
}

int isFoodOnSnake()
{
	for (int i = 0; i < snake.node; i++)
		if (food.x == snake.x[i] && food.y == snake.y[i])
			return 1;
	return 0;
}

void generateFood()
{
	do
	{
		food.x = (rand() % 16) * 8;
		food.y = ((rand() % 7) + 1) * 8;
	} while (isFoodOnSnake());
}

void snakeGame()
{
	switch (snake.dir)
	{
	case RIGHT:
		snake.x[0] += 8;
		if (snake.x[0] > 120)
			snake.x[0] = 0;
		break;
	case UP:
		snake.y[0] -= 8;
		if (snake.y[0] < 8)
			snake.y[0] = 56;
		break;
	case LEFT:
		snake.x[0] -= 8;
		if (snake.x[0] < 0)
			snake.x[0] = 120;
		break;
	case DOWN:
		snake.y[0] += 8;
		if (snake.y[0] > 56)
			snake.y[0] = 8;
		break;
	}

	// Checks for snake's collision with the food
	if ((snake.x[0] == food.x) && (snake.y[0] == food.y))
	{
		snake.node++;
		score += 1;
		gamespeed = 20 - ((score / 5) * 2);
		ESP_LOGI(TAG, "Game speed %d", gamespeed);
		play_tone(1000, 100);
		generateFood();
		ESP_LOGI(TAG, "%d\n", snake.node);
	}

	// Checks for collision with the body
	for (int i = 1; i < snake.node - 1; i++)
		if (snake.x[0] == snake.x[i] && snake.y[0] == snake.y[i])
			isGameOver = true;

	// update snake body
	for (i = snake.node - 1; i > 0; i--)
	{
		snake.x[i] = snake.x[i - 1];
		snake.y[i] = snake.y[i - 1];
	}

	ESP_LOGI(TAG, "len: %d ", snake.node);
	for (int i = 0; i < snake.node; i++){
		ESP_LOGI(TAG, "%d %d %d\n", i, snake.x[i], snake.y[i]);
	}
}

void key()
{
	if (dir == DOWN && snake.dir != UP)
	{
		snake.dir = DOWN;
	}
	if (dir == RIGHT && snake.dir != LEFT)
	{
		snake.dir = RIGHT;
	}
	if (dir == LEFT && snake.dir != RIGHT)
	{
		snake.dir = LEFT;
	}
	if (dir == UP && snake.dir != DOWN)
	{
		snake.dir = UP;
	}
}

void setup()
{
	// init NVS
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
	{
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);
	// Init display
	ESP_LOGI(TAG, "INTERFACE is i2c");
	ESP_LOGI(TAG, "CONFIG_SDA_GPIO=%d", CONFIG_SDA_GPIO);
	ESP_LOGI(TAG, "CONFIG_SCL_GPIO=%d", CONFIG_SCL_GPIO);
	ESP_LOGI(TAG, "CONFIG_RESET_GPIO=%d", CONFIG_RESET_GPIO);
	i2c_master_init(&dev, CONFIG_SDA_GPIO, CONFIG_SCL_GPIO, CONFIG_RESET_GPIO);

	ESP_LOGI(TAG, "Panel is 128x64");
	ssd1306_init(&dev, 128, 64);

	ssd1306_contrast(&dev, 0xff);
	ssd1306_clear_screen(&dev, false);
	ssd1306_display_text(&dev, 4, "   SNAKEGAME", 12, false);
	vTaskDelay(pdMS_TO_TICKS(1000));
	buzzer_init(BUZZER_PIN);

	button_init(BUTTON_UP_PIN, buttonUpISR, NULL);
	button_init(BUTTON_DOWN_PIN, buttonDownISR, NULL);
	button_init(BUTTON_LEFT_PIN, buttonLeftISR, NULL);
	button_init(BUTTON_RIGHT_PIN, buttonRightISR, NULL);
	button_init(BUTTON_RESET_PIN, buttonResetISR, NULL);

	ESP_LOGI(TAG, "ISRs installed");

	// Initialize food
	food.x = 40;
	food.y = 48;
	// Initialize snake
	snake.x[0] = 64;
	snake.y[0] = 32;
	snake.x[1] = 64;
	snake.y[1] = 32;
	snake.dir = RIGHT;
	snake.node = 2;

	gamespeed = 20;
	ESP_LOGI(TAG, "Game speed %d", gamespeed);
	ssd1306_clear_screen(&dev, false);
}

// Function to save high score
void save_high_score(int score)
{
	// Initialize NVS
	esp_err_t err = nvs_flash_init();
	if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
	{
		// NVS partition was truncated and needs to be erased
		// Retry nvs_flash_init
		ESP_ERROR_CHECK(nvs_flash_erase());
		err = nvs_flash_init();
	}
	ESP_ERROR_CHECK(err);

	// Open NVS handle
	nvs_handle_t my_handle;
	err = nvs_open("storage", NVS_READWRITE, &my_handle);
	if (err != ESP_OK)
	{
		ESP_LOGI(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
	}
	else
	{
		int high_score = 0;
		// Read the current high score
		err = nvs_get_i32(my_handle, "high_score", &high_score);
		switch (err)
		{
		case ESP_OK:
			ESP_LOGI(TAG, "Current high score = %d", high_score);
			break;
		case ESP_ERR_NVS_NOT_FOUND:
			ESP_LOGI(TAG, "The value is not initialized yet!");
			break;
		default:
			ESP_LOGI(TAG, "Error (%s) reading!", esp_err_to_name(err));
		}

		// Check if the new score is higher and update if it is
		if (score > high_score)
		{
			ESP_LOGI(TAG, "New high score = %d", score);
			err = nvs_set_i32(my_handle, "high_score", score);
			if (err != ESP_OK)
			{
				ESP_LOGI(TAG, "Failed to write new high score!");
			}

			// Commit written value.
			err = nvs_commit(my_handle);
			if (err != ESP_OK)
			{
				ESP_LOGI(TAG, "Failed to commit new high score!");
			}
		}
		else
		{
			ESP_LOGI(TAG, "High score remains = %d", high_score);
		}

		// Close NVS handle
		nvs_close(my_handle);
	}
}

// Function to get the high score
int get_high_score()
{
	// Initialize NVS
	esp_err_t err = nvs_flash_init();
	if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
	{
		// NVS partition was truncated and needs to be erased
		// Retry nvs_flash_init
		ESP_ERROR_CHECK(nvs_flash_erase());
		err = nvs_flash_init();
	}
	ESP_ERROR_CHECK(err);

	// Open NVS handle
	nvs_handle_t my_handle;
	err = nvs_open("storage", NVS_READWRITE, &my_handle);
	int high_score = 0;
	if (err != ESP_OK)
	{
		ESP_LOGI(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
	}
	else
	{
		// Read the current high score
		err = nvs_get_i32(my_handle, "high_score", &high_score);
		switch (err)
		{
		case ESP_OK:
			ESP_LOGI(TAG, "Current high score = %d", high_score);
			break;
		case ESP_ERR_NVS_NOT_FOUND:
			ESP_LOGI(TAG, "The value is not initialized yet!");
			high_score = 0;
			break;
		default:
			ESP_LOGI(TAG, "Error (%s) reading!", esp_err_to_name(err));
		}

		// Close NVS handle
		nvs_close(my_handle);
	}
	return high_score;
}

void resetGame()
{
	// Update high score
	if (score > highScore)
	{
		highScore = score;
		save_high_score(score);
	}
	// Reset score and game variables
	score = 0;
	dir = RIGHT;

	// Reset snake position and length
	snake.x[0] = 64;
	snake.y[0] = 32;
	snake.x[1] = 64;
	snake.y[1] = 32;
	snake.dir = RIGHT;
	snake.node = 2;

	// Reset game speed
	gamespeed = 20;

	// Generate new food
	generateFood();
}

void gameOver()
{
	highScore = get_high_score();
	ssd1306_display_text(&dev, 2, "    GAMEOVER", 12, false);

	char text[17]; // Buffer size to accommodate 16 characters plus null terminator

	sprintf(text, "Score: %d", score);
	ssd1306_display_text(&dev, 3, text, strlen(text), false);

	sprintf(text, "High score: %d", highScore);
	ssd1306_display_text(&dev, 4, text, strlen(text), false);
}

void draw()
{
	memset(buffer, 0, sizeof(buffer)); // Clear buffer
	for (i = 0; i < snake.node; i++)
		updateBuffer(buffer, snake.x[i], snake.y[i], SnakeBody);
	updateHead(buffer, snake.x[0], snake.y[0], SnakeHead, dir);
	if(snake.node > 2)
		updateTail(buffer, snake.x[snake.node - 1], snake.y[snake.node - 1], SnakeTail);
	foodElement(food.x, food.y, buffer);

	char text[17];
	sprintf(text, "Score: %d", score);
	drawStringToBuffer(buffer, 0, 0, text);

	ssd1306_set_buffer(&dev, buffer);
	_ssd1306_line(&dev, 0, 8, 127, 8, false);
	_ssd1306_line(&dev, 0, 8, 0, 63, false);
	_ssd1306_line(&dev, 127, 8, 127, 63, false);
	_ssd1306_line(&dev, 0, 63, 127, 63, false);

	ssd1306_show_buffer(&dev);
}

void app_main(void)
{
	// Set logging level for the TAG to INFO
	esp_log_level_set(TAG, ESP_LOG_INFO);
	setup();
	while (true)
	{
		while (!isGameOver)
		{
			draw();
			key();
			snakeGame();
			vTaskDelay(gamespeed);
		}

		ssd1306_clear_screen(&dev, false);
		gameOver();
		play_melody_alt(endSound, endSoundD, 8);
		while (isGameOver)
			gameOver();
		resetGame();
		ESP_LOGI(TAG, "Game Over triggered. Resetting game...");
	}
}
