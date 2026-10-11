// ===================================================================================
// USB CDC (Virtual Serial Port) Functions for CH32V203                       * v1.1 *
// ===================================================================================
//
// Uses the USBD peripheral: on the CH32V203F6 it is the one connected to PA11/PA12
// (USBFS only has pins on other packages). USBD is the same design as the STM32F103
// USB block: endpoint buffers live in a 512-byte packet memory (PMA) which the CPU
// sees as 16-bit words at a 32-bit stride, and the status/toggle bits in the
// endpoint registers flip when written with 1.

#include "usb_cdc.h"

// USB clock: 48MHz from the PLL (RCC_CFGR0 USBPRE[1:0], bits 23:22)
#if F_CPU == 48000000
#define USB_PRESCALER 0x00000000 // PLL / 1
#elif F_CPU == 96000000
#define USB_PRESCALER 0x00400000 // PLL / 2
#elif F_CPU == 144000000
#define USB_PRESCALER 0x00800000 // PLL / 3
#else
#error USB needs F_CPU = 48, 96 or 144MHz!
#endif

// ===================================================================================
// Packet Memory and Endpoint Registers
// ===================================================================================
#define PMA_BASE 0x40006000
#define PMA16(a) (*(volatile uint16_t *)(PMA_BASE + (a) * 2))      // a: even USB byte address
#define EPR(n)   (((volatile uint16_t *)&USBD->EPR0)[(n) * 2])     // endpoint register n

// Buffer descriptor table at PMA address 0 (8 bytes per endpoint)
#define BT_ADDR_TX(n)  PMA16((n) * 8 + 0)
#define BT_COUNT_TX(n) PMA16((n) * 8 + 2)
#define BT_ADDR_RX(n)  PMA16((n) * 8 + 4)
#define BT_COUNT_RX(n) PMA16((n) * 8 + 6)

#define EP0_SIZE 64 // control endpoint
#define EP1_SIZE 8  // CDC notifications (interrupt IN, never used)
#define EP2_SIZE 64 // CDC data (bulk OUT + bulk IN)

// Endpoint buffers in the PMA (same layout as the WCH USBD CDC example)
#define EP0_RX_ADDR 0x040
#define EP0_TX_ADDR 0x080
#define EP1_TX_ADDR 0x0C0
#define EP2_RX_ADDR 0x100
#define EP2_TX_ADDR 0x140
#define RX_COUNT_64 0x8400 // RX buffer size 64: 32-byte blocks, 2 blocks

#define EP_TOGGLE_BITS (USBD_EPTX_STAT | USBD_EPRX_STAT | USBD_EP_DTOG_TX | USBD_EP_DTOG_RX)

// Set STAT_TX: toggle-on-write, so write the difference; leave everything else as is
static void EP_setTX(uint8_t ep, uint16_t stat)
{
    uint16_t v = EPR(ep);
    EPR(ep) = (v & USBD_EPREG_MASK) | USBD_EP_CTR_RX | USBD_EP_CTR_TX | ((v & USBD_EPTX_STAT) ^ stat);
}

// Set STAT_RX
static void EP_setRX(uint8_t ep, uint16_t stat)
{
    uint16_t v = EPR(ep);
    EPR(ep) = (v & USBD_EPREG_MASK) | USBD_EP_CTR_RX | USBD_EP_CTR_TX | ((v & USBD_EPRX_STAT) ^ stat);
}

// Clear transfer complete flags (write 0 clears, write 1 keeps)
static void EP_clearCTR_RX(uint8_t ep)
{
    EPR(ep) = (EPR(ep) & USBD_EPREG_MASK & ~USBD_EP_CTR_RX) | USBD_EP_CTR_TX;
}

static void EP_clearCTR_TX(uint8_t ep)
{
    EPR(ep) = (EPR(ep) & USBD_EPREG_MASK & ~USBD_EP_CTR_TX) | USBD_EP_CTR_RX;
}

