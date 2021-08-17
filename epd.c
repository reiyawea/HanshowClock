#include <msp430.h>
#include <stdint.h>
#include <stdbool.h>
#include "epd.h"
#include "lut.h"

#define EPD_W21_CK_LOW P2OUT &= ~BIT3
#define EPD_W21_CK_HIGH P2OUT |= BIT3
#define EPD_W21_DA_LOW P2OUT &= ~BIT4
#define EPD_W21_DA_HIGH P2OUT |= BIT4
#define EPD_W21_CS_LOW P3OUT &= ~BIT4
#define EPD_W21_CS_HIGH P3OUT |= BIT4
#define EPD_W21_CS_LOW P3OUT &= ~BIT4
#define EPD_W21_CS_HIGH P3OUT |= BIT4
#define EPD_W21_DC_LOW P3OUT &= ~BIT5
#define EPD_W21_DC_HIGH P3OUT |= BIT5
#define EPD_W21_DC_LOW P3OUT &= ~BIT5
#define EPD_W21_DC_HIGH P3OUT |= BIT5
#define EPD_W21_RST_LOW P3OUT &= ~BIT6
#define EPD_W21_RST_HIGH P3OUT |= BIT6
#define EPD_W21_BUSY (BIT5 & P2IN)

static void EPD_writeByte(uint8_t);
static void EPD_W21_WriteCMD(uint8_t);
static void EPD_W21_WriteDATA(uint8_t);
static void EPD_W21_WriteCMD1DAT(uint8_t cmd, uint8_t);
static void EPD_W21_WriteCMD2DAT(uint8_t cmd, uint8_t, uint8_t);
extern void delay(uint16_t);

void EPD_init()
{
  EPD_W21_CS_HIGH;
  EPD_W21_CK_LOW;
  EPD_W21_RST_LOW;
  delay(120);
  EPD_W21_RST_HIGH;
  delay(120);
  EPD_busyWait();

  EPD_W21_CS_LOW;
  //    EPD_W21_WriteCMD(0x12);//SWRESET
  //    EPD_busyWait();
  EPD_W21_WriteCMD2DAT(0x01, 0xf9, 0x00); //Driver Output Control. set # of gate = 232
  EPD_W21_WriteCMD1DAT(0x3a, 0x06);       //Set dummy line period=06h
  EPD_W21_WriteCMD1DAT(0x3b, 0x0b);       //Set Gate line width=0Bh
  EPD_W21_WriteCMD1DAT(0x11, 0x06);       //Data Entry mode setting. bit0=X, bit1=Y[1=inc/0=dec], bit2=AM[0:X/1:Y]
  EPD_W21_WriteCMD2DAT(0x44, 0x00, 0x0f); //Set RAM X-address Start/End position =0~15*8=128>122
  EPD_W21_WriteCMD2DAT(0x45, 0x00, 0xf9); //Set RAM Y-address Start/End position =0~249 =250=250
  EPD_W21_WriteCMD1DAT(0x2c, 0xa0);       //Write VCOM register
  EPD_W21_WriteCMD1DAT(0x3c, 0x33);       //Border Waveform Control
  EPD_W21_WriteCMD1DAT(0x21, 0x0D);       //Display Update Control 1. Option for Display Update Bypass Option used for Pattern Display,  which is used for display the RAM content into the Display
  EPD_W21_CS_HIGH;
  EPD_busyWait();
}

void EPD_dispUpdate(uint8_t full)
{
  const uint8_t *lut;
  uint16_t i;

  EPD_W21_CS_LOW;
  if (full)
  {
    lut = LUT;
  }
  else
  {
    lut = LUT_partial;
  }
  EPD_W21_WriteCMD(0x32); //Write LUT register 29 bytes
  EPD_W21_DC_HIGH;
  for (i = 29; i; i--)
  {
    EPD_writeByte(*lut++);
  }
  EPD_W21_WriteCMD1DAT(0x22, 0xc7); //Display Update Control 2 + Setting for LUT from MCU
  EPD_W21_WriteCMD(0x20);           //Master Activation
  EPD_W21_CS_HIGH;
}

void EPD_sleep(void)
{
  EPD_W21_CS_LOW;
  EPD_W21_WriteCMD1DAT(0x10, 0x01); //Deep sleep mode
  EPD_W21_CS_HIGH;
}

static void epd_setWindow(uint8_t x_start, uint8_t y_start, uint8_t x_end, uint8_t y_end)
{
  EPD_W21_WriteCMD2DAT(0x44, x_start >> 3, (x_end + 7) >> 3); // x point must be the multiple of 8 or the last 3 bits will be ignored
  EPD_W21_WriteCMD2DAT(0x45, y_start, y_end);
}
static void epd_setCursor(uint8_t x, uint8_t y)
{
  EPD_W21_WriteCMD1DAT(0x4E, x >> 3); // x point must be the multiple of 8 or the last 3 bits will be ignored
  EPD_W21_WriteCMD1DAT(0x4F, y);
}
void EPD_draw(uint8_t x, uint8_t y, uint8_t xsize, uint8_t ysize, const uint8_t *dat)
{
  uint16_t length = (xsize + 7) / 8 * ysize;
  EPD_W21_CS_LOW;
  epd_setWindow(x, y, x + xsize - 1, y + ysize - 1);
  epd_setCursor(x + xsize - 1, y);
  EPD_W21_WriteCMD(0x24); //write ram
  EPD_W21_DC_HIGH;
  if (dat)
  {
    do
    {
      EPD_writeByte(*dat++);
    } while (--length);
  }
  else
  {
    do
    {
      EPD_writeByte(0);
    } while (--length);
  }
  EPD_W21_CS_HIGH;
}

void EPD_busyWait()
{
  delay(10);
  while (EPD_W21_BUSY)
    ;
}

bool EPD_isBusy()
{
  return EPD_W21_BUSY;
}

void EPD_writeByte(uint8_t sdbyte)
{
  uint16_t i;
  for (i = 8; i; i--)
  {
    if (sdbyte & 0x80)
    {
      EPD_W21_DA_HIGH;
    }
    else
    {
      EPD_W21_DA_LOW;
    }
    sdbyte <<= 1;
    EPD_W21_CK_HIGH;
    asm(" nop");
    EPD_W21_CK_LOW;
  }
}

static void EPD_W21_WriteCMD(unsigned char command)
{
  EPD_W21_DC_LOW;
  EPD_writeByte(command);
}
static void EPD_W21_WriteDATA(unsigned char dat)
{
  EPD_W21_DC_HIGH;
  EPD_writeByte(dat);
}

static void EPD_W21_WriteCMD1DAT(uint8_t cmd, uint8_t data)
{
  EPD_W21_WriteCMD(cmd);
  EPD_W21_WriteDATA(data);
}
static void EPD_W21_WriteCMD2DAT(uint8_t cmd, uint8_t data, uint8_t data2)
{
  EPD_W21_WriteCMD(cmd);
  EPD_W21_WriteDATA(data);
  EPD_writeByte(data2);
}
