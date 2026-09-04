/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : TinyML Power and Thermal Management
  *
  * Current Stage:
  *  - Controlled workload generation
  *  - CPU monitoring
  *  - RAM monitoring
  *  - UART telemetry
  *  - INA219 I2C interface
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include "workload.h"
#include "system_monitor.h"
#include "telemetry.h"
#include "ina219.h"
#include <string.h>
#include <stdio.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

/*
 * Select the controlled workload level.
 *
 * Options:
 *
 *     WORKLOAD_LOW
 *     WORKLOAD_MEDIUM
 *     WORKLOAD_HIGH
 */
static WorkloadLevel_t current_workload = WORKLOAD_LOW;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_I2C1_Init(void);

void I2C_BusTest(void);
void I2C_Scan(void);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

    /*
     * CPU frequency.
     */
    uint32_t cpu_frequency_hz;

    /*
     * Requested workload percentage.
     */
    uint32_t requested_workload_percent;

    /*
     * Active and idle durations.
     */
    uint32_t active_time_ms;
    uint32_t idle_time_ms;

    /*
     * Number of CPU cycles corresponding to
     * the active workload duration.
     */
    uint32_t active_cycles;

    /*
     * Accumulated workload cycles.
     */
    uint32_t busy_cycles_total;

    /*
     * DWT cycle counter values.
     */
    uint32_t workload_start_cycle;
    uint32_t workload_end_cycle;
    uint32_t measurement_start_cycle;
    uint32_t measurement_end_cycle;

    /*
     * Total cycles during measurement.
     */
    uint32_t total_cycles;

    /*
     * RAM measurements.
     */
    uint32_t static_ram_bytes;

    /*
     * Calculated percentages.
     */
    float cpu_load_percent;
    float static_ram_percent;

    /*
     * Number of workload periods.
     */
    uint32_t number_of_periods;

    /*
     * Loop counter.
     */
    uint32_t i;

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */
  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART2_UART_Init();
  MX_I2C1_Init();
  /* USER CODE BEGIN 2 */

    /*
     * ============================================================
     * Initialize application modules
     * ============================================================
     */

    /*
     * Initialize DWT cycle counter and workload subsystem.
     */
    Workload_Init();

    /*
     * Initialize system monitoring subsystem.
     */
    SystemMonitor_Init();

    /*
     * Initialize telemetry subsystem.
     */
    Telemetry_Init();

    /*
     * ============================================================
     * I2C DEVICE SCAN
     * ============================================================
     *
     * Temporarily scan all valid 7-bit I2C addresses.
     *
     * We are using this to determine whether the STM32
     * can detect the INA219 module.
     *
     * Expected default INA219 address:
     *
     *     0x40
     *
     */

    I2C_BusTest();

    /*
     * Give the serial terminal time to initialize.
     */
    HAL_Delay(500);

    /*
     * Send CSV header.
     */
    Telemetry_SendHeader();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
    while (1)
    {
        /*
         * ========================================================
         * 1. Read CPU frequency
         * ========================================================
         */
        cpu_frequency_hz =
            SystemMonitor_GetCPUFrequency();

        /*
         * ========================================================
         * 2. Get selected workload percentage
         * ========================================================
         */
        requested_workload_percent =
            Workload_GetPercent(current_workload);

        /*
         * ========================================================
         * 3. Calculate active and idle time
         *
         * LOW:
         *     20 ms active
         *     80 ms idle
         *
         * MEDIUM:
         *     50 ms active
         *     50 ms idle
         *
         * HIGH:
         *     80 ms active
         *     20 ms idle
         *
         * ========================================================
         */

        active_time_ms =
            requested_workload_percent;

        idle_time_ms =
            100U - active_time_ms;

        /*
         * ========================================================
         * 4. Convert active time to CPU cycles
         * ========================================================
         */
        active_cycles =
            (uint32_t)
            (
                (
                    (uint64_t)cpu_frequency_hz *
                    (uint64_t)active_time_ms
                ) / 1000ULL
            );

        /*
         * ========================================================
         * 5. Number of workload periods
         *
         * Measurement window = 1000 ms
         * Period               = 100 ms
         *
         * Therefore:
         *
         *     1000 / 100 = 10 periods
         *
         * ========================================================
         */

        number_of_periods = 10U;

        /*
         * Reset accumulated busy cycles.
         */
        busy_cycles_total = 0U;

        /*
         * Start complete measurement window.
         */
        measurement_start_cycle =
            SystemMonitor_GetCycleCount();

        /*
         * ========================================================
         * 6. Execute controlled workload
         * ========================================================
         */
        for (i = 0U; i < number_of_periods; i++)
        {
            /*
             * Start workload cycle measurement.
             */
            workload_start_cycle =
                SystemMonitor_GetCycleCount();

            /*
             * Execute synthetic workload.
             */
            Workload_Run(active_cycles);

            /*
             * End workload cycle measurement.
             */
            workload_end_cycle =
                SystemMonitor_GetCycleCount();

            /*
             * Accumulate actual workload cycles.
             */
            busy_cycles_total +=
                (uint32_t)
                (
                    workload_end_cycle -
                    workload_start_cycle
                );

            /*
             * Execute idle period.
             */
            if (idle_time_ms > 0U)
            {
                HAL_Delay(idle_time_ms);
            }
        }

        /*
         * End complete measurement window.
         */
        measurement_end_cycle =
            SystemMonitor_GetCycleCount();

        /*
         * ========================================================
         * Calculate total elapsed cycles.
         * ========================================================
         */
        total_cycles =
            (uint32_t)
            (
                measurement_end_cycle -
                measurement_start_cycle
            );

        /*
         * ========================================================
         * 7. Calculate CPU workload
         * ========================================================
         */
        cpu_load_percent =
            SystemMonitor_CalculateCPULoad(
                busy_cycles_total,
                total_cycles
            );

        /*
         * ========================================================
         * 8. Measure static RAM
         * ========================================================
         */
        static_ram_bytes =
            SystemMonitor_GetStaticRAM();

        /*
         * ========================================================
         * 9. Calculate RAM utilization
         * ========================================================
         */
        static_ram_percent =
            SystemMonitor_CalculateRAMPercent(
                static_ram_bytes
            );

        /*
         * ========================================================
         * 10. Send existing telemetry
         *
         * INA219 readings are intentionally NOT included yet.
         *
         * ========================================================
         */
        Telemetry_SendSample(
            HAL_GetTick(),
            cpu_frequency_hz,
            cpu_load_percent,
            static_ram_bytes,
            static_ram_percent,
            Workload_GetName(current_workload)
        );
    }

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 360;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Activate the Over-Drive mode
  */
  if (HAL_PWREx_EnableOverDrive() != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */
  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */
  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */
  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */
  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */
  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */
  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  /* USER CODE BEGIN MX_GPIO_Init_1 */
  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* USER CODE BEGIN MX_GPIO_Init_2 */
  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/*
 * Application functionality is kept inside its respective
 * modules:
 *
 *     workload.c
 *     system_monitor.c
 *     telemetry.c
 *     ina219.c
 *
 */
void I2C_BusTest(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    char msg[64];

    /* Temporarily configure PB6 and PB7 as GPIO inputs */
    GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    HAL_Delay(10);

    uint8_t scl = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_6);
    uint8_t sda = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_7);

    int len = snprintf(msg,
                       sizeof(msg),
                       "I2C BUS: SCL=%d SDA=%d\r\n",
                       scl,
                       sda);

    HAL_UART_Transmit(&huart2,
                      (uint8_t *)msg,
                      len,
                      HAL_MAX_DELAY);
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */

    /*
     * Disable interrupts.
     */
    __disable_irq();

    /*
     * Stay here if a peripheral or system initialization
     * fails.
     */
    while (1)
    {
    }

  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */

    /*
     * User can add implementation to report the file name
     * and line number.
     */

  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
