// ===================================================================================
// Timer (Timestamp) Functions for CH32V203                                   * v1.1 *
// ===================================================================================
//
// Functions available:
// --------------------
// TIMER_init()             init and start timer at zero
// NS_read()                read nanoseconds since TIMER_init() (64-bit)
// MS_read()                read milliseconds since TIMER_init() (32-bit)
// NS_due(t)                check if nanosecond timestamp t has been reached
// MS_due(t)                check if millisecond timestamp t has been reached
//
// Notes:
// ------
// Based on the millis example (millis.c). The SysTick compare interrupt fires every
// 1ms and increases a 64-bit millisecond counter. NS_read() scales it to nanoseconds
// and adds the SysTick ticks elapsed since the last interrupt, so the
// resolution is one system clock cycle (~6.94ns @ 144MHz). An interrupt every 1ns is
// not possible: that is shorter than one CPU cycle.
// MS_read() wraps around after ~49.7 days; NS_read() after ~584 years. NS_due() and
// MS_due() compare with a signed difference, so they stay correct across wrap-around
// as long as t is less than half the range ahead.
// The delay (DLY) functions continue to work properly.

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "system.h"

#if SYS_USE_VECTORS == 0
#error Interrupt vector table must be enabled (SYS_USE_VECTORS in system.h)!
#endif

#define NS_PER_US  1000UL        // nanoseconds per microsecond
#define NS_PER_MS  1000000UL     // nanoseconds per millisecond
#define NS_PER_SEC 1000000000ULL // nanoseconds per second
#define MS_PER_SEC 1000UL        // milliseconds per second

#define NS_due(t) ((int64_t)(NS_read() - (t)) >= 0) // nanosecond deadline reached?
#define MS_due(t) ((int32_t)(MS_read() - (t)) >= 0) // millisecond deadline reached?

void TIMER_init(void);
uint32_t MS_read(void);

#ifdef __cplusplus
};
#endif
