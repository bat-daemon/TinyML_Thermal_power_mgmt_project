/*
 * uart_cmd.c
 */

#include "uart_cmd.h"
#include "main.h"
#include "logging.h"
#include "battery.h"
#include "fan.h"
#include "predictor.h"
#include "workload_generator.h"
#include "status_led.h"
#include "oled.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#define CMD_MAX_LEN 48

static UART_HandleTypeDef *cmd_huart = NULL;
static uint8_t rx_byte;
static char rx_buf[CMD_MAX_LEN];
static uint8_t rx_len = 0;
static char line[CMD_MAX_LEN];
volatile uint8_t cmd_line_ready = 0;

void Cmd_Init(UART_HandleTypeDef *huart)
{
    cmd_huart = huart;
    /* The .ioc leaves the USART2 interrupt off, so enable it here */
    HAL_NVIC_SetPriority(USART2_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
    HAL_UART_Receive_IT(cmd_huart, &rx_byte, 1);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart != cmd_huart) return;

    char c = (char)rx_byte;
    if (c == '\r' || c == '\n') {
        if (rx_len > 0 && !cmd_line_ready) {
            memcpy(line, rx_buf, rx_len);
            line[rx_len] = '\0';
            cmd_line_ready = 1;
        }
        rx_len = 0;
    } else if (rx_len < CMD_MAX_LEN - 1) {
        rx_buf[rx_len++] = (char)toupper((unsigned char)c);
    }
    HAL_UART_Receive_IT(cmd_huart, &rx_byte, 1);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    /* Overrun etc.: drop the partial line and keep listening */
    if (huart != cmd_huart) return;
    rx_len = 0;
    HAL_UART_Receive_IT(cmd_huart, &rx_byte, 1);
}

static const char *MODE_NAMES[] = { "RUN", "COLLECT", "AUTO" };

static void print_help(void)
{
    LOG_INFO(TAG_DISP, "Commands: BAT <mAh> <V> | MODE RUN/COLLECT/AUTO | WL 0-3/POT | FAN 0-100/AUTO | LIM <caut> <crit> | BASE | LED r g b/AUTO | OLED 0/1");
}

void Cmd_Poll(void)
{
    if (!cmd_line_ready) return;

    char *save = NULL;
    char *cmd = strtok_r(line, " \t", &save);
    char *a1 = strtok_r(NULL, " \t", &save);
    char *a2 = strtok_r(NULL, " \t", &save);
    char *a3 = strtok_r(NULL, " \t", &save);

    if (cmd == NULL) {
        /* empty line */
    } else if (strcmp(cmd, "HELP") == 0) {
        print_help();
    } else if (strcmp(cmd, "BAT") == 0) {
        if (a1 && a2) {
            Battery_Set(strtof(a1, NULL), strtof(a2, NULL));
        } else {
            LOG_INFO(TAG_REACT, "Battery %.0f mAh @ %.2f V, %.1f%% left, %.1f h at current draw",
                     (double)Battery_GetCapacityMah(), (double)Battery_GetVoltage(),
                     (double)Battery_GetPercent(), (double)Battery_GetHoursLeft());
        }
    } else if (strcmp(cmd, "MODE") == 0 && a1) {
        if (strcmp(a1, "RUN") == 0)          App_SetMode(APP_MODE_RUN);
        else if (strcmp(a1, "COLLECT") == 0) App_SetMode(APP_MODE_COLLECT);
        else if (strcmp(a1, "AUTO") == 0)    App_SetMode(APP_MODE_AUTO);
        else LOG_WARN(TAG_DISP, "Unknown mode %s", a1);
        LOG_INFO(TAG_DISP, "Mode = %s", MODE_NAMES[App_GetMode()]);
    } else if (strcmp(cmd, "WL") == 0 && a1) {
        if (App_GetMode() == APP_MODE_AUTO) {
            LOG_WARN(TAG_DISP, "AUTO mode picks the workload; use MODE COLLECT first");
        } else if (strcmp(a1, "POT") == 0) {
            Workload_PotEnable(1);
        } else {
            if (Workload_PotIsEnabled()) {
                Workload_PotEnable(0);
            }
            Workload_SetLevel((WorkloadLevel_t)strtol(a1, NULL, 10));
        }
    } else if (strcmp(cmd, "FAN") == 0 && a1) {
        if (strcmp(a1, "AUTO") == 0) Fan_SetOverride(-1);
        else Fan_SetOverride((int16_t)strtol(a1, NULL, 10));
    } else if (strcmp(cmd, "LIM") == 0) {
        if (a1 && a2) Predictor_SetLimits(strtof(a1, NULL), strtof(a2, NULL));
        LOG_INFO(TAG_ML, "Limits: caution +%.1f C, critical +%.1f C above baseline",
                 (double)Predictor_GetCautionRise(), (double)Predictor_GetCriticalRise());
    } else if (strcmp(cmd, "LED") == 0) {
        if (a1 && strcmp(a1, "AUTO") == 0) {
            StatusLed_Hold(0);
            LOG_INFO(TAG_DISP, "LED back to automatic");
        } else if (a1 && a2 && a3) {
            StatusLed_Hold(1);
            StatusLed_Test((uint8_t)strtol(a1, NULL, 10), (uint8_t)strtol(a2, NULL, 10), (uint8_t)strtol(a3, NULL, 10));
            LOG_INFO(TAG_DISP, "LED test R=%s G=%s B=%s (LED AUTO to release)", a1, a2, a3);
        }
    } else if (strcmp(cmd, "BASE") == 0) {
        App_RecalibrateBaseline();
    } else if (strcmp(cmd, "OLED") == 0) {
        if (!Oled_Present()) {
            LOG_WARN(TAG_DISP, "No OLED was found at boot");
        } else {
            if (a1) Oled_Enable((uint8_t)strtol(a1, NULL, 10));
            LOG_INFO(TAG_DISP, "OLED %s", Oled_IsEnabled() ? "on" : "off");
        }
    } else {
        LOG_WARN(TAG_DISP, "Unknown command '%s'", cmd);
        print_help();
    }

    cmd_line_ready = 0;
}
