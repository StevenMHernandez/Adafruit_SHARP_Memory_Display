/*********************************************************************
This is an Arduino library for our Monochrome SHARP Memory Displays

  Pick one up today in the adafruit shop!
  ------> http://www.adafruit.com/products/1393

These displays use SPI to communicate, 3 pins are required to
interface

Adafruit invests time and resources providing this open source code,
please support Adafruit and open-source hardware by purchasing
products from Adafruit!

Written by Limor Fried/Ladyada  for Adafruit Industries.
BSD license, check license.txt for more information
All text above, and the splash screen must be included in any redistribution
*********************************************************************/

#include "Adafruit_SharpMem.h"
#include "digitalWriteFast.h"

#ifndef _swap_int16_t
#define _swap_int16_t(a, b) { int16_t t = a; a = b; b = t; }
#endif
#ifndef _swap_uint16_t
#define _swap_uint16_t(a, b) { uint16_t t = a; a = b; b = t; }
#endif

/**************************************************************************
    Sharp Memory Display Connector
    -----------------------------------------------------------------------
    Pin   Function        Notes
    ===   ==============  ===============================
      1   VIN             3.3-5.0V (into LDO supply)
      2   3V3             3.3V out
      3   GND
      4   SCLK            Serial Clock
      5   MOSI            Serial Data Input
      6   CS              Serial Chip Select
      9   EXTMODE         COM Inversion Select (Low = SW clock/serial)
      7   EXTCOMIN        External COM Inversion Signal
      8   DISP            Display On(High)/Off(Low)

 **************************************************************************/

#define SHARPMEM_BIT_WRITECMD   (0x80)
#define SHARPMEM_BIT_VCOM       (0x40)
#define SHARPMEM_BIT_CLEAR      (0x20)
#define TOGGLE_VCOM             do { _sharpmem_vcom = _sharpmem_vcom ? 0x00 : SHARPMEM_BIT_VCOM; } while(0);

byte *sharpmem_buffer;

/* ************* */
/* CONSTRUCTORS  */
/* ************* */
Adafruit_SharpMem::Adafruit_SharpMem(uint8_t clk, uint8_t mosi, uint8_t ss, uint16_t width, uint16_t height) :
        Adafruit_GFX(width, height) {
    _clk  = clk;
    _mosi = mosi;
    _ss   = ss;
}

boolean Adafruit_SharpMem::begin(void) {
    // Set pin state before direction to make sure they start this way (no glitching)
    digitalWrite(_ss, HIGH);
    digitalWrite(_clk, LOW);
    digitalWrite(_mosi, HIGH);

    pinMode(_ss, OUTPUT);
    pinMode(_clk, OUTPUT);
    pinMode(_mosi, OUTPUT);

#if defined(USE_FAST_PINIO)
    clkport     = portOutputRegister(digitalPinToPort(_clk));
    clkpinmask  = digitalPinToBitMask(_clk);
    dataport    = portOutputRegister(digitalPinToPort(_mosi));
    datapinmask = digitalPinToBitMask(_mosi);
#endif

    // Set the vcom bit to a defined state
    _sharpmem_vcom = SHARPMEM_BIT_VCOM;


    sharpmem_buffer = (byte *)malloc((WIDTH * HEIGHT) / 8);

    if (!sharpmem_buffer) return false;

    setRotation(0);

    return true;
}

/* *************** */
/* PRIVATE METHODS */
/* *************** */


/**************************************************************************/
/*!
    @brief  Sends a single byte in pseudo-SPI.
*/
/**************************************************************************/
void Adafruit_SharpMem::sendbyte(uint8_t data)
{
    /* *** CUSTOM CODE *** */
    // digitalWriteFast increased speeds notably. It doesn't, however, work
    // to use digitalWriteFast to set CLK low for some weird reason.
    for (int i = 0; i < 8; ++i) {
        digitalWrite(_clk, LOW);
        if (data & 0x80)
            digitalWrite(_mosi, HIGH);
        else
            digitalWrite(_mosi, LOW);

        digitalWrite(_clk, HIGH);
        data <<= 1;
    }
    digitalWrite(_clk, LOW);
}