// Configure endpoint type, address and status; data toggles back to DATA0
static void EP_config(uint8_t ep, uint16_t type, uint16_t statTX, uint16_t statRX)
{
    uint16_t v = EPR(ep);
    EPR(ep) = type | ep | USBD_EP_CTR_RX | USBD_EP_CTR_TX | ((v & EP_TOGGLE_BITS) ^ (statTX | statRX));
}

// Copy between RAM and packet memory
static void PMA_write(uint16_t addr, const uint8_t *src, uint8_t len)
{
    for (uint8_t i = 0; i < len; i += 2)
        PMA16(addr + i) = src[i] | (i + 1 < len ? src[i + 1] << 8 : 0);
}

static void PMA_read(uint16_t addr, uint8_t *dst, uint8_t len)
{
    for (uint8_t i = 0; i < len; i += 2)
    {
        uint16_t w = PMA16(addr + i);
        dst[i] = w;
        if (i + 1 < len) dst[i + 1] = w >> 8;
    }
}

// ===================================================================================
// Descriptors
// ===================================================================================
static const uint8_t DevDescr[] = {
    18, 0x01,                                         // length, type: device
    0x10, 0x01,                                       // USB 1.1
    0x02, 0x00, 0x00,                                 // class: CDC
    EP0_SIZE,                                         // EP0 max packet size
    USB_VENDOR_ID & 0xFF, USB_VENDOR_ID >> 8,         // VID
    USB_PRODUCT_ID & 0xFF, USB_PRODUCT_ID >> 8,       // PID
    USB_DEVICE_VERSION & 0xFF, USB_DEVICE_VERSION >> 8,
    1, 2, 3,                                          // string indices: manuf, prod, serial
    1                                                 // one configuration
};

static const uint8_t CfgDescr[] = {
    // Configuration
    9, 0x02, 67, 0,             // length, type: config, total length
    2, 1, 0,                    // 2 interfaces, configuration value 1, no string
    0x80, USB_MAX_POWER_mA / 2, // bus powered, max power in 2mA units
    // Interface 0: CDC communication (ACM)
    9, 0x04, 0, 0, 1, 0x02, 0x02, 0x01, 4,
    0x05, 0x24, 0x00, 0x10, 0x01, // header functional descriptor: CDC 1.10
    0x05, 0x24, 0x01, 0x00, 0x01, // call management: data interface 1
    0x04, 0x24, 0x02, 0x02,       // ACM: supports line coding and serial state
    0x05, 0x24, 0x06, 0x00, 0x01, // union: CDC IF0, data IF1
    // Endpoint 1 IN: interrupt (notifications)
    7, 0x05, 0x81, 0x03, EP1_SIZE, 0, 255,
    // Interface 1: CDC data
    9, 0x04, 1, 0, 2, 0x0A, 0x00, 0x00, 4,
    // Endpoint 2 OUT: bulk
    7, 0x05, 0x02, 0x02, EP2_SIZE, 0, 0,
    // Endpoint 2 IN: bulk
    7, 0x05, 0x82, 0x02, EP2_SIZE, 0, 0
};

#define STR_DESCR(name, ...) \
    static const uint16_t name[] = {(0x03 << 8) | (2 + sizeof((uint16_t[]){__VA_ARGS__})), __VA_ARGS__}
STR_DESCR(LangDescr, 0x0409); // US English
STR_DESCR(ManufDescr, MANUFACTURER_STR);
STR_DESCR(ProdDescr, PRODUCT_STR);
STR_DESCR(SerDescr, SERIAL_STR);
STR_DESCR(InterfDescr, INTERFACE_STR);
static const uint16_t *const StrDescr[] = {LangDescr, ManufDescr, ProdDescr, SerDescr, InterfDescr};

// ===================================================================================
// Variables
// ===================================================================================

// Standard requests
#define USB_GET_STATUS        0x00
#define USB_CLEAR_FEATURE     0x01
#define USB_SET_FEATURE       0x03
#define USB_SET_ADDRESS       0x05
#define USB_GET_DESCRIPTOR    0x06
#define USB_GET_CONFIGURATION 0x08
#define USB_SET_CONFIGURATION 0x09
#define USB_GET_INTERFACE     0x0A
#define USB_SET_INTERFACE     0x0B

