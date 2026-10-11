// ===================================================================================
// USB CDC (Virtual Serial Port) Functions for CH32V203                       * v1.0 *
// ===================================================================================
//
// Functions available:
// --------------------
// CDC_init()               Init USB (USBD on PA11/PA12) and start enumeration
// CDC_ready()              Check if the host has configured the device (1) or not (0)
// CDC_available()          Number of received bytes waiting to be read
// CDC_read()               Read one received byte (check CDC_available() first!)
// CDC_write(c)             Write one character to the send buffer
// CDC_print(s)             Write string to the send buffer
// CDC_println(s)           Write string plus newline and flush
// CDC_flush()              Send the buffered characters to the host
//
// Notes:
// ------
// Uses the USBD peripheral (STM32F103-style), the one wired to PA11/PA12 on the
// CH32V203F6P6. The internal 1.5k pull-up on D+ is used, no external parts needed.
// The device enumerates as a standard CDC-ACM serial port, which needs no driver on
// Linux, macOS and Windows 10+. The baudrate set by the host is ignored.
// Nothing here blocks forever: if no program on the host has the port open, sent
// data is dropped after a short timeout instead of hanging the main loop.
// USB needs a 48MHz clock from the PLL, so F_CPU must be 48, 96 or 144MHz.

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "system.h"

#if SYS_USE_VECTORS == 0
#error Interrupt vector table must be enabled (SYS_USE_VECTORS in system.h)!
#endif

// USB device descriptor
#define USB_VENDOR_ID      0x16C0 // VID (shared www.voti.nl)
#define USB_PRODUCT_ID     0x27DD // PID (shared CDC-ACM)
#define USB_DEVICE_VERSION 0x0100 // v1.0 (BCD-format)
#define USB_MAX_POWER_mA   500    // max power in mA (LED strip)

// USB descriptor strings (find the port by the product string)
#define MANUFACTURER_STR   'w','a','g','i','m','i','n','a','t','o','r'
#define PRODUCT_STR        'C','H','3','2','V','2','0','3',' ','S','K','9','8','2','2'
#define SERIAL_STR         'C','H','3','2','V','C','D','C'
#define INTERFACE_STR      'C','D','C','-','S','e','r','i','a','l'

void CDC_init(void);
uint8_t CDC_ready(void);
uint8_t CDC_available(void);
char CDC_read(void);
void CDC_write(char c);
void CDC_print(const char *str);
void CDC_println(const char *str);
void CDC_flush(void);

#ifdef __cplusplus
};
#endif
