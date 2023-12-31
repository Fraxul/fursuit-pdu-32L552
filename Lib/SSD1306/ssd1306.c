/**
 2020 by Stefan Wagner
 Project Files (Github): https://github.com/wagiminator/ATtiny13-TinyOLEDdemo
 License: http://creativecommons.org/licenses/by-sa/3.0/

 Adapted for STM32 and implemented font magnification
 by Alfredo Cortellini May 2021
 Adaptation project: https://github.com/alcor6502/STM32F4-Driver-SSD1306/
**/

#include "ssd1306.h"
#include <FreeRTOS.h>
#include <task.h>
#include "log.h"

// Standard ASCII 5x8 font (adapted from Neven Boyanov and Stephen Denne)
const uint8_t font_5x8[] = {
		0x00, 0x00, 0x00, 0x00, 0x00, //   0
		0x00, 0x00, 0x2f, 0x00, 0x00, // ! 1
		0x00, 0x07, 0x00, 0x07, 0x00, // " 2
		0x14, 0x7f, 0x14, 0x7f, 0x14, // # 3
		0x24, 0x2a, 0x7f, 0x2a, 0x12, // $ 4
		0x62, 0x64, 0x08, 0x13, 0x23, // % 5
		0x36, 0x49, 0x55, 0x22, 0x50, // & 6
		0x00, 0x05, 0x03, 0x00, 0x00, // ' 7
		0x00, 0x1c, 0x22, 0x41, 0x00, // ( 8
		0x00, 0x41, 0x22, 0x1c, 0x00, // ) 9
		0x14, 0x08, 0x3E, 0x08, 0x14, // * 10
		0x08, 0x08, 0x3E, 0x08, 0x08, // + 11
		0x00, 0x00, 0xA0, 0x60, 0x00, // , 12
		0x08, 0x08, 0x08, 0x08, 0x08, // - 13
		0x00, 0x60, 0x60, 0x00, 0x00, // . 14
		0x20, 0x10, 0x08, 0x04, 0x02, // / 15
		0x3E, 0x51, 0x49, 0x45, 0x3E, // 0 16
		0x00, 0x42, 0x7F, 0x40, 0x00, // 1 17
		0x42, 0x61, 0x51, 0x49, 0x46, // 2 18
		0x21, 0x41, 0x45, 0x4B, 0x31, // 3 19
		0x18, 0x14, 0x12, 0x7F, 0x10, // 4 20
		0x27, 0x45, 0x45, 0x45, 0x39, // 5 21
		0x3C, 0x4A, 0x49, 0x49, 0x30, // 6 22
		0x01, 0x71, 0x09, 0x05, 0x03, // 7 23
		0x36, 0x49, 0x49, 0x49, 0x36, // 8 24
		0x06, 0x49, 0x49, 0x29, 0x1E, // 9 25
		0x00, 0x36, 0x36, 0x00, 0x00, // : 26
		0x00, 0x56, 0x36, 0x00, 0x00, // ; 27
		0x08, 0x14, 0x22, 0x41, 0x00, // < 28
		0x14, 0x14, 0x14, 0x14, 0x14, // = 29
		0x00, 0x41, 0x22, 0x14, 0x08, // > 30
		0x02, 0x01, 0x51, 0x09, 0x06, // ? 31
		0x32, 0x49, 0x59, 0x51, 0x3E, // @ 32
		0x7C, 0x12, 0x11, 0x12, 0x7C, // A 33
		0x7F, 0x49, 0x49, 0x49, 0x36, // B 34
		0x3E, 0x41, 0x41, 0x41, 0x22, // C 35
		0x7F, 0x41, 0x41, 0x22, 0x1C, // D 36
		0x7F, 0x49, 0x49, 0x49, 0x41, // E 37
		0x7F, 0x09, 0x09, 0x09, 0x01, // F 38
		0x3E, 0x41, 0x49, 0x49, 0x7A, // G 39
		0x7F, 0x08, 0x08, 0x08, 0x7F, // H 40
		0x00, 0x41, 0x7F, 0x41, 0x00, // I 41
		0x20, 0x40, 0x41, 0x3F, 0x01, // J 42
		0x7F, 0x08, 0x14, 0x22, 0x41, // K 43
		0x7F, 0x40, 0x40, 0x40, 0x40, // L 44
		0x7F, 0x02, 0x0C, 0x02, 0x7F, // M 45
		0x7F, 0x04, 0x08, 0x10, 0x7F, // N 46
		0x3E, 0x41, 0x41, 0x41, 0x3E, // O 47
		0x7F, 0x09, 0x09, 0x09, 0x06, // P 48
		0x3E, 0x41, 0x51, 0x21, 0x5E, // Q 49
		0x7F, 0x09, 0x19, 0x29, 0x46, // R 50
		0x46, 0x49, 0x49, 0x49, 0x31, // S 51
		0x01, 0x01, 0x7F, 0x01, 0x01, // T 52
		0x3F, 0x40, 0x40, 0x40, 0x3F, // U 53
		0x1F, 0x20, 0x40, 0x20, 0x1F, // V 54
		0x3F, 0x40, 0x38, 0x40, 0x3F, // W 55
		0x63, 0x14, 0x08, 0x14, 0x63, // X 56
		0x07, 0x08, 0x70, 0x08, 0x07, // Y 57
		0x61, 0x51, 0x49, 0x45, 0x43, // Z 58
		0x00, 0x7F, 0x41, 0x41, 0x00, // [ 59
		0x02, 0x04, 0x08, 0x10, 0x20, // \ 60
		0x00, 0x41, 0x41, 0x7F, 0x00, // ] 61
		0x04, 0x02, 0x01, 0x02, 0x04, // ^ 62
		0x40, 0x40, 0x40, 0x40, 0x40, // _ 63
		0x38, 0x38, 0x38, 0x38, 0x38, //   64 Special Character kind of an underscore
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //   65 Special Character
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //   66 Special Character
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //   67 Special Character
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF  //   68 Special Character
		};

