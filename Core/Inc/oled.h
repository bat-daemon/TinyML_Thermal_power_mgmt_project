/*
 * oled.h
 *
 *  SSD1306 128x64 OLED on I2C1 (shared with the INA219), 8 text lines of
 *  21 characters. The frame is drawn in RAM once a second and sent one
 *  page (8 pixel rows) per main-loop pass, so the bus is never blocked for
 *  more than a few ms. If no display answers at boot, everything is skipped.
 */

#ifndef OLED_H
#define OLED_H

#include <stdint.h>

#define OLED_LINES  8
#define OLED_COLS   21

void    Oled_Init(void);
uint8_t Oled_Present(void);
void    Oled_Clear(void);
/* printf into text line 0..7; invert = white background (used for alerts) */
void    Oled_Printf(uint8_t line, uint8_t invert, const char *fmt, ...);
void    Oled_Flush(void);          /* mark the frame ready to send */
void    Oled_Service(void);        /* call from the main loop: sends at most one page */
void    Oled_Enable(uint8_t on);   /* UART "OLED 0/1": blank the panel and stop bus traffic */
uint8_t Oled_IsEnabled(void);

#endif /* OLED_H */
