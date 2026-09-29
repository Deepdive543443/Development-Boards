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
// Blink example.
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
// - Run 'make flash'.

// ===================================================================================
// Libraries, Definitions and Macros
// ===================================================================================
#include <system.h> // system functions
#include <gpio.h>   // GPIO functions
#include <nanos.h>  // nanosecond timestamp functions

#define PIN_LED       PB1                 // define LED pin
#define LED_PERIOD_NS (1ULL * NS_PER_SEC) // toggle LED every 1 second

// Deadline reached? (signed difference, so it stays correct across wrap-around)
#define NS_due(t) ((int64_t)(NS_read() - (t)) >= 0)

// ===================================================================================
// Main Function
// ===================================================================================
int main(void)
{
    // Setup
    PIN_output(PIN_LED);                           // set LED pin to output
    NS_init();                                     // start nanosecond counter
    uint64_t led_next = NS_read() + LED_PERIOD_NS; // next LED toggle time

    // Loop (never blocks: add more tasks with their own "next" timestamp)
    while (1)
    {
        IWDG_feed(); // in case hardware watchdog is enabled
        if (NS_due(led_next))
        {
            PIN_toggle(PIN_LED);       // toggle LED
            led_next += LED_PERIOD_NS; // advance from deadline: no drift
        }
    }
}
