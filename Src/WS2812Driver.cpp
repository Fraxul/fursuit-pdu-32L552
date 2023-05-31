#include <stdint.h>
#include <string.h>
#include <limits.h>
#include "main.h"
#include "WS2812Driver.h"
#include "EyeRenderer.h"
#include "PowerManagement.h"
#include "adc.h"
#include "eye_layout.h"
#include "FreeRTOS.h"
#include "task.h"

using namespace glm;

// ===================================================================================
// LED driver code originally from https://github.com/simap/pixelblaze_output_expander
// (Copyright (c) 2018 Ben Hencke, released under The MIT License)
// ===================================================================================

//tim2 has 3 periods w/ dma triggers to set start bits, data bits from buffer, and clear bits
//tim3 gates tim2 and period should be long enough for data to xfer
//tim3 prescaler matches tim2 period, then number of bytes transfered matches count so they end at the same time

#define BYTES_PER_PIXEL 3
#define BYTES_PER_CHANNEL (channelMaxLength * BYTES_PER_PIXEL)
#define BYTES_TOTAL (BYTES_PER_CHANNEL * 8)

uint32_t bitBufferA[BYTES_PER_CHANNEL * 2];
uint32_t bitBufferB[BYTES_PER_CHANNEL * 2];
volatile bool bufferSwapState = false;
volatile bool pendingFrameReady = false;

static inline uint32_t* drawBuffer() { return bufferSwapState ? bitBufferA : bitBufferB; }
static inline uint32_t* readBuffer() { return bufferSwapState ? bitBufferB : bitBufferA; }
static inline void swapBuffers() { bufferSwapState = !bufferSwapState; }
static inline void clearBuffers() {
	// Buffers get cleared to 0xff for all-black -- bits are inverted as they are sent to the display
	memset(bitBufferA, 0xff, BYTES_PER_CHANNEL * 2 * sizeof(uint32_t));
	memset(bitBufferB, 0xff, BYTES_PER_CHANNEL * 2 * sizeof(uint32_t));
}

// How many bits are being sent -- (pixel count * 3 * 8), usually.
static constexpr inline uint16_t ws2812TransmissionLengthBits() {
	return static_cast<uint16_t>(channelMaxLength) * (BYTES_PER_PIXEL * 8);
}
// DMA sources for start and stop bits, fed into the GPIO BSRR port register.
// [0] is the start bits, which sets the appropriate BS{0...15} flags.
// [1] is the stop bits, which sets the the BR{0...15} flags to match the start bits: effectively (start bits) << 16
volatile uint32_t ws2812StartStopBits[2];

volatile uint32_t frameCounter = 0;
extern "C" uint32_t getFrameCounter() { return frameCounter; }

static void drawPendingFrame() {
	vPortEnterCritical();

	swapBuffers();
	pendingFrameReady = false;

	frameCounter += 1;

	// Reset timers
	LL_TIM_DisableCounter(TIM2);
	LL_TIM_DisableCounter(TIM3);


	uint32_t zero = 0;
	//OK this is a bit weird, there's some kind of pending DMA request that will transfer immediately and shift bits by one
	//set it up to write a zero, then set it up again
	LL_DMA_DisableChannel(DMA2, LL_DMA_CHANNEL_2);
	LL_DMA_DisableIT_TC(DMA2, LL_DMA_CHANNEL_2);
  LL_DMA_SetMemoryAddress(DMA2, LL_DMA_CHANNEL_2, (uint32_t)&zero);
	LL_DMA_SetDataLength(DMA2, LL_DMA_CHANNEL_2, 10);
	LL_DMA_EnableChannel(DMA2, LL_DMA_CHANNEL_2);

	//turn it off, set up dma to transfer from bitBuffer
	LL_DMA_DisableChannel(DMA2, LL_DMA_CHANNEL_2);
	LL_DMA_SetMemoryAddress(DMA2, LL_DMA_CHANNEL_2, (uint32_t)readBuffer());
	LL_DMA_SetDataLength(DMA2, LL_DMA_CHANNEL_2, ws2812TransmissionLengthBits());
	LL_DMA_ClearFlag_TC2(DMA2);

	LL_DMA_EnableIT_TC(DMA2, LL_DMA_CHANNEL_2);
	LL_DMA_EnableChannel(DMA2, LL_DMA_CHANNEL_2);


	LL_TIM_SetCounter(TIM2, 0);
	LL_TIM_SetCounter(TIM3, 0);
	LL_TIM_GenerateEvent_UPDATE(TIM3);
	LL_TIM_ClearFlag_UPDATE(TIM3);

	LL_TIM_EnableCounter(TIM2);
	LL_TIM_EnableCounter(TIM3);

	vPortExitCritical();
}

