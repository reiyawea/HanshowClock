#ifndef EPD_H_
#define EPD_H_

#define EPD_HEIGHT 250
#define EPD_WIDTH  122

void EPD_init();
void EPD_dispUpdate(uint8_t full);
void EPD_busyWait();
bool EPD_isBusy();
void EPD_sleep(void);
void EPD_draw(uint8_t x, uint8_t y, uint8_t xsize, uint8_t ysize, const uint8_t *dat);

#endif /* EPD_H_ */
