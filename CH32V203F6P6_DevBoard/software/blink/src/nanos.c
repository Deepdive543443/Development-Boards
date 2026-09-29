// ===================================================================================
// Nanosecond Timestamp Functions for CH32V203                                * v1.0 *
// ===================================================================================

#include "nanos.h"

volatile uint64_t NS_counter = 0; // nanoseconds at the last SysTick interrupt

// Init and start nanosecond counter (same SysTick setup as MIL_init())
void NS_init(void)
{
    STK->CTLR = 0;                // disable SysTick
    NS_counter = 0;               // start at zero
    NVIC_EnableIRQ(SysTicK_IRQn); // enable the SysTick IRQ
    STK->CMPL = DLY_MS_TIME - 1;  // set interval to 1ms
    STK->CMPH = 0;
    STK->CNTL = 0; // start at zero
    STK->CNTH = 0;
    STK->CTLR = STK_CTLR_STE      // enable SysTick
                | STK_CTLR_STIE   // enable SysTick compare match interrupt
                | STK_CTLR_STCLK; // set SysTick clock to F_CPU
}

// Read nanoseconds since NS_init()
uint64_t NS_read(void)
{
    uint64_t base;
    uint32_t elapsed;
    do
    { // retry if the ISR ran in between
        base = NS_counter;
        elapsed = STK->CNTL - (STK->CMPL - DLY_MS_TIME); // ticks since last interrupt
    } while (base != NS_counter);
    // 32-bit math only: elapsed is ~1ms of ticks, so elapsed * 1000 fits in 32 bits
    return base + elapsed * 1000 / DLY_US_TIME;
}

// Interrupt service routine (same as millis.c, but counts nanoseconds)
void SysTick_Handler(void) __attribute__((interrupt));
void SysTick_Handler(void)
{
    uint32_t temp;
    NS_counter += NS_PER_MS;           // increase nanosecond counter by 1ms
    temp = STK->CMPL;                  // save for 64-bit add
    STK->CMPL += DLY_MS_TIME;          // next interrupt 1ms later
    if (STK->CMPL < temp) STK->CMPH++; // high-word
    STK->SR = 0;                       // clear interrupt flag
}
