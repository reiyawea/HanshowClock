/*
  This project is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Deviation is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Deviation.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <msp430.h>
#include <stdint.h>
#include "a7105.h"

#define rfcslow P1OUT &= ~BIT3
#define rfcshigh P1OUT |= BIT3

uint8_t RF_SPI_TxRx(uint8_t dat);
static int A7105_calibrate_IF();
static int A7105_calibrate_VCB();
extern void delay(uint16_t);

void A7105_Init()
{
  //GPIO for RF SPI
  P1DIR &= ~(BIT1 + BIT2 + BIT4);
  P1SEL |= BIT1 + BIT2 + BIT4;
  P1SEL2 |= BIT1 + BIT2 + BIT4;
  //GPIO for RF CS
  P1OUT |= BIT3;
  P1DIR |= BIT3;

  UCA0CTL0 = UCCKPH + UCMSB + UCMST + UCSYNC; // 3-pin, 8-bit SPI master
  UCA0CTL1 = UCSSEL_2 + UCSWRST;              // SMCLK
  UCA0BR0 = 0x02;                             // /2
  UCA0BR1 = 0;                                //
  UCA0MCTL = 0;                               // No modulation
  UCA0CTL1 &= ~UCSWRST;                       // **Initialize USCI state machine**

  delay(100);
  A7105_WriteReg(A7105_00_MODE, 0x00); // this writes a null value to register 0x00, which triggers the reset
  A7105_WriteReg(A7105_0B_GPIO1_PIN_I, A7105_IO_SDO | A7105_GIOxOE);
  A7105_Strobe(A7105_PLL);
  while ((A7105_ReadReg(A7105_00_MODE) & 0x1f) != 0x1c)
    ;
  A7105_calibrate_IF();
  A7105_WriteReg(A7105_24_VCO_CURCAL, 0x13);
  A7105_calibrate_VCB();
  A7105_Strobe(A7105_STANDBY);
  A7105_WriteReg(A7105_01_MODE_CONTROL, A7105_AIF | A7105_FMS);
  A7105_WriteReg(A7105_03_FIFOI, 20);
  A7105_WriteID(0xa55a46b9);
  A7105_WriteReg(A7105_0C_GPIO2_PIN_II, A7105_IO_WTR | A7105_GIOxOE);
  A7105_WriteReg(A7105_0E_DATA_RATE, 1);
  A7105_WriteReg(A7105_0F_PLL_I, 161);   //channel
  A7105_WriteReg(A7105_1F_CODE_I, 0x3F); //WHTS=FECS=CRCS=1;
  A7105_WriteReg(A7105_20_CODE_II, 0x17);
  A7105_WriteReg(A7105_21_CODE_III, 233);
  A7105_WriteReg(A7105_19_RX_GAIN_I, 0x80);
  A7105_WriteReg(A7105_1C_RX_GAIN_IV, 0x0A);
  A7105_Strobe(A7105_SLEEP);
}

static int A7105_calibrate_IF()
{
  uint8_t calibration_result;
  uint16_t count;
  //IF Filter Bank Calibration
  // write 001 to this register, chip will then calibrate IF filter bank, and lower the flag when the calibration is complete
  A7105_WriteReg(A7105_02_CALC, A7105_MASK_FBC);
  // if 02h has not cleared within 500ms, give a timeout error and abort.
  count = 0;
  while (A7105_ReadReg(A7105_02_CALC) & A7105_MASK_FBC)
  {
    if (++count >= 60000)
    {
      return -1;
    }
  }
  // read IF calibration status
  calibration_result = A7105_ReadReg(A7105_22_IF_CALIB_I);
  // check to see if auto calibration failure flag is set. If so, give error message and abort
  if (calibration_result & A7105_MASK_FBCF)
  {
    return -2;
  }
  return 0;
}

static int A7105_calibrate_VCB()
{
  uint8_t calibration_result;
  uint16_t count;
  //Initiate VCO bank calibration. register will auto clear when complete
  A7105_WriteReg(A7105_02_CALC, A7105_MASK_VBC);
  // allow 500ms for calibration to complete
  count = 0;
  while (A7105_ReadReg(A7105_02_CALC) & A7105_MASK_VBC)
  {
    // if not complete, issue timeout error and abort
    if (++count >= 60000)
    {
      return -1;
    }
  }
  // if auto calibration fail flag is high, print error and abort
  calibration_result = A7105_ReadReg(A7105_25_VCO_SBCAL_I);
  if (calibration_result & A7105_MASK_VBCF)
  {
    return -2;
  }
  return 0;
}

int A7105_EnterWOR()
{
  uint8_t RCOT = 0, calibration_result;
  uint16_t count;

  A7105_Strobe(A7105_STANDBY);
  A7105_WriteReg(A7105_07_RC_OSC_I, 0xff);
  A7105_WriteReg(A7105_08_RC_OSC_II, 0xc0 | 11);
  do
  {
    RCOT = (RCOT + 0x10) & 0x30;
    A7105_WriteReg(A7105_09_RC_OSC_III, RCOT | A7105_CALW | A7105_RCOSC_E);
    count = 0;
    do
    {
      if (++count >= 6000)
      {
        return -1;
      }
      calibration_result = A7105_ReadReg(A7105_07_RC_OSC_I);
    } while (calibration_result & A7105_MASK_CALR);
  } while (calibration_result < 0x14);
  A7105_WriteReg(A7105_0C_GPIO2_PIN_II, A7105_IO_WAK | A7105_GIOxOE);
  A7105_WriteReg(A7105_01_MODE_CONTROL, A7105_AIF | A7105_FMS | A7105_WORE); // FMS=AIF=1
  A7105_Strobe(A7105_SLEEP);

  return 0;
}

// Transmits the given strobe command. Commands are enumerated in a7105.h and detailed in the documentation
void A7105_Strobe(enum A7105_State state)
{
  // NB: the CS pin must be lowered before and raised after every communication with the chip
  rfcslow;
  RF_SPI_TxRx(state);
  rfcshigh;
}

// Normal registers, essentially everything except the FIFO buffer and the ID register,
// hold only one byte. These two functions therefore transfer only one byte.

void A7105_WriteReg(uint8_t address, uint8_t data)
{
  rfcslow;
  RF_SPI_TxRx(address);
  RF_SPI_TxRx(data);
  rfcshigh;
}
uint8_t A7105_ReadReg(uint8_t address)
{
  uint8_t r;
  rfcslow;
  RF_SPI_TxRx(0x40 | address); // raise the read flag on the address
  r = RF_SPI_TxRx(0xff);
  rfcshigh;
  return r;
}

void A7105_WriteData(uint8_t *dpbuffer, uint8_t len)
{
  A7105_Strobe(A7105_RST_WRPTR); //reset write FIFO PTR
  rfcslow;
  RF_SPI_TxRx(A7105_05_FIFO_DATA); // FIFO DATA register - about to send data to put into FIFO
  for (; len; len--)
  {
    RF_SPI_TxRx(*dpbuffer++);
  }
  rfcshigh;
}

void A7105_ReadData(uint8_t *dpbuffer, uint8_t len)
{

  A7105_Strobe(A7105_RST_RDPTR);
  rfcslow;
  RF_SPI_TxRx(0x40 | A7105_05_FIFO_DATA);
  for (; len; len--)
  {
    *dpbuffer++ = RF_SPI_TxRx(0xff);
  }
  rfcshigh;
}

void A7105_WriteID(uint32_t id)
{
  rfcslow;
  RF_SPI_TxRx(A7105_06_ID_DATA);
  RF_SPI_TxRx((id >> 24) & 0xFF);
  RF_SPI_TxRx((id >> 16) & 0xFF);
  RF_SPI_TxRx((id >> 8) & 0xFF);
  RF_SPI_TxRx(id & 0xFF);
  rfcshigh;
}

uint32_t A7105_ReadID()
{
  uint32_t r;
  rfcslow;
  RF_SPI_TxRx(0x40 | A7105_06_ID_DATA);
  r = RF_SPI_TxRx(0xFF);
  r <<= 8;
  r |= RF_SPI_TxRx(0xFF);
  r <<= 8;
  r |= RF_SPI_TxRx(0xFF);
  r <<= 8;
  r |= RF_SPI_TxRx(0xFF);
  rfcshigh;
  return r;
}

uint8_t RF_SPI_TxRx(uint8_t dat)
{
  UCA0TXBUF = dat;
  while (!(UCA0TXIFG & IFG2))
    ;
  while (!(UCA0RXIFG & IFG2))
    ;
  return UCA0RXBUF;
}
