// ===================================================================================
// Timer (Timestamp) Functions for CH32V203                                   * v1.1 *
// ===================================================================================

#include "timer.h"

static volatile uint64_t MS_counter = 0; // milliseconds since TIMER_init()

// Init and start timer (same SysTick setup as MIL_init())
void TIMER_init(void)
{
    STK->CTLR = 0;                // disable SysTick
    MS_counter = 0;               // start at zero
    NVIC_EnableIRQ(SysTicK_IRQn); // enable the SysTick IRQ
    STK->CMPL = DLY_MS_TIME - 1;  // set interval to 1ms
    STK->CMPH = 0;
    STK->CNTL = 0; // start at zero
    STK->CNTH = 0;
    STK->CTLR = STK_CTLR_STE      // enable SysTick
                | STK_CTLR_STIE   // enable SysTick compare match interrupt
                | STK_CTLR_STCLK; // set SysTick clock to F_CPU
}

// Read milliseconds since TIMER_init() (low word only: atomic, no retry needed)
uint32_t MS_read(void)
{
    return (uint32_t)MS_counter;
}

// Interrupt service routine (same as millis.c)
void SysTick_Handler(void) __attribute__((interrupt));
void SysTick_Handler(void)
{
    uint32_t temp;
    MS_counter++;                      // increase millisecond counter
    temp = STK->CMPL;                  // save for 64-bit add
    STK->CMPL += DLY_MS_TIME;          // next interrupt 1ms later
    if (STK->CMPL < temp) STK->CMPH++; // high-word
    STK->SR = 0;                       // clear interrupt flag
}
