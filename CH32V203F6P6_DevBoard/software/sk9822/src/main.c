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
// - Run 'cmake -B build' once, then 'cmake --build build -t flash'.

// ===================================================================================
// Libraries, Definitions and Macros
// ===================================================================================
#include <system.h> // system functions
#include <gpio.h>   // GPIO functions
#include <timer.h>  // timestamp functions
#include <spi.h>    // SPI functions

#define PIN_LED       PB1           // define LED pin
#define LED_PERIOD_MS (1ULL * 1000) // toggle LED every 1 second
#define SPI_PERIOD_MS (2ULL * 1000) // send SPI test pattern every 2 seconds

// SPI test pattern (MSB first): 0xAA = 8 alternating bits, 0xF0 = shows bit order
const uint8_t SPI_TEST[] = {0xAA, 0xF0, 0x00, 0xFF};

static void ch32v203f6p6_setup()
{
    PIN_output(PIN_LED); // set LED pin to output
    TIMER_init();        // start timer
    SPI1_init();         // init SPI1 (SCK: PA5, MOSI: PA7)
}

// ===================================================================================
// Main Function
// ===================================================================================
int main(void)
{
    ch32v203f6p6_setup();                          // init SPI1 (SCK: PA5, MOSI: PA7)
    uint32_t led_next = MS_read() + LED_PERIOD_MS; // next LED toggle time
    uint32_t spi_next = MS_read() + SPI_PERIOD_MS; // next SPI transmission time

    // Loop (never blocks: add more tasks with their own "next" timestamp)
    while (1)
    {
        IWDG_feed(); // in case hardware watchdog is enabled
        if (MS_due(led_next))
        {
            PIN_toggle(PIN_LED);       // toggle LED
            led_next += LED_PERIOD_MS; // advance from deadline: no drift
        }

        if (MS_due(spi_next))
        {
            SPI1_writeBuffer(SPI_TEST, sizeof(SPI_TEST)); // send test pattern
            spi_next += SPI_PERIOD_MS;
        }
    }
}
