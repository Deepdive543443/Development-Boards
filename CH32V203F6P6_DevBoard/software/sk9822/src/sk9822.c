// ===================================================================================
// SK9822 LED Strip Functions for CH32V203                                    * v1.1 *
// ===================================================================================

#include "sk9822.h"
#include "spi.h"

#define SK9822_GLOBAL_MAX 31 // 5-bit global brightness
#define SK9822_LEVEL_MAX  65535UL
#define SK9822_BREATHE_STEPS 256 // table steps per full breathing period

// round(65535 * ((1 - cos(2*pi*i/256)) / 2) ^ 2.2) for i = 0..128 (first half period)
static const uint16_t SK9822_BREATHE_TABLE[SK9822_BREATHE_STEPS / 2 + 1] = {
        0,     0,     0,     0,     0,     0,     1,     1,
        2,     4,     6,    10,    14,    20,    28,    37,
       49,    64,    82,   104,   130,   160,   195,   237,
      284,   338,   399,   468,   546,   633,   731,   838,
      957,  1088,  1231,  1388,  1559,  1744,  1945,  2162,
     2395,  2646,  2915,  3202,  3508,  3834,  4180,  4547,
     4935,  5345,  5777,  6231,  6708,  7208,  7731,  8278,
     8848,  9442, 10060, 10702, 11367, 12056, 12768, 13504,
    14263, 15044, 15848, 16674, 17521, 18388, 19276, 20184,
    21110, 22054, 23016, 23994, 24987, 25995, 27016, 28049,
    29094, 30148, 31211, 32282, 33359, 34441, 35526, 36614,
    37702, 38790, 39875, 40957, 42033, 43103, 44165, 45217,
    46257, 47285, 48299, 49297, 50277, 51239, 52180, 53100,
    53997, 54869, 55715, 56533, 57323, 58083, 58812, 59509,
    60173, 60801, 61395, 61952, 62471, 62952, 63394, 63796,
    64158, 64478, 64757, 64994, 65188, 65340, 65448, 65513,
    65535,
};

// Init SPI1 for the LED strip
void SK9822_init(void)
{
    SPI1_init();
}

// Scale one 8-bit channel by the PWM factor f (0..65535), rounded
static uint8_t SK9822_scale(uint8_t c, uint32_t f)
{
    return (c * f + SK9822_LEVEL_MAX / 2) / SK9822_LEVEL_MAX;
}

// Send num_leds LEDs, all with the same color, at master level 0..65535
void SK9822_show(SK9822_color_t color, uint16_t num_leds, uint16_t level)
{
    // Smallest global brightness that can still reach the level, then PWM does the rest:
    // level / 65535 = (global / 31) * (f / 65535)
    uint32_t global = ((uint32_t)level * SK9822_GLOBAL_MAX + SK9822_LEVEL_MAX - 1) / SK9822_LEVEL_MAX;
    uint32_t f = global ? (uint32_t)level * SK9822_GLOBAL_MAX / global : 0;
    uint8_t led[4] = {0xE0 | global, SK9822_scale(color.b, f), SK9822_scale(color.g, f),
                      SK9822_scale(color.r, f)};

    for (uint8_t i = 0; i < 4; i++) SPI1_write(0x00); // start frame
    for (uint16_t i = 0; i < num_leds; i++) SPI1_writeBuffer(led, sizeof(led));
    for (uint8_t i = 0; i < 4; i++) SPI1_write(0x00); // reset frame
    for (uint16_t i = 0; i < (num_leds + 15) / 16; i++) SPI1_write(0x00); // end frame
    SPI1_flush();
}

// Breathing table value at step i = 0..256, mirrored for the second half period
static int32_t SK9822_breathe_entry(uint32_t i)
{
    return SK9822_BREATHE_TABLE[i <= SK9822_BREATHE_STEPS / 2 ? i : SK9822_BREATHE_STEPS - i];
}

// Breathing level at time t (ms) for period p (ms, < 65536), peaking at global 1..31
uint16_t SK9822_breathe(uint32_t t_ms, uint32_t period_ms, uint8_t peak_global)
{
    uint32_t phase = (t_ms % period_ms) * (SK9822_BREATHE_STEPS << 8) / period_ms; // 8.8 fixed point
    uint32_t i = phase >> 8;
    int32_t frac = phase & 0xFF;
    int32_t a = SK9822_breathe_entry(i);
    int32_t b = SK9822_breathe_entry(i + 1);
    uint32_t level = a + (((b - a) * frac) >> 8);
    return level * peak_global / SK9822_GLOBAL_MAX;
}