// Cursor variables
static uint8_t col, page;

// I2C variables
static I2C_HandleTypeDef *i2cHandle = NULL;

static const uint8_t ssd1306_initCmd[] = {
		// NOP sled in case the command interface gets into a weird state
		// The longest command is 7 bytes (0x26/0x27 Continuous Horizontal Scroll),
		// so 8 bytes of NOPs should be enough to get out of any command that
		// happens to be active.

		0xE3, 0xE3, 0xE3, 0xE3,
		0xE3, 0xE3, 0xE3, 0xE3,

#if (SSD1306_HEIGHT == 64)
		0xA8, 0x3F,				// Set multiplex (HEIGHT-1): 0x3F for 128x64
		0x22, 0x00, 0x07, // Set min and max page:  0x07 for 128x64
		0xDA, 0x12,				// Set COM pins hardware config to seq: 0x12 for 128x64
#else
		0xA8, 0x1F,							// Set multiplex (HEIGHT-1): 0x1F for 128x32
		0x22, 0x00, 0x03,				// Set min and max page:  0x03 for 128x32
		0xDA, 0x02,							// Set COM pins hardware config to seq: 0x02 for 128x32
#endif
		0x20, 0x00,				// Set horizontal memory addressing mode
		0x21, 0x00, 0x7f, // Set column start and end address to defaults (0-127)
		0x8D, 0x14,				// Enable charge pump
		0x81, 0xFF,				// Set contrast 0x01 = Min contrast, 0xFF = Max contrast
		0xD5, 0xF0,				// Set display clock divide and frequency set clock to max
		0x2E,							// Deactivate scroll
		0x40,							// Display RAM start line = 0
		0xD3, 0x00,				// Set display offset to 0
		0xAF,							// Screen on

		// Flip screen command must be last -- see ssd1306_SendConfig
		0xA1, 0xC8, // Flip the screen (optional -- see ssd1306_SendConfig)
};


static volatile TaskHandle_t ssd1306WriteTask;

//  IC2 Write Function
static void i2cWrite(uint8_t mode, const uint8_t *data, uint8_t lenght){

	ssd1306WriteTask = xTaskGetCurrentTaskHandle();
	ulTaskNotifyValueClear(ssd1306WriteTask, 0xffffffffUL);

	while (1) {
		HAL_StatusTypeDef st = HAL_I2C_Mem_Write_DMA(i2cHandle, SSD1306_I2C_ADDR, mode, I2C_MEMADD_SIZE_8BIT, (uint8_t*) data, lenght);
		if (st == HAL_BUSY) {
			vTaskDelay(1);
			continue;
		}

		if (st != HAL_OK) {
			logprintf("SSD1306 i2cWrite() failed with HAL status %u\n", st);
		}
		break;
	}

	ulTaskNotifyTake(pdFALSE, pdMS_TO_TICKS(SSD1306_I2C_TIMEOUT));
}

void ssd1306_TransactionCompleteCallback(I2C_HandleTypeDef *hi2c) {
	BaseType_t xHigherPriorityTaskWoken = 0;
	vTaskNotifyGiveFromISR(ssd1306WriteTask, &xHigherPriorityTaskWoken);
		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}


