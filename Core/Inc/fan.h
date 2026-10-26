/*
 * fan.h
 *
 *  12 V DC fan through an L293D (wiring in app_config.h). TIM1 and the
 *  GPIOs are set up here in user code, so CubeMX does not need changing.
 */

#ifndef FAN_H
#define FAN_H

#include <stdint.h>

void    Fan_Init(void);
void    Fan_SetPercent(uint8_t pct);   /* 0 = off; 1..100 is raised to FAN_MIN_RUN_PCT */
uint8_t Fan_GetPercent(void);
void    Fan_Service(void);             /* call often from the main loop: ends the start-up kick */

/* Manual override from the UART "FAN" command: 0..100, or -1 to return to automatic */
void    Fan_SetOverride(int16_t pct);
int16_t Fan_GetOverride(void);

#endif /* FAN_H */
