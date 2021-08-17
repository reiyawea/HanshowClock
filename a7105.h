#ifndef _IFACE_A7105_H_
#define _IFACE_A7105_H_

// strobe commands. These are used to set the transceiver mode
enum A7105_State {
    A7105_SLEEP     = 0x80,
    A7105_IDLE      = 0x90,
    A7105_STANDBY   = 0xA0,
    A7105_PLL       = 0xB0,
    A7105_RX        = 0xC0,
    A7105_TX        = 0xD0,
    A7105_RST_WRPTR = 0xE0,
    A7105_RST_RDPTR = 0xF0,
};

// register addresses
enum {
    A7105_00_MODE         = 0x00,
    A7105_01_MODE_CONTROL = 0x01,
    A7105_02_CALC         = 0x02,
    A7105_03_FIFOI        = 0x03,
    A7105_04_FIFOII       = 0x04,
    A7105_05_FIFO_DATA    = 0x05,
    A7105_06_ID_DATA      = 0x06,
    A7105_07_RC_OSC_I     = 0x07,
    A7105_08_RC_OSC_II    = 0x08,
    A7105_09_RC_OSC_III   = 0x09,
    A7105_0A_CK0_PIN      = 0x0A,
    A7105_0B_GPIO1_PIN_I  = 0x0B,
    A7105_0C_GPIO2_PIN_II = 0x0C,
    A7105_0D_CLOCK        = 0x0D,
    A7105_0E_DATA_RATE    = 0x0E,
    A7105_0F_PLL_I        = 0x0F,
    A7105_10_PLL_II       = 0x10,
    A7105_11_PLL_III      = 0x11,
    A7105_12_PLL_IV       = 0x12,
    A7105_13_PLL_V        = 0x13,
    A7105_14_TX_I         = 0x14,
    A7105_15_TX_II        = 0x15,
    A7105_16_DELAY_I      = 0x16,
    A7105_17_DELAY_II     = 0x17,
    A7105_18_RX           = 0x18,
    A7105_19_RX_GAIN_I    = 0x19,
    A7105_1A_RX_GAIN_II   = 0x1A,
    A7105_1B_RX_GAIN_III  = 0x1B,
    A7105_1C_RX_GAIN_IV   = 0x1C,
    A7105_1D_RSSI_THOLD   = 0x1D,
    A7105_1E_ADC          = 0x1E,
    A7105_1F_CODE_I       = 0x1F,
    A7105_20_CODE_II      = 0x20,
    A7105_21_CODE_III     = 0x21,
    A7105_22_IF_CALIB_I   = 0x22,
    A7105_23_IF_CALIB_II  = 0x23,
    A7105_24_VCO_CURCAL   = 0x24,
    A7105_25_VCO_SBCAL_I  = 0x25,
    A7105_26_VCO_SBCAL_II = 0x26,
    A7105_27_BATTERY_DET  = 0x27,
    A7105_28_TX_TEST      = 0x28,
    A7105_29_RX_DEM_TEST_I  = 0x29,
    A7105_2A_RX_DEM_TEST_II = 0x2A,
    A7105_2B_CPC          = 0x2B,
    A7105_2C_XTAL_TEST    = 0x2C,
    A7105_2D_PLL_TEST     = 0x2D,
    A7105_2E_VCO_TEST_I   = 0x2E,
    A7105_2F_VCO_TEST_II  = 0x2F,
    A7105_30_IFAT         = 0x30,
    A7105_31_RSCALE       = 0x31,
    A7105_32_FILTER_TEST  = 0x32,
};

#define A7105_MASK_TRER  1
#define A7105_MASK_FBC  1
#define A7105_MASK_VBC  (1 << 1)
#define A7105_MASK_FBCF (1 << 4)
#define A7105_MASK_VBCF (1 << 3)
#define A7105_MASK_CRCF (1 << 5)
#define A7105_CALW (1 << 3)
#define A7105_RCOSC_E (1 << 2)
#define A7105_AIF (1 << 5)
#define A7105_WORE (1 << 3)
#define A7105_FMS (1 << 1)
#define A7105_GIOxOE  1
#define A7105_MASK_CALR (1 << 7)
#define A7105_IO_WTR (0<<2)
#define A7105_IO_WAK (4<<2)
#define A7105_IO_SDO (6<<2)

#define rfio2 (P1IN & BIT0)

// Triggers the chip to reset, then prints the contents of the mode register to serial
void A7105_Init();

// Transmits the given strobe command. Commands are enumerated in a7105.h and detailed in the documentation
void A7105_Strobe(enum A7105_State);

// Access the current value of the transmitter ID
void A7105_WriteID(uint32_t);
uint32_t A7105_ReadID();

// Access an arbitrary register, given by addr
void A7105_WriteReg(uint8_t addr, uint8_t value);
uint8_t A7105_ReadReg(uint8_t addr);

// Transmit or receive data through the FIFO buffer with current settings
void A7105_WriteData(uint8_t *, uint8_t);
void A7105_ReadData(uint8_t *, uint8_t);

int A7105_EnterWOR();
#endif