void Adafruit_SharpMem::sendbyteLSB(uint8_t data)
{
    /* *** CUSTOM CODE *** */
    // digitalWriteFast increased speeds notably. It doesn't, however, work
    // to use digitalWriteFast to set CLK low for some weird reason.
    for (int i=0; i<8; ++i)
    {
        // Make sure clock starts low
        digitalWrite(_clk, LOW);
        if (data & 0x01)
            digitalWrite(_mosi, HIGH);
        else
            digitalWrite(_mosi, LOW);
        // Clock is active high
        digitalWrite(_clk, HIGH);
        data >>= 1;
    }
    // Make sure clock ends low
    digitalWrite(_clk, LOW);
}

/* ************** */
/* PUBLIC METHODS */
/* ************** */

// 1<<n is a costly operation on AVR -- table usu. smaller & faster
static const uint8_t PROGMEM
set[] = {  1,  2,  4,  8,  16,  32,  64,  128 },
clr[] = { (uint8_t)~1 , (uint8_t)~2 , (uint8_t)~4 , (uint8_t)~8,
(uint8_t)~16, (uint8_t)~32, (uint8_t)~64, (uint8_t)~128 };

/**************************************************************************/
/*!
    @brief Draws a single pixel in image buffer

    @param[in]  x
                The x position (0 based)
    @param[in]  y
                The y position (0 based)
*/
/**************************************************************************/
void Adafruit_SharpMem::drawPixel(int16_t x, int16_t y, uint16_t color)
{
    if((x < 0) || (x >= _width) || (y < 0) || (y >= _height)) return;

    switch(rotation) {
        case 1:
        _swap_int16_t(x, y);
            x = WIDTH  - 1 - x;
            break;
        case 2:
            x = WIDTH  - 1 - x;
            y = HEIGHT - 1 - y;
            break;
        case 3:
        _swap_int16_t(x, y);
            y = HEIGHT - 1 - y;
            break;
    }

    if(color) {
        sharpmem_buffer[(y * WIDTH + x) / 8] |= pgm_read_byte(&set[x & 7]);
    } else {
        sharpmem_buffer[(y * WIDTH + x) / 8] &= pgm_read_byte(&clr[x & 7]);
    }
}

/**************************************************************************/
/*!
    @brief Gets the value (1 or 0) of the specified pixel from the buffer

    @param[in]  x
                The x position (0 based)
    @param[in]  y
                The y position (0 based)

    @return     1 if the pixel is enabled, 0 if disabled
*/
/**************************************************************************/
uint8_t Adafruit_SharpMem::getPixel(uint16_t x, uint16_t y)
{
    if((x >= _width) || (y >= _height)) return 0; // <0 test not needed, unsigned

    switch(rotation) {
        case 1:
        _swap_uint16_t(x, y);
            x = WIDTH  - 1 - x;
            break;
        case 2:
            x = WIDTH  - 1 - x;
            y = HEIGHT - 1 - y;
            break;
        case 3:
        _swap_uint16_t(x, y);
            y = HEIGHT - 1 - y;
            break;
    }

    return sharpmem_buffer[(y * WIDTH + x) / 8] & pgm_read_byte(&set[x & 7]) ? 1 : 0;
}

/**************************************************************************/
/*!
    @brief Clears the screen
*/
/**************************************************************************/
void Adafruit_SharpMem::clearDisplay()
{
    memset(sharpmem_buffer, 0xff, (WIDTH * HEIGHT) / 8);
    // Send the clear screen command rather than doing a HW refresh (quicker)
    digitalWriteFast(_ss, HIGH);
    sendbyte(_sharpmem_vcom | SHARPMEM_BIT_CLEAR);
    sendbyteLSB(0x00);
    TOGGLE_VCOM;
    digitalWriteFast(_ss, LOW);
}

/**************************************************************************/
/*!
    @brief Renders the contents of the pixel buffer on the LCD
*/
/**************************************************************************/
void Adafruit_SharpMem::refresh()
{
    /* CUSTOM CODE */
    uint8_t current_line;

    digitalWriteFast(_ss, HIGH);
    // send write cmd
    sendbyte(SHARPMEM_BIT_WRITECMD | _sharpmem_vcom);
    TOGGLE_VCOM;

    // send data
    for (int i = 0; i < HEIGHT; ++i) {
        // send line number
        current_line = i+1;
        sendbyteLSB(current_line);

        for (int j = 0; j < WIDTH / 8; ++j) {
            uint8_t data = sharpmem_buffer[( WIDTH / 8 ) * i + j];
            sendbyteLSB(data);
        }
        sendbyte(0x00);  // end-of-line trailing byte
    }

    // send rest of trailing 16 bits
    sendbyte(0x00);

    digitalWriteFast(_ss, LOW);
}
