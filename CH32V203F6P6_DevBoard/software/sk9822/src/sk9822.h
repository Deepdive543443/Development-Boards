// ===================================================================================
// SK9822 LED Strip Functions for CH32V203                                    * v1.1 *
// ===================================================================================
//
// Functions available:
// --------------------
// SK9822_init()             Init SPI1 for the LED strip (SCK: PA5, MOSI: PA7)
// SK9822_show(c, n, l)      Send n LEDs, all with color c, at master level l
// SK9822_breathe(t, p, g)   Breathing level at time t (ms), period p (ms), peak global g
//
// Notes:
// ------
// Colors are 8-bit RGB. The master level l (0..65535) is linear light output: it is
// split into the SK9822 5-bit global brightness (drive current) and the 8-bit PWM
// value, so dim levels keep ~31x finer steps than with PWM alone.
// SK9822_show() blocks while the frame is sent: 4 + 4*n + 4 + ceil(n/16) bytes,
// ~460us for 30 LEDs at 2.25MHz SPI clock (see SPI1_PRESC in spi.h).
// Frame format: 32-bit zero start frame, per LED 0xE0|global, B, G, R, then a 32-bit
// zero reset frame and at least n/2 extra clock bits so the last LED latches its data.
//
// SK9822_breathe(): perceived brightness follows (1 - cos(2*pi*t/p)) / 2, dark at
// t = 0, peak at p/2. The peak is global brightness g (1..31) at full PWM, so g = 5
// breathes between off and 5/31 of full output. The curve is gamma corrected (2.2)
// into linear light output so it looks smooth to the eye. No FPU and no libm: a
// 129-entry lookup table holds half the period (the curve is symmetric) and is
// linearly interpolated. The level depends only on t, so late or skipped frames do
// not change the speed.

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "system.h"

typedef struct
{
    uint8_t r, g, b;
} SK9822_color_t;

void SK9822_init(void);
void SK9822_show(SK9822_color_t color, uint16_t num_leds, uint16_t level);
uint16_t SK9822_breathe(uint32_t t_ms, uint32_t period_ms, uint8_t peak_global);

#ifdef __cplusplus
};
#endif
