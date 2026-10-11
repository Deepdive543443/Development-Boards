// ===================================================================================
// Rotary Encoder Knob with Push Button for CH32V203                          * v1.0 *
// ===================================================================================

#include "knob.h"
#include "gpio.h"

static uint16_t KNOB_lastCount; // TIM2 count at the last detent

// Init encoder (TIM2 encoder mode) and button pin
void KNOB_init(void)
{
    PIN_input_PU(PIN_KNOB_A);
    PIN_input_PU(PIN_KNOB_B);
    PIN_input_PD(PIN_KNOB_SW);

    RCC->APB1PCENR |= RCC_TIM2EN;
    TIM2->CTLR1 = TIM_CKD_1;                 // filter clock = timer clock / 4
    TIM2->CHCTLR1 = TIM_CC1S_0 | TIM_IC1F    // CH1 input from TI1, strongest filter
                  | TIM_CC2S_0 | TIM_IC2F;   // CH2 input from TI2, strongest filter
    TIM2->CCER = 0;                          // both channels non-inverted
    TIM2->SMCFGR = TIM_SMS_0 | TIM_SMS_1;    // encoder mode 3: count all edges of both
    TIM2->ATRLR = 0xFFFF;                    // count over the full 16-bit range
    TIM2->CNT = 0;
    TIM2->CTLR1 |= TIM_CEN;                  // start counting
    KNOB_lastCount = 0;
}

// Detent steps turned since last call (+ right, - left)
int8_t KNOB_steps(void)
{
    int16_t diff = (int16_t)(TIM2->CNT - KNOB_lastCount); // wraps correctly
    int8_t steps = diff / KNOB_COUNTS_PER_STEP;           // full detents only
    KNOB_lastCount += steps * KNOB_COUNTS_PER_STEP;       // keep the partial rest
    return KNOB_REVERSE ? -steps : steps;
}

// 1 once per button press (debounced; call every KNOB_POLL_MS)
uint8_t KNOB_pressed(void)
{
    static uint8_t stable = 0; // debounced state: 1 = pressed
    static uint8_t count = 0;  // polls the raw state differed from the stable state
    uint8_t raw = !PIN_read(PIN_KNOB_SW);
    if (raw == stable)
    {
        count = 0;
        return 0;
    }
    if (++count < KNOB_DEBOUNCE) return 0;
    count = 0;
    stable = raw;
    return stable; // report the press, not the release
}
