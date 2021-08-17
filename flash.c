#include <msp430.h>
#include <stdint.h>

#define FLASH_CS_HIGH P3OUT |= BIT0
#define FLASH_CS_LOW P3OUT &= ~BIT0

static uint8_t FLASH_SPI_TxRx(uint8_t dat);

void flash_init()
{
  //GPIO for flash SPI
  P1DIR &= ~(BIT5 + BIT6 + BIT7);
  P1SEL |= BIT5 + BIT6 + BIT7;
  P1SEL2 |= BIT5 + BIT6 + BIT7;
  //GPIO for flash CS
  P3OUT |= BIT0;
  P3DIR |= BIT0;
  UCB0CTL0 = UCCKPH + UCMSB + UCMST + UCSYNC; // 3-pin, 8-bit SPI master
  UCB0CTL1 = UCSSEL_2 + UCSWRST;              // SMCLK
  UCB0BR0 = 0x01;                             // factor=/1
  UCB0BR1 = 0;
  UCB0CTL1 &= ~UCSWRST;

  FLASH_CS_LOW;
  FLASH_SPI_TxRx(0xab); //Read ID and wake up from deep power-down
  FLASH_SPI_TxRx(0);
  FLASH_SPI_TxRx(0);
  FLASH_SPI_TxRx(0);
  FLASH_SPI_TxRx(0xff);
  FLASH_CS_HIGH;
}

void flash_sleep()
{
  FLASH_CS_LOW;
  FLASH_SPI_TxRx(0xb9); //Deep Power-Down
  FLASH_CS_HIGH;
  UCB0CTL1 |= UCSWRST;
}

static uint8_t FLASH_SPI_TxRx(uint8_t dat)
{
  UCB0TXBUF = dat;
  while (!(UCB0TXIFG & IFG2))
    ;
  while (!(UCB0RXIFG & IFG2))
    ;
  return UCB0RXBUF;
}
