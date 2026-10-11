// ===================================================================================
// Persistent Color Storage in Flash for CH32V203                             * v1.0 *
// ===================================================================================

#include "storage.h"

#define STORAGE_ADDR  0x08007000 // last 4K sector (excluded in the linker script)
#define STORAGE_MAGIC 0xC010     // marks a valid record (erased flash is not 0xFFFF on all chips)
#define FLASH_KEY1    0x45670123
#define FLASH_KEY2    0xCDEF89AB

// Record: 3 halfwords at STORAGE_ADDR: magic, R | G << 8, B
#define STORAGE_HW(i) (((volatile uint16_t *)STORAGE_ADDR)[i])

// Load saved color; returns 1 if found
uint8_t STORAGE_load(SK9822_color_t *color)
{
    if (STORAGE_HW(0) != STORAGE_MAGIC) return 0;
    color->r = STORAGE_HW(1) & 0xFF;
    color->g = STORAGE_HW(1) >> 8;
    color->b = STORAGE_HW(2) & 0xFF;
    return 1;
}

static void FLASH_wait(void)
{
    while (FLASH->STATR & FLASH_STATR_BSY);
    FLASH->STATR = FLASH_STATR_EOP; // clear end of operation flag
}

// Unlock flash and erase the storage sector
static void FLASH_eraseStorage(void)
{
    FLASH->KEYR = FLASH_KEY1;
    FLASH->KEYR = FLASH_KEY2;
    FLASH->CTLR |= FLASH_CTLR_PER; // 4K sector erase
    FLASH->ADDR = STORAGE_ADDR;
    FLASH->CTLR |= FLASH_CTLR_STRT;
    FLASH_wait();
    FLASH->CTLR &= ~FLASH_CTLR_PER;
}

static void FLASH_writeHalfword(uint8_t i, uint16_t data)
{
    FLASH->CTLR |= FLASH_CTLR_PG;
    STORAGE_HW(i) = data;
    FLASH_wait();
    FLASH->CTLR &= ~FLASH_CTLR_PG;
}

// Save color (skipped if already saved, to spare the flash)
void STORAGE_save(SK9822_color_t color)
{
    SK9822_color_t saved;
    if (STORAGE_load(&saved) && saved.r == color.r && saved.g == color.g && saved.b == color.b) return;
    FLASH_eraseStorage();
    FLASH_writeHalfword(1, color.r | (color.g << 8));
    FLASH_writeHalfword(2, color.b);
    FLASH_writeHalfword(0, STORAGE_MAGIC); // last: record only valid when complete
    FLASH->CTLR |= FLASH_CTLR_LOCK;
}

// Forget the saved color
void STORAGE_clear(void)
{
    if (STORAGE_HW(0) != STORAGE_MAGIC) return;
    FLASH_eraseStorage();
    FLASH->CTLR |= FLASH_CTLR_LOCK;
}
