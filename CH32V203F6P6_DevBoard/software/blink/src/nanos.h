// ===================================================================================
// Nanosecond Timestamp Functions for CH32V203                                * v1.0 *
// ===================================================================================
//
// Functions available:
// --------------------
// NS_init()                init and start nanosecond counter at zero
// NS_read()                read nanoseconds since NS_init() (64-bit)
//
// Notes:
// ------
// Based on the millis example (millis.c). The SysTick compare interrupt fires every
// 1ms and adds 1000000 to the nanosecond counter. NS_read() adds the SysTick ticks
// elapsed since the last interrupt, so the resolution is one system clock cycle
// (~6.94ns @ 144MHz). An interrupt every 1ns is not possible: that is shorter than
// one CPU cycle. The delay (DLY) functions continue to work properly.

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

void NS_init(void);
uint64_t NS_read(void);

#ifdef __cplusplus
};
#endif