// TIM2_CH3 (bit content transfer) IRQ handler
extern "C" {
	extern TaskHandle_t WS2812Handle;
	void WS2812_drawingComplete() {
		BaseType_t xHigherPriorityTaskWoken = pdFALSE;
		uint32_t previousNotificationValue;
		xTaskGenericNotifyFromISR(WS2812Handle, /*value=*/ 0, eSetValueWithOverwrite, &previousNotificationValue, &xHigherPriorityTaskWoken);
		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	}
}


glm::u8vec3 HsvToRgb(glm::u8vec3 hsv) {
    glm::u8vec3 rgb;
    uint8_t region, p, q, t;

    if (hsv.y == 0) { // zero saturation
        rgb.r = hsv.y;
        rgb.g = hsv.y;
        rgb.b = hsv.y;
        return rgb;
    }

    // converting to 16 bit to prevent overflow
    uint16_t h = hsv.x;
    uint16_t s = hsv.y;
    uint16_t v = hsv.z;

    region = h / 43;
    uint16_t remainder = (h - (region * 43)) * 6;

    p = (v * (255u - s)) >> 8;
    q = (v * (255u - ((s * remainder) >> 8))) >> 8;
    t = (v * (255u - ((s * (255u - remainder)) >> 8))) >> 8;

    switch (region)
    {
        case 0:
            rgb.r = v;
            rgb.g = t;
            rgb.b = p;
            break;
        case 1:
            rgb.r = q;
            rgb.g = v;
            rgb.b = p;
            break;
        case 2:
            rgb.r = p;
            rgb.g = v;
            rgb.b = t;
            break;
        case 3:
            rgb.r = p;
            rgb.g = q;
            rgb.b = v;
            break;
        case 4:
            rgb.r = t;
            rgb.g = p;
            rgb.b = v;
            break;
        default:
            rgb.r = v;
            rgb.g = p;
            rgb.b = q;
            break;
    }

    return rgb;
}


// from "Hacker's Delight" (https://books.google.com/books?id=iBNKMspIlqEC&lpg=SL20-PA11)
// Figure 7-2, "Transposing an 8x8-bit matrix." Page 111
static inline void transpose8(uint32_t* input, uint32_t* output) {
	// Load the array and pack it into x and y.
	uint32_t x = input[0];
	uint32_t y = input[1];

	uint32_t t;
	t = (x ^ (x >> 7)) & 0x00AA00AA;  x = x ^ t ^ (t << 7);
	t = (y ^ (y >> 7)) & 0x00AA00AA;  y = y ^ t ^ (t << 7);

	t = (x ^ (x >>14)) & 0x0000CCCC;  x = x ^ t ^ (t <<14);
	t = (y ^ (y >>14)) & 0x0000CCCC;  y = y ^ t ^ (t <<14);

	t = (x & 0xF0F0F0F0) | ((y >> 4) & 0x0F0F0F0F);
	y = ((x << 4) & 0xF0F0F0F0) | (y & 0x0F0F0F0F);
	x = t;

	//output[0] = x;
	//output[1] = y;

	// fix bit-ordering and invert for feeding to BRR
	output[0] = ~__REV(x);
	output[1] = ~__REV(y);
}

class HSVTestPattern : WS2812Pattern {
public:
	virtual uint32_t initFrame() {
		uint32_t tick = HAL_GetTick();
		u8vec3 hsv((tick >> 4) & 0xff, 0xcc, 0xa);
		m_rgb = HsvToRgb(hsv);
		return 0;
	}

	virtual u8vec3 shadePixel(uint8_t channel, uint16_t pixelIdx) {
		return m_rgb;
	}

protected:
	u8vec3 m_rgb;
};


