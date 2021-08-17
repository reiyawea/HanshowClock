#include <msp430.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "calendar.h"
#include "font.h"
#include "epd.h"
#include "flash.h"
#include "a7105.h"

time_t time;
lunar_t lunar;
uint32_t epoch;
uint8_t buffer[21];
uint16_t vbat;

uint8_t tasks;
#define DISP_MAKE 0x01
#define DISP_FULL_UPDATE 0x02
#define DISP_WAIT 0x04
#define RF_WAKE_UP 0x08
#define RF_RECV_START 0x10
#define RF_RECV_WAIT 0x20
#define RF_WOR_WAIT 0x40
#define RF_ENTER_WOR 0x80

#define TIME_POS_X 0
#define TIME_POS_Y 0
#define TIME_SIZE_X 104
#define TIME_SIZE_Y 53
#define DATE_POS_X 104
#define DATE_POS_Y 8
void makeDisplay();

static void Init(void);
void delay(uint16_t);
uint16_t ADC_getVbat();

int main(void)
{
  Init();
  A7105_Init();
  flash_init();
  flash_sleep();
  tasks = DISP_MAKE | DISP_FULL_UPDATE | RF_ENTER_WOR;
  vbat = ADC_getVbat();
  while (1)
  {
    if (!tasks)
    {
      BCSCTL2 |= SELM_3; // MCLK = VLO
      __bis_SR_register(LPM3_bits);
    }
    BCSCTL2 &= ~SELM_3; // MCLK = DCO
    if ((tasks & ~(RF_WOR_WAIT | DISP_FULL_UPDATE)) == DISP_WAIT)
    {
      BCSCTL1 = CALBC1_1MHZ;
      DCOCTL = CALDCO_1MHZ;
      BCSCTL2 |= DIVM_3;
    }
    else
    {
      BCSCTL2 &= ~DIVM_3;
      BCSCTL1 = CALBC1_8MHZ;
      DCOCTL = CALDCO_8MHZ;
    }
    if (tasks & RF_WAKE_UP)
    {
      WDTCTL = WDT_ARST_1000;
      A7105_WriteReg(A7105_0C_GPIO2_PIN_II, A7105_IO_WTR | A7105_GIOxOE);
      A7105_Strobe(A7105_PLL);
      while ((A7105_ReadReg(A7105_00_MODE) & 0x1f) != 0x1c)
        ;
      TACTL |= TACLR;
      tasks &= ~RF_WAKE_UP;
      tasks |= RF_RECV_START;
    }
    if (tasks & RF_RECV_START)
    {
      A7105_Strobe(A7105_RX);
      while (!rfio2)
        ;
      tasks &= ~RF_RECV_START;
      tasks |= RF_RECV_WAIT;
    }
    if (tasks & RF_RECV_WAIT)
    {
      if (!rfio2)
      {
        if (A7105_ReadReg(A7105_00_MODE) & A7105_MASK_CRCF)
        {
          tasks |= RF_RECV_START;
        }
        else
        {
          A7105_ReadData(buffer, 21);
          if (buffer[0] == 250)
          {
            epoch = buffer[4];
            epoch <<= 8;
            epoch |= buffer[3];
            epoch <<= 8;
            epoch |= buffer[2];
            epoch <<= 8;
            epoch |= buffer[1];
            A7105_Strobe(A7105_SLEEP);
            WDTCTL = WDT_ARST_1000 + WDTHOLD;
            tasks |= RF_WOR_WAIT;
            if (!(tasks & (DISP_MAKE | DISP_WAIT)))
              tasks |= DISP_MAKE | DISP_FULL_UPDATE;
          }
          else
          {
            tasks |= RF_RECV_START;
          }
        }
        tasks &= ~RF_RECV_WAIT;
      }
    }
    if (tasks & RF_ENTER_WOR)
    {
      WDTCTL = WDT_ARST_250;
      A7105_EnterWOR();
      P1IFG = 0;
      P1IE |= BIT0;
      WDTCTL = WDT_ARST_1000 + WDTHOLD; // stop watchdog timer
      tasks &= ~RF_ENTER_WOR;
    }
    if (tasks & DISP_MAKE)
    {
      epochToDateTime(epoch + 8 * 3600, &time);
      if (time.tm_min == 0)
        tasks |= DISP_FULL_UPDATE;
      solarToLunar(&time, &lunar);
      EPD_init();
      makeDisplay();
      if (tasks & DISP_FULL_UPDATE)
      {
        vbat = 0x3FF;
        ADC10CTL0 |= REFON;
        ADC10CTL0 |= ENC;
      }
      EPD_dispUpdate(tasks & DISP_FULL_UPDATE);
      tasks &= ~DISP_MAKE;
      tasks |= DISP_WAIT;
    }
    if (tasks & DISP_WAIT)
    {
      if (tasks & DISP_FULL_UPDATE)
      {
        uint16_t res = ADC_getVbat();
        if (res < vbat)
          vbat = res;
      }
      if (!EPD_isBusy())
      {
        BCSCTL2 &= ~DIVM_3;
        ADC10CTL0 &= ~ENC;
        ADC10CTL0 &= ~REFON;
        EPD_sleep();
        tasks &= ~(DISP_WAIT | DISP_FULL_UPDATE);
        if (tasks & RF_WOR_WAIT)
        {
          tasks &= ~RF_WOR_WAIT;
          tasks |= RF_ENTER_WOR;
        }
      }
    }
  }
}