//  Initialize the display
void ssd1306_Init(I2C_HandleTypeDef *hi2c) {
	i2cHandle = hi2c;
	i2cHandle->MemTxCpltCallback = ssd1306_TransactionCompleteCallback;
	i2cHandle->ErrorCallback = ssd1306_TransactionCompleteCallback;

	ssd1306_SendConfig();

	ssd1306_ClearScreen();
	ssd1306_SetDisplayOnOff(1);
}

void ssd1306_SendConfig() {
	// Send the initialization string.
	// We skip the last 2 command bytes if the screen is not flipped.
	i2cWrite(SSD1306_I2C_CMD, ssd1306_initCmd, (sizeof(ssd1306_initCmd) - (SSD1306_FLIP_SCREEN ? 0 : 2)));
}


// Clear screen
void ssd1306_ClearScreen(void) {
	uint8_t i;
	const uint8_t blocks = SSD1306_WIDTH * SSD1306_HEIGHT / 8 / 16;
	ssd1306_SetCursor(0, 0);              // Set cursor at upper left corner

	uint8_t i2cBuff[16];
	for (i = 0; i < 16; i++) {
		i2cBuff[i] = 0x00;
	}
	for (i = 0; i < blocks; i++) {
		i2cWrite(SSD1306_I2C_DATA, i2cBuff, 16);
	}
}


// Print a character
void ssd1306_WriteChar(char ch, uint8_t fsize) {
	uint8_t i2cBuff[24];

	uint8_t sliceChar, shifter;
	uint16_t offsetChar; // calculated position of character in font array
	uint32_t temp;

	fsize = (fsize & 0x03) + 1; // Prevent array overflow
	if ((ch < 32) || (ch > 100)) {
		ch = 32; 				// Prevent to search outside of chars array
	}

	for (uint8_t k = 0; k < fsize; k++) {
		offsetChar = (ch - 32) * 5;
		for (sliceChar = 0; sliceChar < (5 * fsize); sliceChar += fsize) {
			temp = 0;

			for (uint8_t j = 0; j < 8; j++) { // This routine is designed to multiply the pixels in vertical based on size
				shifter = j * (fsize - 1); // Set the shift amount to spread the pixels on multiple pages based on size
				for (uint8_t m = 0; m < fsize; m++) {
					temp |= ((font_5x8[offsetChar] & (0x01 << j)) << (shifter + m));
				}
			}
			shifter = 8 * k; // Set the shifting amount to select the right portion of the char
			for (uint8_t m = 0; m < fsize; m++) {
				i2cBuff[sliceChar + m] = (uint8_t) (temp >> shifter);
			}
			offsetChar++;
		}
		for (uint8_t m = 0; m < fsize; m++) {
			i2cBuff[sliceChar++] = 0x00;
		}
		i2cWrite(SSD1306_I2C_DATA, i2cBuff, sliceChar);
		ssd1306_SetCursor(col, ++page);
	}
	ssd1306_SetCursor(col += (fsize * SSD1306_CHAR_WIDTH), page -= fsize);

}

// Print a string
void ssd1306_WriteString(char *msg, uint8_t fsize) {
	char ch = *msg;             	// Read first character from program memory
	while (ch != 0) {				// Repeat until string terminator
		ssd1306_WriteChar(ch, fsize); // Print character
		ch = *(++msg);              // Read next character
	}
}

// Set cursor position - Vertical value is with increments of 8 pixels
void ssd1306_SetCursor(uint8_t xpos, uint8_t ypos) {
	col = xpos & 0x7F;				//Prevent column overflow
#if (SSD1306_HEIGHT == 64)
	page = ypos & 0x07;				//Prevent rows overflow if 8 rows (64 pixel height)
#else
	page = ypos & 0x03;				//Prevent rows overflow if 4 rows (32 pixel height)
#endif
	uint8_t i2cBuff[] = {
		col & 0x0F,					// set low nibble of start column
		0x10 | (col >> 4),	// set high nibble of start column
		0xB0 | page};				// set start page
	i2cWrite(SSD1306_I2C_CMD, i2cBuff, sizeof(i2cBuff));
}


// Set the display contrast
void ssd1306_SetContrast(uint8_t contrast) {
	uint8_t i2cBuff[] = { 0x81, contrast };
	i2cWrite(SSD1306_I2C_CMD, i2cBuff, sizeof(i2cBuff));
}


// Switch On/Off the display
void ssd1306_SetDisplayOnOff(uint8_t onOff) {
	uint8_t i2cBuff[] = { 0xAE + (onOff & 0x01) };
	i2cWrite(SSD1306_I2C_CMD, i2cBuff, sizeof(i2cBuff));
}
