// ===================================================================================
// SK9822 LED Strip Functions for CH32V203                                    * v1.1 *
// ===================================================================================

#include "sk9822.h"
#include "spi.h"
#include "timer.h"
#include "usb_cdc.h"
#include "storage.h"
#include "knob.h"

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

static SK9822_color_t s_color;                       // R, G, B at peak
static uint8_t s_brightness = SK9822_BRIGHTNESS_MAX; // peak global brightness (0: off)
static uint8_t s_breathe = 1;                        // 1: breathing, 0: steady
static uint32_t s_frame_next;                        // next frame time
static uint32_t s_knob_next;                         // next frame time

static const SK9822_color_t SK9822_DEFAULT_COLOR = {255, 40, 0}; // used if none is saved
static uint8_t s_save_pending = 0; // color changed but not saved yet
static uint32_t s_save_due;        // when to save it

// Init SPI1 for the LED strip
void SK9822_init(void)
{
    SPI1_init();
    CDC_init();  // USB virtual serial port (PA11, PA12)
    KNOB_init(); // rotary encoder (PA0, PA1) and button (PA2)

    s_frame_next = MS_read();
    s_knob_next = MS_read();
    if (!STORAGE_load(&s_color)) s_color = SK9822_DEFAULT_COLOR;
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

// ===================================================================================
// Serial Commands
// ===================================================================================

// Parse 2 hex digits; returns -1 if invalid
static int16_t SK9822_parse_hex_byte(const char *s)
{
    int16_t value = 0;
    for (uint8_t i = 0; i < 2; i++)
    {
        char c = s[i];
        if (c >= '0' && c <= '9')      value = value * 16 + c - '0';
        else if (c >= 'a' && c <= 'f') value = value * 16 + c - 'a' + 10;
        else return -1;
    }
    return value;
}

// Reply with the current color as "#RRGGBB" (prefix "OK " after a change)
static void SK9822_reply_color(uint8_t ok)
{
    static const char HEX[] = "0123456789ABCDEF";
    char buf[11];
    char *p = buf;
    if (ok) { *p++ = 'O'; *p++ = 'K'; *p++ = ' '; }
    *p++ = '#';
    *p++ = HEX[s_color.r >> 4]; *p++ = HEX[s_color.r & 15];
    *p++ = HEX[s_color.g >> 4]; *p++ = HEX[s_color.g & 15];
    *p++ = HEX[s_color.b >> 4]; *p++ = HEX[s_color.b & 15];
    *p = 0;
    CDC_println(buf);
}

// Execute one command line (already lowercase, no line ending)
static void SK9822_handle_command(const char *cmd, uint8_t len)
{
    if (len == 1 && cmd[0] == '?')
    {
        SK9822_reply_color(0);
        return;
    }

    if (len == 7 && cmd[0] == '#') { cmd++; len--; }
    if (len == 6)
    {
        int16_t r = SK9822_parse_hex_byte(cmd), g = SK9822_parse_hex_byte(cmd + 2), b = SK9822_parse_hex_byte(cmd + 4);
        if (r >= 0 && g >= 0 && b >= 0)
        {
            s_color = (SK9822_color_t){r, g, b};
            s_save_pending = 1;                  // save once the color settles
            s_save_due = MS_read() + SK9822_SAVE_DELAY_MS;
            SK9822_reply_color(1);
            return;
        }
    }

    const char *def = "default";
    uint8_t i = 0;
    while (i < len && cmd[i] == def[i]) i++;
    if (len == 7 && i == 7)
    {
        STORAGE_clear();
        s_save_pending = 0;
        s_color = SK9822_DEFAULT_COLOR;
        SK9822_reply_color(1);
        return;
    }

    CDC_println("ERR");
}

// Collect received characters into lines and run them
static void SK9822_poll_serial(void)
{
    static char line[SK9822_CMD_MAX_LEN + 1];
    static uint8_t len = 0;
    static uint8_t overflow = 0;

    while (CDC_available())
    {
        char c = CDC_read();
        if (c == '\r' || c == '\n')
        {
            if (overflow) CDC_println("ERR");
            else if (len) SK9822_handle_command(line, len);
            len = 0;
            overflow = 0;
        }
        else if (len < SK9822_CMD_MAX_LEN)
        {
            if (c >= 'A' && c <= 'Z') c += 'a' - 'A'; // case-insensitive
            line[len++] = c;
        }
        else overflow = 1; // too long: ignore the rest of the line
    }
}

void SK9822_setColor(SK9822_color_t color) { s_color = color; }
SK9822_color_t SK9822_getColor(void) { return s_color; }

void SK9822_setBrightness(int16_t brightness)
{
    s_brightness = brightness < 0 ? 0 : brightness > SK9822_BRIGHTNESS_MAX ? SK9822_BRIGHTNESS_MAX : brightness;
}
uint8_t SK9822_getBrightness(void) { return s_brightness; }

void SK9822_setBreathe(uint8_t on) { s_breathe = on ? 1 : 0; }
uint8_t SK9822_getBreathe(void) { return s_breathe; }

void SK9822_update(void)
{
    SK9822_poll_serial(); // color commands from the PC

    if (s_save_pending && MS_due(s_save_due))
    {
        STORAGE_save(s_color); // only after the color settled: spares the flash
        s_save_pending = 0;
    }

    if (!MS_due(s_frame_next)) return;
    uint16_t level = s_breathe
                         ? SK9822_breathe(MS_read(), SK9822_BREATHE_MS, s_brightness) // level from time, not frame count
                         : SK9822_LEVEL_MAX * s_brightness / SK9822_GLOBAL_MAX;       // steady at the breathing peak
    SK9822_show(s_color, SK9822_NUM_LEDS, level);
    s_frame_next += SK9822_FRAME_MS; // advance from deadline: no drift

    if (MS_due(s_knob_next))
    {
        if (KNOB_pressed()) SK9822_setBreathe(!SK9822_getBreathe());                 // toggle breathing / steady
        if (!s_breathe) SK9822_setBrightness(SK9822_getBrightness() + KNOB_steps()); // right: brighter, left: darker (clamped)
        s_knob_next += KNOB_POLL_MS;
    }
}