void Init(void)
{
  WDTCTL = WDT_ARST_1000 + WDTHOLD; // stop watchdog timer
  //SYS CLK
  BCSCTL1 = CALBC1_8MHZ;
  DCOCTL = CALDCO_8MHZ;
  //LFXT1 for ACLK

  BCSCTL3 = LFXT1S_0 | XCAP_3;    // ACLK=LFXT1= 32768 crystal + 12.5pF
  BCSCTL2 = SELM_0 | DIVM_0;      // MCLK = DCO, SMCLK=DCO
  TACTL = TASSEL_1 | MC_2 | TAIE; // TA=ACLK

  //GPIO for EPD
  P2OUT &= ~(BIT3 | BIT4);
  P2DIR |= BIT3 | BIT4;
  P3OUT |= BIT7; //unknown pin to epd
  P3OUT &= ~(BIT1 | BIT4 | BIT5 | BIT6);
  P3DIR |= BIT1 | BIT4 | BIT5 | BIT6 | BIT7;

  ADC10CTL1 = INCH_11;
  ADC10CTL0 = SREF_1 + ADC10SHT_3 + ADC10SR + REFON + ADC10ON + ENC;

  P1IES = 0;
  IFG1 &= ~OFIFG;
  __bis_SR_register(GIE);
}

void delay(uint16_t nClk)
{
  TACCR0 = TAR + nClk * 32;
  TACCTL0 &= ~CCIFG;
  while (!(TACCTL0 & CCIFG))
    ;
}

