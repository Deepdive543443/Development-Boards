// ===================================================================================
// Rotary Encoder Knob with Push Button for CH32V203                          * v1.0 *
// ===================================================================================
//
// Functions available:
// --------------------
// KNOB_init()              Init encoder (TIM2 encoder mode) and button pin
// KNOB_steps()             Detent steps turned since last call (+ right, - left)
// KNOB_pressed()           1 once per button press (call every KNOB_POLL_MS)
//
// Notes:
// ------
// Wiring (EC11 or similar): A -> PA0, B -> PA1, C -> GND, SW -> PA2, other SW -> GND.
// Internal pull-ups are used. A/B go to TIM2 CH1/CH2, which counts both edges of
// both channels in hardware (encoder mode 3): no interrupts, no missed steps while
// the main loop is busy. Contact bounce on one channel just counts back and forth,
// and the input filter removes short glitches.
// If the knob counts the wrong way, set KNOB_REVERSE to 1 (or swap A and B).
// If one click moves 2 steps or needs 2 clicks, change KNOB_COUNTS_PER_STEP.

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "system.h"

#define PIN_KNOB_A  PA0 // TIM2 CH1 (fixed)
#define PIN_KNOB_B  PA1 // TIM2 CH2 (fixed)
#define PIN_KNOB_SW PA2 // any free pin, pressed = low

#define KNOB_REVERSE         0 // 1: swap rotation direction
#define KNOB_COUNTS_PER_STEP 4 // encoder counts per detent (EC11: 4)
#define KNOB_POLL_MS         5 // KNOB_pressed() call interval
#define KNOB_DEBOUNCE        3 // button must be stable for 3 polls (15ms)

void KNOB_init(void);
int8_t KNOB_steps(void);
uint8_t KNOB_pressed(void);

#ifdef __cplusplus
};
#endif
