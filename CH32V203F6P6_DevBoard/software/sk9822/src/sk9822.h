// ===================================================================================
// SK9822 LED Strip Functions for CH32V203                                    * v1.1 *
// ===================================================================================
//
// Functions available:
// --------------------
// SK9822_init()             Init SPI1 for the LED strip (SCK: PA5, MOSI: PA7), load saved color
// SK9822_show(c, n, l)      Send n LEDs, all with color c, at master level l
// SK9822_breathe(t, p, g)   Breathing level at time t (ms), period p (ms), peak global g
// SK9822_update()           Main loop task: frames, serial commands, color saving
// SK9822_setColor(c)        Set the strip color (R, G, B at peak)
// SK9822_getColor()         Get the strip color
// SK9822_setBrightness(b)   Set peak global brightness, clamped to 0..SK9822_BRIGHTNESS_MAX
// SK9822_getBrightness()    Get peak global brightness (0: off)
// SK9822_setBreathe(on)     1: breathing, 0: steady at the breathing peak
// SK9822_getBreathe()       Get breathing mode
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
//
// SK9822_update() never blocks except while a frame is sent: it sends one frame every
// SK9822_FRAME_MS. It also runs the serial color commands (see main.c) and saves the
// color to flash SK9822_SAVE_DELAY_MS after the last change. Call TIMER_init() before
// SK9822_init(), and CDC_init() for the serial commands. The defaults below can be
// overridden with compile definitions.

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "system.h"

#ifndef SK9822_NUM_LEDS
#define SK9822_NUM_LEDS       70    // LEDs on the strip
#endif
#ifndef SK9822_FRAME_MS
#define SK9822_FRAME_MS       5     // send a new frame every 5ms (200Hz)
#endif
#ifndef SK9822_BREATHE_MS
#define SK9822_BREATHE_MS     10000 // one full breath (dark -> peak -> dark) in 10 seconds
#endif
#ifndef SK9822_SAVE_DELAY_MS
#define SK9822_SAVE_DELAY_MS  2000  // save color to flash once it stopped changing for 2s
#endif
#ifndef SK9822_CMD_MAX_LEN
#define SK9822_CMD_MAX_LEN    16    // longest serial command line
#endif
#ifndef SK9822_BRIGHTNESS_MAX
#define SK9822_BRIGHTNESS_MAX 6     // max global brightness (of 31): higher draws too much current
#endif

typedef struct
{
    uint8_t r, g, b;
} SK9822_color_t;

void SK9822_init(void);
void SK9822_show(SK9822_color_t color, uint16_t num_leds, uint16_t level);
uint16_t SK9822_breathe(uint32_t t_ms, uint32_t period_ms, uint8_t peak_global);

void SK9822_update(void);
void SK9822_setColor(SK9822_color_t color);
SK9822_color_t SK9822_getColor(void);
void SK9822_setBrightness(int16_t brightness);
uint8_t SK9822_getBrightness(void);
void SK9822_setBreathe(uint8_t on);
uint8_t SK9822_getBreathe(void);

#ifdef __cplusplus
};
#endif