void makeDisplay()
{
  uint8_t month;
  uint16_t y;
  const uint8_t *batt;

  /*----- Clear EPD RAM -----*/
  EPD_draw(0, 0, EPD_WIDTH, EPD_HEIGHT, NULL);
  /*----- Draw Hour -----*/
  y = TIME_POS_Y + time.tm_min % 16;
  if (time.tm_hour >= 10)
    EPD_draw(TIME_POS_X, y, TIME_SIZE_X, TIME_SIZE_Y, Font53x104[time.tm_hour / 10]);
  y += TIME_SIZE_Y;
  EPD_draw(TIME_POS_X, y, TIME_SIZE_X, TIME_SIZE_Y, Font53x104[time.tm_hour % 10]);
  y += TIME_SIZE_Y;
  /*----- Draw Colon -----*/
  EPD_draw(TIME_POS_X, y, 104, 16, Font16x104);
  y += 16;
  /*----- Draw Minute -----*/
  EPD_draw(TIME_POS_X, y, TIME_SIZE_X, TIME_SIZE_Y, Font53x104[time.tm_min / 10]);
  y += TIME_SIZE_Y;
  EPD_draw(TIME_POS_X, y, TIME_SIZE_X, TIME_SIZE_Y, Font53x104[time.tm_min % 10]);
  /*----- Draw Month -----*/
  y = DATE_POS_Y;
  month = time.tm_mon + 1;
  if (month >= 10)
    EPD_draw(DATE_POS_X, y, 16, 8, Font8x16[month / 10]);
  y += 8;
  EPD_draw(DATE_POS_X, y, 16, 8, Font8x16[month % 10]);
  y += 8;
  EPD_draw(DATE_POS_X, y, 16, 16, Font16x16[Font16x16_MONTH]); //yue4
  y += 16;
  /*----- Draw Day -----*/
  if (time.tm_mday >= 10)
    EPD_draw(DATE_POS_X, y, 16, 8, Font8x16[time.tm_mday / 10]);
  y += 8;
  EPD_draw(DATE_POS_X, y, 16, 8, Font8x16[time.tm_mday % 10]);
  y += 8;
  EPD_draw(DATE_POS_X, y, 16, 16, Font16x16[Font16x16_DAY]); //ri4
  y += 16 + 8;
  /*----- Draw Weekday -----*/
  EPD_draw(DATE_POS_X, y, 16, 16, Font16x16[Font16x16_WEEK]); //xing1
  y += 16;
  EPD_draw(DATE_POS_X, y, 16, 16, Font16x16[Font16x16_WEEK + 1]); //qi1
  y += 16;
  EPD_draw(DATE_POS_X, y, 16, 16, Font16x16[time.tm_wday]);
  y += 16 + 8;
  /*----- Draw Lunar month -----*/
  if (lunar.reserved)
  {
    EPD_draw(DATE_POS_X, y, 16, 16, Font16x16[Font16x16_LEAP]); //run
    y += 16;
  }
  if (lunar.month == 1)
  {
    EPD_draw(DATE_POS_X, y, 16, 16, Font16x16[Font16x16_JAN]); //zheng1
  }
  else if (lunar.month == 11)
  {
    EPD_draw(DATE_POS_X, y, 16, 16, Font16x16[10]); //shi2
    y += 16;
    EPD_draw(DATE_POS_X, y, 16, 16, Font16x16[1]); //yi1
  }
  else
  {
    EPD_draw(DATE_POS_X, y, 16, 16, Font16x16[lunar.month]);
  }
  y += 16;
  EPD_draw(DATE_POS_X, y, 16, 16, Font16x16[Font16x16_MONTH]); //yue4
  y += 16;
  /*----- Draw Lunar day -----*/
  if (lunar.day <= 10)
  {
    EPD_draw(DATE_POS_X, y, 16, 16, Font16x16[Font16x16_EARLY]); //chu1
    y += 16;
    EPD_draw(DATE_POS_X, y, 16, 16, Font16x16[lunar.day]);
  }
  else if (lunar.day <= 19)
  {
    EPD_draw(DATE_POS_X, y, 16, 16, Font16x16[10]); //shi2
    y += 16;
    EPD_draw(DATE_POS_X, y, 16, 16, Font16x16[lunar.day % 10]);
  }
  else if (lunar.day == 20)
  {
    EPD_draw(DATE_POS_X, y, 16, 16, Font16x16[2]); //er4
    y += 16;
    EPD_draw(DATE_POS_X, y, 16, 16, Font16x16[10]); //shi2
  }
  else if (lunar.day <= 29)
  {
    EPD_draw(DATE_POS_X, y, 16, 16, Font16x16[Font16x16_TWENTY]); //nian4
    y += 16;
    EPD_draw(DATE_POS_X, y, 16, 16, Font16x16[lunar.day % 10]);
  }
  else if (lunar.day == 30)
  {
    EPD_draw(DATE_POS_X, y, 16, 16, Font16x16[3]); //san1
    y += 16;
    EPD_draw(DATE_POS_X, y, 16, 16, Font16x16[10]); //shi2
  }
  y = EPD_HEIGHT - 13;
  /*----- Select Battery Icon -----*/
  if (vbat < 597)//1.75V
  {
    batt = Batt13x8[0];
  }
  else if (vbat < 768)//2.25V
  {
    batt = Batt13x8[1];
  }
  else if (vbat < 938)//2.75V
  {
    batt = Batt13x8[2];
  }
  else
  {
    batt = Batt13x8[3];
  }
  /*----- Draw Battery Icon -----*/
  EPD_draw(DATE_POS_X + 8, y, 8, 13, batt);
  y += 16;
}

uint16_t ADC_getVbat()
{
  uint16_t res[3], i;
  for (i = 0; i < 3; i++)
  {
    ADC10CTL0 |= ADC10SC;
    while (!(ADC10CTL0 & ADC10IFG))
      ;
    ADC10CTL0 &= ~ADC10IFG;
    res[i] = ADC10MEM;
  }

  if (res[0] > res[1])
  {
    res[0] ^= res[1];
    res[1] ^= res[0];
    res[0] ^= res[1];
  }
  if (res[1] > res[2])
  {
    res[1] ^= res[2];
    res[2] ^= res[1];
    res[1] ^= res[2];
  }
  if (res[0] > res[1])
  {
    return res[0];
  }else{
    return res[1];
  }
}

#pragma vector = TIMER0_A1_VECTOR
__interrupt void Timer_A1(void)
{
  uint8_t sec;

  if (TAIV == 0x0A)
  {
    epoch += 2;
    sec = epoch % 60;
    if (sec == 0 || sec == 1)
    {
      tasks |= DISP_MAKE;
      __bic_SR_register_on_exit(LPM3_bits);
    }
  }
}

#pragma vector = PORT1_VECTOR
__interrupt void Port_10(void)
{
  P1IE &= ~BIT0;
  P1IFG &= ~BIT0;
  tasks |= RF_WAKE_UP;
  __bic_SR_register_on_exit(LPM3_bits);
}
