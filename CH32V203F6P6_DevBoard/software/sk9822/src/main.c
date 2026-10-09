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
// Blink example plus an orange breathing light on a 30-LED SK9822 strip.
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

#define PIN_LED       PB1           // define LED pin
#define LED_PERIOD_MS (1ULL * 1000) // toggle LED every 1 second
#define NUM_LEDS        30    // LEDs on the strip
#define FRAME_PERIOD_MS 5     // send a new frame every 5ms (200Hz)
#define BREATHE_MS      10000 // one full breath (dark -> peak -> dark) in 10 seconds
#define BREATHE_PEAK    5     // peak global brightness (1..31): breathe between 0 and 5

static const SK9822_color_t ORANGE = {255, 70, 0}; // R, G, B at peak

static void ch32v203f6p6_setup()
{
    PIN_output(PIN_LED); // set LED pin to output
    TIMER_init();        // start timer
    SK9822_init();       // init LED strip on SPI1 (SCK: PA5, MOSI: PA7)
}

// ===================================================================================
// Main Function
// ===================================================================================
int main(void)
{
    ch32v203f6p6_setup();                          // init LED pin, timer and LED strip
    uint32_t led_next = MS_read() + LED_PERIOD_MS; // next LED toggle time
    uint32_t frame_next = MS_read();               // next LED strip frame time

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
            uint16_t level = SK9822_breathe(MS_read(), BREATHE_MS, BREATHE_PEAK); // level from time, not frame count
            SK9822_show(ORANGE, NUM_LEDS, level);
            frame_next += FRAME_PERIOD_MS;
        }
    }
}
