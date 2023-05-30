#pragma once
#include <stdint.h>
#ifdef __cplusplus
#include <glm/ext/vector_uint3_sized.hpp>

#endif


#ifdef __cplusplus
extern "C" {
#endif
    void WS2812_setup();
    void runPattern();
    uint8_t isDrawingBusy();
    uint32_t getFrameCounter();
#ifdef __cplusplus
}

#ifdef __cplusplus

class WS2812Pattern {
public:
    // initFrame returns the number of milliseconds to delay until the next frame, or 0 if a frame needs to be rendered now.
	virtual uint32_t initFrame() = 0;
	virtual glm::u8vec3 shadePixel(uint8_t channel, uint16_t pixelIdx);
};

#else
struct WS2812Pattern;
#endif

#endif