class LumaTestPattern : WS2812Pattern {
public:
	virtual uint32_t initFrame() {
		uint32_t tick = HAL_GetTick();
		uint32_t y = (tick & 1023) >> 1; // 0-511
		if (y >= 256) y = 511 - y;

		m_rgb.r = m_rgb.g = m_rgb.b = (y >> 1); // limit to 50% brightness
		return 0;
	}

	virtual u8vec3 shadePixel(uint8_t channel, uint16_t pixelIdx) {
		return m_rgb;
	}

protected:
	u8vec3 m_rgb;
};

class ChannelIndexTestPattern : WS2812Pattern {
public:
	virtual uint32_t initFrame() {
		return 0;
	}

	virtual u8vec3 shadePixel(uint8_t channel, uint16_t pixelIdx) {
		u8vec3 res;
		res.r = (channel & 4) ? 20 : 0;
		res.g = (channel & 2) ? 20 : 0;
		res.b = (channel & 1) ? 20 : 0;
		return res;
	}
};

class PixelIndexTestPattern : WS2812Pattern {
public:
	virtual uint32_t initFrame() {
		return 0;
	}

	virtual u8vec3 shadePixel(uint8_t channel, uint16_t pixelIdx) {
		u8vec3 res;
		res.r = (pixelIdx & 4) ? 20 : 0;
		res.g = (pixelIdx & 2) ? 20 : 0;
		res.b = (pixelIdx & 1) ? 20 : 0;
		return res;
	}
};


