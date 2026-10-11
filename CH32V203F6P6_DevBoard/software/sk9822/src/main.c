// ===================================================================================
// Project:   Example for CH32V203
// Version:   v1.0
// Year:      2023
// Author:    Stefan Wagner
// Github:    https://github.com/wagiminator
// EasyEDA:   https://easyeda.com/wagiminator
// License:   http://creativecommons.org/licenses/by-sa/3.0/
// ===================================================================================
//
// Description:
// ------------
// Blink example plus a breathing light on an SK9822 strip. The color can be changed
// from the PC over USB (virtual serial port, see tools/set_color.py) and is saved in
// flash, so it survives a power cycle.
//
// Serial commands (one per line, case-insensitive, replies end with a newline):
// #RRGGBB or RRGGBB        set color (hex), reply "OK #RRGGBB"
// ?                        reply current color "#RRGGBB"
// DEFAULT                  forget saved color, back to the built-in default
//
// Knob (rotary encoder with push button, see knob.h for wiring):
// press                    switch between breathing and steady light
// rotate right / left      brighter / darker (0..BRIGHTNESS_MAX, not saved)
//
// References:
// -----------
// - WCH Nanjing Qinheng Microelectronics: http://wch.cn
//
// Compilation Instructions:
// -------------------------
// - Make sure GCC toolchain (gcc-riscv64-unknown-elf, newlib) and Python3 with
//   chprog and rvprog (via pip) are installed. In addition, Linux requires access
//   rights to the USB bootloader.
// - Press the BOOT0 button on the MCU board and keep it pressed while connecting it
//   via USB to your PC.
// - Run 'cmake -B build' once, then 'cmake --build build -t flash'.

// ===================================================================================
// Libraries, Definitions and Macros
// ===================================================================================
#include <system.h> // system functions
#include <gpio.h>   // GPIO functions
#include <timer.h>  // timestamp functions
#include <sk9822.h> // SK9822 LED strip functions
#include <usb_cdc.h> // USB virtual serial port
#include <storage.h> // saved color in flash
#include <knob.h>    // rotary encoder knob

#define PIN_LED       PB1           // define LED pin
#define LED_PERIOD_MS (500) // toggle LED every 1 second
#define NUM_LEDS        70    // LEDs on the strip
#define FRAME_PERIOD_MS 5     // send a new frame every 5ms (200Hz)
#define BREATHE_MS      10000 // one full breath (dark -> peak -> dark) in 10 seconds
#define BRIGHTNESS_MAX  6    // max global brightness (of 31): higher draws too much current
#define SAVE_DELAY_MS   2000  // save color to flash once it stopped changing for 2s
#define CMD_MAX_LEN     16    // longest serial command line

static const SK9822_color_t DEFAULT_COLOR = {255, 40, 0}; // used if none is saved
static SK9822_color_t s_color;                              // R, G, B at peak

static uint8_t s_brightness = BRIGHTNESS_MAX; // global brightness 0..BRIGHTNESS_MAX (0: off)
static uint8_t s_breathe = 1;                 // 1: breathing, 0: steady

static uint8_t s_save_pending = 0; // color changed but not saved yet
static uint32_t s_save_due;        // when to save it

static void ch32v203f6p6_setup()
{
    PIN_output(PIN_LED); // set LED pin to output
    TIMER_init();        // start timer
    SK9822_init();       // init LED strip on SPI1 (SCK: PA5, MOSI: PA7)
    CDC_init();          // USB virtual serial port (PA11, PA12)
    KNOB_init();         // rotary encoder (PA0, PA1) and button (PA2)
    if (!STORAGE_load(&s_color)) s_color = DEFAULT_COLOR;
}

// ===================================================================================
// Serial Commands
// ===================================================================================

// Parse 2 hex digits; returns -1 if invalid
static int16_t parse_hex_byte(const char *s)
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
static void reply_color(uint8_t ok)
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
static void handle_command(const char *cmd, uint8_t len)
{
    if (len == 1 && cmd[0] == '?')
    {
        reply_color(0);
        return;
    }

    if (len == 7 && cmd[0] == '#') { cmd++; len--; }
    if (len == 6)
    {
        int16_t r = parse_hex_byte(cmd), g = parse_hex_byte(cmd + 2), b = parse_hex_byte(cmd + 4);
        if (r >= 0 && g >= 0 && b >= 0)
        {
            s_color = (SK9822_color_t){r, g, b};
            s_save_pending = 1;                  // save once the color settles
            s_save_due = MS_read() + SAVE_DELAY_MS;
            reply_color(1);
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
        s_color = DEFAULT_COLOR;
        reply_color(1);
        return;
    }

    CDC_println("ERR");
}

// Collect received characters into lines and run them
static void poll_serial(void)
{
    static char line[CMD_MAX_LEN + 1];
    static uint8_t len = 0;
    static uint8_t overflow = 0;

    while (CDC_available())
    {
        char c = CDC_read();
        if (c == '\r' || c == '\n')
        {
            if (overflow) CDC_println("ERR");
            else if (len) handle_command(line, len);
            len = 0;
            overflow = 0;
        }
        else if (len < CMD_MAX_LEN)
        {
            if (c >= 'A' && c <= 'Z') c += 'a' - 'A'; // case-insensitive
            line[len++] = c;
        }
        else overflow = 1; // too long: ignore the rest of the line
    }
}

// ===================================================================================
// Main Function
// ===================================================================================
int main(void)
{
    ch32v203f6p6_setup();                          // init LED pin, timer and LED strip
    uint32_t led_next = MS_read() + LED_PERIOD_MS; // next LED toggle time
    uint32_t frame_next = MS_read();               // next LED strip frame time
    uint32_t knob_next = MS_read();                // next knob button poll time

    // Loop (never blocks: add more tasks with their own "next" timestamp)
    while (1)
    {
        IWDG_feed(); // in case hardware watchdog is enabled
        if (MS_due(led_next))
        {
            PIN_toggle(PIN_LED);       // toggle LED
            led_next += LED_PERIOD_MS; // advance from deadline: no drift
        }

        if (MS_due(frame_next))
        {
            uint16_t level = s_breathe
                           ? SK9822_breathe(MS_read(), BREATHE_MS, s_brightness) // level from time, not frame count
                           : 65535UL * s_brightness / 31;                       // steady at the breathing peak
            SK9822_show(s_color, NUM_LEDS, level);
            frame_next += FRAME_PERIOD_MS;
        }

        if (MS_due(knob_next))
        {
            if (KNOB_pressed()) s_breathe = !s_breathe; // toggle breathing / steady
            int16_t b = s_brightness + KNOB_steps();    // right: brighter, left: darker
            s_brightness = b < 0 ? 0 : b > BRIGHTNESS_MAX ? BRIGHTNESS_MAX : b;
            knob_next += KNOB_POLL_MS;
        }

        poll_serial(); // color commands from the PC

        if (s_save_pending && MS_due(s_save_due))
        {
            STORAGE_save(s_color); // only after the color settled: spares the flash
            s_save_pending = 0;
        }
    }
}
