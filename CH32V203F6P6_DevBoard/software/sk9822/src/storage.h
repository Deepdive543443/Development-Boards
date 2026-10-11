// ===================================================================================
// Persistent Color Storage in Flash for CH32V203                             * v1.0 *
// ===================================================================================
//
// Functions available:
// --------------------
// STORAGE_load(c)          Load saved color into c; returns 1 if found, 0 if none
// STORAGE_save(c)          Save color c (skipped if already saved)
// STORAGE_clear()          Forget the saved color
//
// Notes:
// ------
// Uses the last 4K sector of the 32K flash (0x08007000), which the linker script
// keeps free of code. The flash survives ~10K erase cycles, so avoid saving in a
// loop: save only after the color stopped changing for a while.

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "system.h"
#include "sk9822.h"

uint8_t STORAGE_load(SK9822_color_t *color);
void STORAGE_save(SK9822_color_t color);
void STORAGE_clear(void);

#ifdef __cplusplus
};
#endif