// CDC class requests
#define SET_LINE_CODING        0x20 // host configures line coding
#define GET_LINE_CODING        0x21 // host reads configured line coding
#define SET_CONTROL_LINE_STATE 0x22 // generates RS-232/V.24 style control signals
#define SEND_BREAK             0x23 // send break

#define USB_REQ_TYP_MASK  0x60
#define USB_REQ_TYP_STD   0x00
#define USB_REQ_TYP_CLASS 0x20

// Line coding: 115200 baud, 1 stop bit, no parity, 8 data bits (only stored)
static uint8_t CDC_lineCoding[7] = {0x00, 0xC2, 0x01, 0x00, 0, 0, 8};

static volatile uint8_t  USB_config;    // configuration selected by host (0: none)
static uint8_t           USB_setupReq;  // request of the current control transfer
static uint8_t           USB_setupTyp;  // request type of the current control transfer
static uint8_t           USB_addr;      // address to set after the status stage

// Control transfer state
enum { CTRL_IDLE, CTRL_DATA_IN, CTRL_DATA_OUT, CTRL_STATUS_IN };
static uint8_t        CTRL_stage;
static const uint8_t *CTRL_ptr;     // data left to send in the IN data stage
static uint16_t       CTRL_left;
static uint8_t        CTRL_zlp;     // zero length packet needed to end the data stage
static uint8_t        CTRL_buf[2];  // small replies built in RAM

static uint8_t           CDC_readBuf[EP2_SIZE]; // last packet received from the host
static uint8_t           CDC_writeBuf[EP2_SIZE];
static volatile uint8_t  CDC_readCount;    // bytes left in CDC_readBuf
static volatile uint8_t  CDC_readPtr;      // next byte to read
static uint8_t           CDC_writePtr;     // bytes waiting in CDC_writeBuf
static volatile uint8_t  CDC_writeBusy;    // EP2 IN transfer in progress

// ===================================================================================
// Front End Functions
// ===================================================================================

// Check if the host has configured the device
uint8_t CDC_ready(void)
{
    return USB_config != 0;
}

// Number of received bytes waiting to be read
uint8_t CDC_available(void)
{
    return CDC_readCount;
}

// Read one received byte (call only if CDC_available() > 0)
char CDC_read(void)
{
    char c = CDC_readBuf[CDC_readPtr++];
    if (--CDC_readCount == 0) EP_setRX(2, USBD_EP_RX_ACK); // buffer empty: accept the next packet
    return c;
}

// Wait for the previous upload to finish; give up after ~20ms (port not open on host)
static uint8_t CDC_waitWrite(void)
{
    for (uint16_t i = 0; CDC_writeBusy; i++)
    {
        if (i >= 20000 || !USB_config) return 0;
        DLY_us(1);
    }
    return 1;
}

// Send the buffered characters to the host
void CDC_flush(void)
{
    if (!CDC_writePtr || !USB_config || !CDC_waitWrite())
    {
        CDC_writePtr = 0; // nothing to send, or nobody listening: drop
        return;
    }
    CDC_writeBusy = 1;
    PMA_write(EP2_TX_ADDR, CDC_writeBuf, CDC_writePtr);
    BT_COUNT_TX(2) = CDC_writePtr;
    EP_setTX(2, USBD_EP_TX_ACK);
    CDC_writePtr = 0;
}

// Write one character to the send buffer
void CDC_write(char c)
{
    if (!CDC_waitWrite()) return;
    CDC_writeBuf[CDC_writePtr++] = c;
    if (CDC_writePtr == EP2_SIZE) CDC_flush(); // flush if buffer full
}

// Write string to the send buffer
void CDC_print(const char *str)
{
    while (*str) CDC_write(*str++);
}

// Write string plus newline and flush
void CDC_println(const char *str)
{
    CDC_print(str);
    CDC_write('\n');
    CDC_flush();
}