uint32_t Disp5V_charge_time_ms = 0;
extern "C" __attribute__((optimize("-Os"))) void Task_WS2812() {
	EyeRenderer pattern;
	//HSVTestPattern pattern;
	//ChannelIndexTestPattern pattern;
	//PixelIndexTestPattern pattern;
	//LumaTestPattern pattern;


	// Configuration:
	// TIM2 CH2/3/4 generates the waveform (using DMA2 ch. 1-3), TIM3 CH1 drives TIM2
	// Output is PB0 - PB7

	// Stop the DMA timers while debugging
	LL_DBGMCU_APB1_GRP1_FreezePeriph(LL_DBGMCU_APB1_GRP1_TIM2_STOP | LL_DBGMCU_APB1_GRP1_TIM3_STOP);

	// Ensure that pins are set low when display powers up
	LL_GPIO_ResetOutputPin(GPIOB, LL_GPIO_PIN_0 | LL_GPIO_PIN_1 | LL_GPIO_PIN_2 | LL_GPIO_PIN_3 | LL_GPIO_PIN_4 | LL_GPIO_PIN_5 | LL_GPIO_PIN_6 | LL_GPIO_PIN_7);

	// Make sure that the timers are disabled
	LL_TIM_DisableCounter(TIM2);
	LL_TIM_DisableCounter(TIM3);

	// Turn off power to display and wait for the Disp5V supply to drain
	LL_GPIO_SetOutputPin(nLEDPwrEnable_GPIO_Port, nLEDPwrEnable_Pin); // Power off
	{
		while (true) {
			uint32_t mv = ADC_read_Disp5V_Sense();
			if (mv < 250)
					break; // power is off.
			vTaskDelay(pdMS_TO_TICKS(4));
		}
	}

	// Settle time post poweroff
	vTaskDelay(pdMS_TO_TICKS(1000));

	// Wait for the display power rail to settle.
	// The microcontroller will boot up before the 5V supply is enabled.
	// We don't want to turn on the display power before the 5V supply has stabilized,
	// because the inrush limiter won't work correctly.
	{
		uint32_t pg_counter = 0;
		while (true) {
			uint32_t mv = ADC_read_5V_Sense();
			if (mv > 4750) {
				pg_counter += 1;
				if (pg_counter > 25)
					break; // power has been good long enough
			} else {
				// require continuous power-good samples
				pg_counter = 0;
			}

			vTaskDelay(pdMS_TO_TICKS(4));
		}
	}

	// Turn on the display power.
	{
		uint32_t powerOn_start_time = HAL_GetTick();
		LL_GPIO_ResetOutputPin(nLEDPwrEnable_GPIO_Port, nLEDPwrEnable_Pin); // Power on
		uint32_t powerGood_time = powerOn_start_time;
		uint32_t pg_counter = 0;
		while (true) {
			uint32_t mv = ADC_read_Disp5V_Sense();

			if (mv > 4750) {
				if ((++pg_counter) > 25)
					break; // stable
			} else {
				pg_counter = 0;
				powerGood_time = HAL_GetTick(); // push the timer forward as long as the power is not good
			}
			vTaskDelay(pdMS_TO_TICKS(4));
  	}
		Disp5V_charge_time_ms = powerGood_time - powerOn_start_time;
	}

	clearBuffers();

	vTaskDelay(pdMS_TO_TICKS(100)); // Give the display some extra time to stabilize

	// ws2812StartBits can be updated to mask off channels. by default, all channels are active.
	ws2812StartStopBits[0] = 0xff; // BSRR 'set' bits
	ws2812StartStopBits[1] = (ws2812StartStopBits[0]) << 16; // BSRR 'reset' bits

	//set up the 3 stages of DMA triggers on TIM2

	// Fires with TIM2_CH2 -- starts the pulse by writing to the 'set' bits in BSRR.
	LL_DMA_SetMemoryAddress(DMA2, LL_DMA_CHANNEL_1, (uint32_t) &ws2812StartStopBits[0]);
	LL_DMA_SetPeriphAddress(DMA2, LL_DMA_CHANNEL_1, (uint32_t) &GPIOB->BSRR);
	LL_DMA_SetMemoryIncMode(DMA2, LL_DMA_CHANNEL_1, LL_DMA_MEMORY_NOINCREMENT);
	LL_DMA_SetPeriphIncMode(DMA2, LL_DMA_CHANNEL_1, LL_DMA_PERIPH_NOINCREMENT);
	LL_DMA_SetMode(DMA2, LL_DMA_CHANNEL_1, LL_DMA_MODE_CIRCULAR);
	LL_DMA_SetDataLength(DMA2, LL_DMA_CHANNEL_1, 1);
	LL_DMA_EnableChannel(DMA2, LL_DMA_CHANNEL_1);

	//fires with TIM2_CH3 - set the data bit
	// This either continues the pulse (for a 1) or turns it into a short pulse (for a 0)
	// We write inverted bits to BRR to avoid clobbering any other output data on the GPIO port.
	LL_DMA_SetMemoryAddress(DMA2, LL_DMA_CHANNEL_2, (uint32_t) readBuffer());
	LL_DMA_SetPeriphAddress(DMA2, LL_DMA_CHANNEL_2, (uint32_t) &GPIOB->BRR);
	LL_DMA_SetMemoryIncMode(DMA2, LL_DMA_CHANNEL_2, LL_DMA_MEMORY_INCREMENT);
	LL_DMA_SetPeriphIncMode(DMA2, LL_DMA_CHANNEL_2, LL_DMA_PERIPH_NOINCREMENT);
	LL_DMA_SetMode(DMA2, LL_DMA_CHANNEL_2, LL_DMA_MODE_CIRCULAR);
	LL_DMA_SetDataLength(DMA2, LL_DMA_CHANNEL_2, ws2812TransmissionLengthBits());
	LL_DMA_EnableIT_TC(DMA2, LL_DMA_CHANNEL_2);
	LL_DMA_EnableChannel(DMA2, LL_DMA_CHANNEL_2);

	// Fires with TIM2_CH4 -- clears the pulse with the 'reset' bits in BSRR.
	LL_DMA_SetMemoryAddress(DMA2, LL_DMA_CHANNEL_3, (uint32_t) &ws2812StartStopBits[1]);
	LL_DMA_SetPeriphAddress(DMA2, LL_DMA_CHANNEL_3, (uint32_t)&GPIOB->BSRR);
	LL_DMA_SetMemoryIncMode(DMA2, LL_DMA_CHANNEL_3, LL_DMA_MEMORY_NOINCREMENT);
	LL_DMA_SetPeriphIncMode(DMA2, LL_DMA_CHANNEL_3, LL_DMA_PERIPH_NOINCREMENT);
	LL_DMA_SetMode(DMA2, LL_DMA_CHANNEL_3, LL_DMA_MODE_CIRCULAR);
	LL_DMA_SetDataLength(DMA2, LL_DMA_CHANNEL_3, 1);
	LL_DMA_EnableChannel(DMA2, LL_DMA_CHANNEL_3);


	//enable the dma xfers on capture compare events
	LL_TIM_EnableDMAReq_CC2(TIM2);
	LL_TIM_EnableDMAReq_CC3(TIM2);
	LL_TIM_EnableDMAReq_CC4(TIM2);

	LL_TIM_EnableMasterSlaveMode(TIM3);

	// TIM3 CCR1 delays by 300 bit-times before starting DMA to generate the WS2812 reset signal.
	// tim3's prescaler matches tim2's cycle so each increment of tim3 is one bit-time
	LL_TIM_SetAutoReload(TIM3, ws2812TransmissionLengthBits() + 300);
	LL_TIM_OC_SetCompareCH1(TIM3, 300);


	// Scanout a couple of black frames at the start with longer delays between them
	// This seems to help with initialization.
	for (int i = 0; i < 20; ++i) {
		drawPendingFrame();
		vTaskDelay(pdMS_TO_TICKS(5));
	}


	while (true) {
		// Wait for previous scanout to finish
		uint32_t ulInterruptStatus;
		// already have a frame queued up and we're still in scanout -- nothing to do

		xTaskNotifyWait(
														0x00,							 /* Don't clear any bits on entry. */
														ULONG_MAX,					 /* Clear all bits on exit. */
														&ulInterruptStatus, /* Receives the notification value. */
														portMAX_DELAY);		 /* Block indefinitely. */


		if (systemPowerState.poweroffRequested) {
			// Clear the screen, then turn off the display power.
			clearBuffers();
			drawPendingFrame();
			vTaskDelay(pdMS_TO_TICKS(5));

			LL_GPIO_SetOutputPin(nLEDPwrEnable_GPIO_Port, nLEDPwrEnable_Pin); // Power off
			vTaskSuspend(nullptr);
		}

		// Submit the current frame. (swaps buffers, so we're ready to draw again)
		drawPendingFrame();

		while (true) {
			uint32_t frameDelayTime = pattern.initFrame();
			if (!frameDelayTime)
				break;
			vTaskDelay(frameDelayTime);
		}

		// Generate a pixel at a time across every channel so that we can write entire blocks of bit-data at once.
		uint32_t* dst = drawBuffer();

		for (uint8_t pix = 0; pix < channelMaxLength; ++pix) {
			u8vec3 channelPix[8];
			for (uint8_t channelIdx = 0; channelIdx < 8; ++channelIdx) {

				if (pix >= channelLengths[channelIdx]) {
					// No pixel on this channel at this index -- just pad with 0
					channelPix[channelIdx] = u8vec3(0);
					continue;
				}

				channelPix[channelIdx] = pattern.shadePixel(channelIdx, pix);
			}
#if 1
				uint32_t v[2];
				v[0] =
					channelPix[7].g << 24 |
					channelPix[6].g << 16 |
					channelPix[5].g << 8 |
					channelPix[4].g;
				v[1] =
					channelPix[3].g << 24 |
					channelPix[2].g << 16 |
					channelPix[1].g << 8 |
					channelPix[0].g;
				transpose8(v, dst);
				dst += 2;

				v[0] =
					channelPix[7].r << 24 |
					channelPix[6].r << 16 |
					channelPix[5].r << 8 |
					channelPix[4].r;
				v[1] =
					channelPix[3].r << 24 |
					channelPix[2].r << 16 |
					channelPix[1].r << 8 |
					channelPix[0].r;
				transpose8(v, dst);
				dst += 2;

				v[0] =
					channelPix[7].b << 24 |
					channelPix[6].b << 16 |
					channelPix[5].b << 8 |
					channelPix[4].b;
				v[1] =
					channelPix[3].b << 24 |
					channelPix[2].b << 16 |
					channelPix[1].b << 8 |
					channelPix[0].b;
				transpose8(v, dst);
				dst += 2;
#else
				dst[0] = 0;
				dst[1] = 0;
				dst += 2;
				dst[0] = 0;
				dst[1] = 0;
				dst += 2;
#endif
		}
	} // infinite loop
}
