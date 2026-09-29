// ===================================================================================
// SPI1 Master Functions (TX only) for CH32V203                               * v1.1 *
// ===================================================================================
//
// Scope:
// ------
// This is NOT a general-purpose SPI driver. It controls the SPI1 peripheral only,
// with a fixed pin mapping and a fixed transmit-only master configuration, which is
// all an SK9822/APA102 LED strip needs (clock + data, no MISO, no NSS). All names
// start with SPI1_ to make that explicit. SPI2, other pins (remap), receive,
// full-duplex, slave mode, 16-bit frames, DMA and CRC are not supported.
//
// Functions available:
// --------------------
// SPI1_init()               Init SPI1 with defined clock rate (see below)
// SPI1_write(d)             Transmit one data byte
// SPI1_writeBuffer(b, n)    Transmit n data bytes from buffer b
// SPI1_flush()              Wait until the last byte has been shifted out
//
// SPI1_busy()               Check if SPI bus is busy
// SPI1_ready()              Check if SPI is ready to write
// SPI1_enable()             Enable SPI module
// SPI1_disable()            Disable SPI module
// SPI1_setBAUD(n)           Set BAUD rate (see below)
// SPI1_setCPOL(n)           0: SCK low in idle, 1: SCK high in idle
// SPI1_setCPHA(n)           Start sampling from 0: first clock edge, 1: second clock edge
//
// SPI1 pin mapping (default, no remap):
// -------------------------------------
// SCK-pin   PA5
// MOSI-pin  PA7
// PA6 (MISO) and PA4 (NSS) are not configured and stay free for other use.
//
// Notes:
// ------
// Based on spi_tx.c (CH32X033) and the WCH EVT SPI examples. SPI1 runs as master in
// one-line transmit-only mode, 8-bit, MSB first, mode 0 (CPOL=0, CPHA=0). If a slave
// select is needed, it must be defined and controlled by the application as a GPIO.
// SPI1 is clocked by PCLK2, which system.c sets to HCLK (= F_CPU).

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "system.h"

// SPI Parameters
#define SPI1_PRESC 5 // SPI_CLKRATE = F_CPU / (2 << SPI1_PRESC) = 2.25MHz @ 144MHz

// SPI Functions and Macros
#define SPI1_busy()  (SPI1->STATR & SPI_STATR_BSY)
#define SPI1_ready() (SPI1->STATR & SPI_STATR_TXE)

#define SPI1_enable()   SPI1->CTLR1 |= SPI_CTLR1_SPE
#define SPI1_disable()  SPI1->CTLR1 &= ~SPI_CTLR1_SPE
#define SPI1_setCPOL(n) (n) ? (SPI1->CTLR1 |= SPI_CTLR1_CPOL) : (SPI1->CTLR1 &= ~SPI_CTLR1_CPOL)
#define SPI1_setCPHA(n) (n) ? (SPI1->CTLR1 |= SPI_CTLR1_CPHA) : (SPI1->CTLR1 &= ~SPI_CTLR1_CPHA)
#define SPI1_setBAUD(n) SPI1->CTLR1 = (SPI1->CTLR1 & ~SPI_CTLR1_BR) | (((n)&7) << 3)

void SPI1_init(void);
void SPI1_write(uint8_t data);
void SPI1_writeBuffer(const uint8_t* buf, uint16_t len);
void SPI1_flush(void);

#ifdef __cplusplus
};
#endif