// ===================================================================================
// Init and Endpoint Setup
// ===================================================================================

// Reset all endpoints after a USB bus reset (hardware clears the endpoint registers)
static void USB_EP_init(void)
{
    USBD->BTABLE = 0;
    BT_ADDR_RX(0) = EP0_RX_ADDR;
    BT_COUNT_RX(0) = RX_COUNT_64;
    BT_ADDR_TX(0) = EP0_TX_ADDR;
    BT_COUNT_TX(0) = 0;
    BT_ADDR_TX(1) = EP1_TX_ADDR;
    BT_COUNT_TX(1) = 0;
    BT_ADDR_RX(2) = EP2_RX_ADDR;
    BT_COUNT_RX(2) = RX_COUNT_64;
    BT_ADDR_TX(2) = EP2_TX_ADDR;
    BT_COUNT_TX(2) = 0;

    EP_config(0, USBD_EP_CONTROL, USBD_EP_TX_NAK, USBD_EP_RX_ACK);
    EP_config(1, USBD_EP_INTERRUPT, USBD_EP_TX_DIS, USBD_EP_RX_DIS); // enabled by SET_CONFIGURATION
    EP_config(2, USBD_EP_BULK, USBD_EP_TX_DIS, USBD_EP_RX_DIS);

    USBD->DADDR = USBD_DADDR_EF; // enable function, address 0
    USB_config = 0;
    CTRL_stage = CTRL_IDLE;
    CDC_readCount = 0;
    CDC_writePtr = 0;
    CDC_writeBusy = 0;
}

// Enable (or disable) the CDC data endpoints
static void CDC_EP_init(uint8_t enable)
{
    EP_config(1, USBD_EP_INTERRUPT, enable ? USBD_EP_TX_NAK : USBD_EP_TX_DIS, USBD_EP_RX_DIS);
    EP_config(2, USBD_EP_BULK, enable ? USBD_EP_TX_NAK : USBD_EP_TX_DIS, enable ? USBD_EP_RX_ACK : USBD_EP_RX_DIS);
    CDC_readCount = 0;
    CDC_writePtr = 0;
    CDC_writeBusy = 0;
}

// Init USB and start enumeration
void CDC_init(void)
{
    RCC->CFGR0 = (RCC->CFGR0 & ~0x00C00000) | USB_PRESCALER; // USB clock = 48MHz
    RCC->APB1PCENR |= RCC_USBEN;                             // enable USBD clock

    // Disconnect for 20ms so the host notices a new device after a reset
    EXTEN->EXTEN_CTR &= ~EXTEN_USBD_PU_EN;
    GPIOA->OUTDR &= ~((1 << 11) | (1 << 12));
    GPIOA->CFGHR = (GPIOA->CFGHR & ~0x000FF000) | 0x00033000; // PA11/PA12 output low
    DLY_ms(20);
    GPIOA->CFGHR = (GPIOA->CFGHR & ~0x000FF000) | 0x00044000; // PA11/PA12 floating input

    USBD->CNTR = USBD_CNTR_FRES; // power up, hold USB reset
    DLY_us(10);
    USBD->CNTR = 0;
    USBD->ISTR = 0;
    USB_EP_init();
    USBD->CNTR = USBD_CNTR_CTRM | USBD_CNTR_RESETM; // interrupts: transfer done, bus reset
    NVIC_EnableIRQ(USB_LP_CAN1_RX0_IRQn);
    EXTEN->EXTEN_CTR |= EXTEN_USBD_PU_EN; // internal 1.5k pull-up on D+: connect
}

// ===================================================================================
// Endpoint 0 (Control) Handlers
// ===================================================================================

// Send the next part of the IN data stage
static void CTRL_sendChunk(void)
{
    uint8_t len = CTRL_left > EP0_SIZE ? EP0_SIZE : CTRL_left;
    PMA_write(EP0_TX_ADDR, CTRL_ptr, len);
    CTRL_ptr += len;
    CTRL_left -= len;
    BT_COUNT_TX(0) = len;
    EP_setTX(0, USBD_EP_TX_ACK);
}

