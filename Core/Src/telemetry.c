/*
 * telemetry.c
 *
 *  Created on: Sep 4, 2026
 *      Author: swaro
 */
#include <stdio.h>
#include <string.h>
#include <telemetry.h>


/*
 * UART buffer used for CSV telemetry.
 */
static char uart_buffer[200];


/*
 * USART2 handle is defined in main.c.
 *
 * extern allows this module to use the same UART
 * peripheral without defining another UART handle.
 */
extern UART_HandleTypeDef huart2;


/**
  * @brief Initialize telemetry subsystem.
  */
void Telemetry_Init(void)
{
    /*
     * No additional initialization is required because
     * USART2 is initialized by CubeMX-generated code.
     */
}


/**
  * @brief Send a string through USART2.
  */
static void Telemetry_SendString(const char *str)
{
    HAL_UART_Transmit(
        &huart2,
        (uint8_t *)str,
        strlen(str),
        100U
    );
}


/**
  * @brief Send CSV header.
  */
void Telemetry_SendHeader(void)
{
    Telemetry_SendString(
        "Timestamp_ms,"
        "CPU_Frequency_Hz,"
        "CPU_Load_Percent,"
        "Static_RAM_Bytes,"
        "Static_RAM_Percent,"
        "Workload\r\n"
    );
}


/**
  * @brief Send one telemetry sample.
  *
  * Floating-point values are converted into integer
  * hundredths before formatting.
  *
  * Example:
  *
  *     19.80 -> 1980
  *
  *     0.56  -> 56
  *
  * This avoids requiring floating-point support in
  * printf/snprintf.
  */
void Telemetry_SendSample(
    uint32_t timestamp_ms,
    uint32_t cpu_frequency_hz,
    float cpu_load_percent,
    uint32_t static_ram_bytes,
    float static_ram_percent,
    const char *workload_name)
{
    uint32_t cpu_load_hundredths;

    uint32_t ram_percent_hundredths;


    /*
     * Convert percentages to integer hundredths.
     */
    cpu_load_hundredths =
        (uint32_t)(cpu_load_percent * 100.0f);


    ram_percent_hundredths =
        (uint32_t)(static_ram_percent * 100.0f);


    /*
     * Build CSV string.
     */
    snprintf(
        uart_buffer,
        sizeof(uart_buffer),

        "%lu,%lu,%lu.%02lu,%lu,%lu.%02lu,%s\r\n",

        /*
         * Timestamp.
         */
        (unsigned long)timestamp_ms,


        /*
         * CPU frequency.
         */
        (unsigned long)cpu_frequency_hz,


        /*
         * CPU load integer part.
         */
        (unsigned long)
        (cpu_load_hundredths / 100U),


        /*
         * CPU load fractional part.
         */
        (unsigned long)
        (cpu_load_hundredths % 100U),


        /*
         * Static RAM.
         */
        (unsigned long)static_ram_bytes,


        /*
         * RAM percentage integer part.
         */
        (unsigned long)
        (ram_percent_hundredths / 100U),


        /*
         * RAM percentage fractional part.
         */
        (unsigned long)
        (ram_percent_hundredths % 100U),


        /*
         * Workload name.
         */
        workload_name
    );


    /*
     * Send telemetry.
     */
    Telemetry_SendString(uart_buffer);
}

