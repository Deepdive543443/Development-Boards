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
// rotate right / left      brighter / darker (0..SK9822_BRIGHTNESS_MAX, not saved)
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
#include <system.h>  // system functions
#include <gpio.h>    // GPIO functions
#include <timer.h>   // timestamp functions
#include <sk9822.h>  // SK9822 LED strip functions
#include <usb_cdc.h> // USB virtual serial port
#include <knob.h>    // rotary encoder knob

#define PIN_LED       PB1   // define LED pin
#define LED_PERIOD_MS (500) // toggle LED every 1 second

static void ch32v203f6p6_setup()
{
    PIN_output(PIN_LED);
    TIMER_init();
    SK9822_init();
}

// ===================================================================================
// Main Function
// ===================================================================================
int main(void)
{
    ch32v203f6p6_setup();
    uint32_t led_next = MS_read() + LED_PERIOD_MS;

    while (1)
    {
        IWDG_feed();

        // toggle LED
        if (MS_due(led_next))
        {
            PIN_toggle(PIN_LED);
            led_next += LED_PERIOD_MS;
        }

        SK9822_update();
    }
}