// Handle standard requests; return data length (data at CTRL_ptr) or -1 to stall
static int16_t USB_standardRequest(uint16_t wValue, uint16_t wIndex)
{
    switch (USB_setupReq)
    {
        case USB_GET_DESCRIPTOR:
            switch (wValue >> 8)
            {
                case 0x01: CTRL_ptr = DevDescr; return sizeof(DevDescr);
                case 0x02: CTRL_ptr = CfgDescr; return sizeof(CfgDescr);
                case 0x03:
                    if ((wValue & 0xFF) >= sizeof(StrDescr) / sizeof(StrDescr[0])) return -1;
                    CTRL_ptr = (const uint8_t *)StrDescr[wValue & 0xFF];
                    return CTRL_ptr[0];
                default: return -1; // unsupported descriptor
            }

        case USB_SET_ADDRESS:
            USB_addr = wValue & 0x7F; // set after the status stage
            return 0;

        case USB_GET_CONFIGURATION:
            CTRL_buf[0] = USB_config;
            CTRL_ptr = CTRL_buf;
            return 1;

        case USB_SET_CONFIGURATION:
            USB_config = wValue & 0xFF;
            CDC_EP_init(USB_config != 0);
            return 0;

        case USB_GET_INTERFACE:
            CTRL_buf[0] = 0;
            CTRL_ptr = CTRL_buf;
            return 1;

        case USB_SET_INTERFACE:
            return 0;

        case USB_GET_STATUS:
            CTRL_buf[0] = 0;
            CTRL_buf[1] = 0;
            CTRL_ptr = CTRL_buf;
            return 2;

        case USB_CLEAR_FEATURE: // clear endpoint halt: reset data toggle
            if ((USB_setupTyp & 0x1F) != 0x02) return -1;
            switch (wIndex & 0xFF)
            {
                case 0x81: EP_config(1, USBD_EP_INTERRUPT, USBD_EP_TX_NAK, USBD_EP_RX_DIS); break;
                case 0x82:
                case 0x02: CDC_EP_init(1); break;
                default: return -1;
            }
            return 0;

        default:
            return -1; // unsupported request (incl. SET_FEATURE: no remote wakeup)
    }
}

// Handle CDC class requests; return data length (data at CTRL_ptr) or -1 to stall
static int16_t CDC_classRequest(void)
{
    switch (USB_setupReq)
    {
        case GET_LINE_CODING:
            CTRL_ptr = CDC_lineCoding;
            return sizeof(CDC_lineCoding);
        case SET_LINE_CODING:        // data follows in the OUT stage
        case SET_CONTROL_LINE_STATE: // DTR/RTS: not used
        case SEND_BREAK:
            return 0;
        default:
            return -1;
    }
}

// SETUP packet received on EP0
static void USB_EP0_SETUP(void)
{
    uint8_t setup[8];
    PMA_read(EP0_RX_ADDR, setup, 8);
    USB_setupTyp = setup[0];
    USB_setupReq = setup[1];
    uint16_t wValue = setup[2] | (setup[3] << 8);
    uint16_t wIndex = setup[4] | (setup[5] << 8);
    uint16_t wLength = setup[6] | (setup[7] << 8);

    int16_t len;
    if ((USB_setupTyp & USB_REQ_TYP_MASK) == USB_REQ_TYP_STD) len = USB_standardRequest(wValue, wIndex);
    else if ((USB_setupTyp & USB_REQ_TYP_MASK) == USB_REQ_TYP_CLASS) len = CDC_classRequest();
    else len = -1;

    if (len < 0) // unsupported: stall
    {
        CTRL_stage = CTRL_IDLE;
        EP_setTX(0, USBD_EP_TX_STALL);
        EP_setRX(0, USBD_EP_RX_STALL);
    }
    else if (USB_setupTyp & 0x80) // device to host: IN data stage, then status OUT
    {
        if (len > wLength) len = wLength;
        CTRL_left = len;
        CTRL_zlp = len && len < wLength && len % EP0_SIZE == 0;
        CTRL_stage = CTRL_DATA_IN;
        CTRL_sendChunk();
        EP_setRX(0, USBD_EP_RX_ACK); // accept the status OUT
    }
    else if (wLength) // host to device with data (SET_LINE_CODING): OUT data stage
    {
        CTRL_stage = CTRL_DATA_OUT;
        EP_setTX(0, USBD_EP_TX_NAK);
        EP_setRX(0, USBD_EP_RX_ACK);
    }
    else // no data: zero length status IN
    {
        CTRL_stage = CTRL_STATUS_IN;
        BT_COUNT_TX(0) = 0;
        EP_setTX(0, USBD_EP_TX_ACK);
    }
}

// IN transfer on EP0 completed
static void USB_EP0_IN(void)
{
    if (CTRL_stage == CTRL_DATA_IN)
    {
        if (CTRL_left || CTRL_zlp) // next part, or the zero length end
        {
            if (!CTRL_left) CTRL_zlp = 0;
            CTRL_sendChunk();
        }
        return; // else wait for the status OUT
    }
    if (CTRL_stage == CTRL_STATUS_IN)
    {
        if ((USB_setupTyp & USB_REQ_TYP_MASK) == USB_REQ_TYP_STD && USB_setupReq == USB_SET_ADDRESS)
            USBD->DADDR = USBD_DADDR_EF | USB_addr;
        CTRL_stage = CTRL_IDLE;
    }
}

// OUT transfer on EP0 completed (data stage of SET_LINE_CODING or status stage)
static void USB_EP0_OUT(void)
{
    if (CTRL_stage == CTRL_DATA_OUT)
    {
        uint8_t len = BT_COUNT_RX(0) & 0x3FF;
        if (len > sizeof(CDC_lineCoding)) len = sizeof(CDC_lineCoding);
        if (USB_setupReq == SET_LINE_CODING) PMA_read(EP0_RX_ADDR, CDC_lineCoding, len);
        CTRL_stage = CTRL_STATUS_IN; // zero length status IN
        BT_COUNT_TX(0) = 0;
        EP_setTX(0, USBD_EP_TX_ACK);
    }
    else CTRL_stage = CTRL_IDLE; // status OUT of an IN transfer
    EP_setRX(0, USBD_EP_RX_ACK);
}

// ===================================================================================
// Interrupt Service Routine
// ===================================================================================
void USB_LP_CAN1_RX0_IRQHandler(void) __attribute__((interrupt));
void USB_LP_CAN1_RX0_IRQHandler(void)
{
    uint16_t istr;
    while ((istr = USBD->ISTR) & USBD_ISTR_CTR) // transfer completed
    {
        uint8_t ep = istr & USBD_ISTR_EP_ID;
        uint16_t epr = EPR(ep);

        if (epr & USBD_EP_CTR_TX)
        {
            EP_clearCTR_TX(ep);
            if (ep == 0) USB_EP0_IN();
            else if (ep == 2) CDC_writeBusy = 0; // upload done (hardware NAKs further INs)
        }

        if (epr & USBD_EP_CTR_RX)
        {
            EP_clearCTR_RX(ep);
            if (ep == 0)
            {
                if (epr & USBD_EP_SETUP) USB_EP0_SETUP();
                else USB_EP0_OUT();
            }
            else if (ep == 2) // data from host (hardware NAKs further OUTs until read)
            {
                uint8_t len = BT_COUNT_RX(2) & 0x3FF;
                if (len > EP2_SIZE) len = EP2_SIZE;
                PMA_read(EP2_RX_ADDR, CDC_readBuf, len);
                CDC_readPtr = 0;
                CDC_readCount = len;
                if (!len) EP_setRX(2, USBD_EP_RX_ACK);
            }
        }
    }

    if (istr & USBD_ISTR_RESET) // bus reset
    {
        USBD->ISTR = (uint16_t)~USBD_ISTR_RESET; // write 0 clears
        USB_EP_init();
    }
}
