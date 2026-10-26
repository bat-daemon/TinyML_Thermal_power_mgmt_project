/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "logging.h"
#include "sensor_acquisition.h"
#include "workload_generator.h"
#include "system_resource.h"
#include "app_config.h"
#include "feature_extraction.h"
#include "predictor.h"
#include "reaction.h"
#include "battery.h"
#include "fan.h"
#include "status_led.h"
#include "uart_cmd.h"
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

/* USER CODE BEGIN PV */
/* Set by interrupts; any of them wakes the main loop out of a busy slice or __WFI */
static volatile uint8_t tick_flag = 0;
static volatile uint8_t button_flag = 0;
static float pot_pct = 0.0f;          /* last pot dial reading, shown on the [SYS] line */
static volatile uint8_t app_event = 0;

static AppMode_t app_mode = APP_MODE_RUN;

/* Idle baseline for the die temperature */
static uint8_t  calibrating = 1;
static uint32_t baseline_n = 0;
static float    baseline_sum = 0.0f;
static float    baseline_c = 0.0f;

/* Moving average of the die temperature */
static float    temp_hist[TEMP_SMOOTH_N];
static uint32_t temp_count = 0;

/* Power averaged over each 1 s slot (sampled every POWER_SAMPLE_MS) */
static float    pwr_sum_mw = 0.0f;
static float    cur_sum_ma = 0.0f;
static float    volt_sum_v = 0.0f;
static uint32_t pwr_n = 0;
static uint32_t last_power_ms = 0;

static uint32_t slot_start_ms = 0;
static uint32_t last_tick_ms = 0;

static const char *LEVEL_NAMES[] = { "IDLE", "LOW", "MED", "HIGH" };
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static void App_Tick(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void App_SetMode(AppMode_t mode)
{
    app_mode = mode;
    Workload_AutoEnable(mode == APP_MODE_AUTO, HAL_GetTick() ^ (uint32_t)(Sensor_ReadTempDie() * 1000.0f));
}

AppMode_t App_GetMode(void)
{
    return app_mode;
}

void App_RecalibrateBaseline(void)
{
    calibrating = 1;
    baseline_n = 0;
    baseline_sum = 0.0f;
    temp_count = 0;
    Features_Reset();
    Workload_SetCap(WORKLOAD_IDLE);   /* baseline must be measured at idle (no log: not a reaction) */
    StatusLed_ShowCalibrating();
    LOG_INFO(TAG_SENS, "Measuring idle baseline for %u s...", (unsigned)BASELINE_SAMPLES);
}

static void sample_power(void)
{
    float v, i, p;
    if (Sensor_ReadPower(&v, &i, &p)) {
        pwr_sum_mw += p;
        cur_sum_ma += i;
        volt_sum_v += v;
        pwr_n++;
    }
}

static float smooth_temp(float t)
{
    temp_hist[temp_count % TEMP_SMOOTH_N] = t;
    temp_count++;
    uint32_t n = (temp_count < TEMP_SMOOTH_N) ? temp_count : TEMP_SMOOTH_N;
    float sum = 0.0f;
    for (uint32_t i = 0; i < n; i++) sum += temp_hist[i];
    return sum / (float)n;
}

/* One pass of the pipeline, once per TIM2 tick:
 * sense -> features -> predict -> react -> battery -> output */
static void App_Tick(void)
{
    uint32_t now = HAL_GetTick();
    float dt_s = (last_tick_ms == 0) ? (SAMPLE_PERIOD_MS / 1000.0f) : (now - last_tick_ms) / 1000.0f;
    last_tick_ms = now;

    SensorReading_t r;
    if (!Sensor_ReadAll(&r)) {
        LOG_WARN(TAG_SENS, "INA219 read failed this tick");
    }
    if (pwr_n > 0) {
        /* Replace the single snapshot with the average over the whole slot */
        r.power_mw = pwr_sum_mw / (float)pwr_n;
        r.current_ma = cur_sum_ma / (float)pwr_n;
        r.voltage_v = volt_sum_v / (float)pwr_n;
    }
    pwr_sum_mw = cur_sum_ma = volt_sum_v = 0.0f;
    pwr_n = 0;

    float t_smooth = smooth_temp(r.temp_die_c);
    if (calibrating) {
        baseline_sum += r.temp_die_c;
        if (++baseline_n >= BASELINE_SAMPLES) {
            baseline_c = baseline_sum / (float)baseline_n;
            calibrating = 0;
            Reaction_Init();
            LOG_INFO(TAG_SENS, "Baseline %.2f C. Limits: caution +%.1f C, critical +%.1f C",
                     (double)baseline_c, (double)Predictor_GetCautionRise(), (double)Predictor_GetCriticalRise());
        }
    }
    float rise = calibrating ? 0.0f : (t_smooth - baseline_c);

    if (app_mode == APP_MODE_AUTO && !calibrating) {
        Workload_AutoStep(now);
    }

    /* The level that ran during the slot just finished: this is what the model sees */
    uint8_t wl_req = (uint8_t)Workload_GetLevel();
    uint8_t wl_eff = (uint8_t)Workload_GetEffectiveLevel();

    float feat[FEAT_COUNT];
    Features_Update(rise, r.power_mw, wl_eff, feat);

    Prediction_t pred;
    Predictor_Run(feat, &pred);

    Battery_Update(r.power_mw, dt_s);

    if (!calibrating) {
        Reaction_Update(&pred, rise, Battery_GetPercent(), app_mode == APP_MODE_RUN);
        StatusLed_ShowRisk(Reaction_SafetyActive() ? RISK_CRITICAL : pred.temp_risk);
    }

    DataRecord_t rec = {
        .timestamp_ms     = now,
        .power_mw         = r.power_mw,
        .voltage_v        = r.voltage_v,
        .current_ma       = r.current_ma,
        .temp_die_c       = r.temp_die_c,
        .temp_rise_c      = rise,
        .temp_ambient_c   = r.temp_ambient_c,
        .workload_req     = wl_req,
        .workload_eff     = wl_eff,
        .cpu_freq_hz      = System_GetCpuFreqHz(),
        .stack_used_bytes = System_GetStackUsedBytes(),
        .heap_used_bytes  = System_GetHeapUsedBytes(),
        .fan_pct          = Fan_GetPercent(),
        .temp_risk        = (uint8_t)pred.temp_risk,
        .power_risk       = (uint8_t)pred.power_risk,
        .rule_temp_risk   = (uint8_t)pred.rule_temp_risk,
        .mode             = (uint8_t)app_mode,
    };
    Log_Data(&rec);

    LOG_INFO(TAG_DISP, "T=%.1fC Ta=%.1fC rise=%+.1fC slope=%+.2fC/s P=%.0fmW WL=%s/%s TEMP(%s):%s TEMP(rule):%s PWR:%s fan=%u%% bat=%.1f%% %.1fh",
             (double)r.temp_die_c, (double)r.temp_ambient_c, (double)rise, (double)feat[FEAT_TEMP_SLOPE10], (double)r.power_mw,
             LEVEL_NAMES[wl_req], LEVEL_NAMES[Workload_GetEffectiveLevel()],
             pred.model_used ? "ML" : "RULE", Risk_Name(pred.temp_risk), Risk_Name(pred.rule_temp_risk),
             Risk_Name(pred.power_risk), (unsigned)Fan_GetPercent(),
             (double)Battery_GetPercent(), (double)Battery_GetHoursLeft());
    if (pred.engine_xcubeai) {
        LOG_INFO(TAG_SYS, "CPU=%luMHz stack=%luB heap=%luB inference: X-CUBE-AI %luus, plain C %luus, differ %lu pot=%.0f%%(%s)",
                 (unsigned long)(rec.cpu_freq_hz / 1000000u), (unsigned long)rec.stack_used_bytes,
                 (unsigned long)rec.heap_used_bytes, (unsigned long)pred.inference_xai_us,
                 (unsigned long)pred.inference_c_us, (unsigned long)Predictor_GetEngineMismatches(),
                 (double)pot_pct, Workload_PotIsEnabled() ? "ON" : "off");
    } else {
        LOG_INFO(TAG_SYS, "CPU=%luMHz stack=%luB heap=%luB inference=%luus (plain C) pot=%.0f%%(%s)",
                 (unsigned long)(rec.cpu_freq_hz / 1000000u), (unsigned long)rec.stack_used_bytes,
                 (unsigned long)rec.heap_used_bytes, (unsigned long)pred.inference_us,
                 (double)pot_pct, Workload_PotIsEnabled() ? "ON" : "off, type WL POT");
    }
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2) {
        tick_flag = 1;
        app_event = 1;
    }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    static uint32_t last_press_ms = 0;
    if (GPIO_Pin == GPIO_PIN_13 && (HAL_GetTick() - last_press_ms) > 250u) {
        last_press_ms = HAL_GetTick();
        button_flag = 1;
        app_event = 1;
    }
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */
  System_ResourceInit();   /* paint the stack canary before any deep calls */
  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_I2C1_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_TIM4_Init();
  MX_TIM14_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  Log_Init(&huart2);
  LOG_INFO(TAG_SENS, "Logging initialized, build %s %s", __DATE__, __TIME__);

  if (!Sensor_Init()) {
      LOG_ERROR(TAG_SENS, "Sensor init failed — halting");
      Error_Handler();
  }
  StatusLed_Init();
  Fan_Init();
  Battery_Init();
  Predictor_Init();
  Reaction_Init();
  Cmd_Init(&huart2);
  Workload_SetLevel(WORKLOAD_IDLE);
  App_RecalibrateBaseline();
  HAL_TIM_Base_Start_IT(&htim2);
  LOG_INFO(TAG_SYS, "CPU %lu Hz, horizon %lu s. Type HELP for commands.",
           (unsigned long)System_GetCpuFreqHz(), (unsigned long)Predictor_GetHorizonS());
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    app_event = 0;

    Cmd_Poll();
    Fan_Service();

    if (button_flag) {
        button_flag = 0;
        if (app_mode == APP_MODE_AUTO) {
            LOG_WARN(TAG_WKLD, "Button ignored in AUTO mode");
        } else {
            if (Workload_PotIsEnabled()) {
                Workload_PotEnable(0);
            }
            Workload_CycleLevel();
        }
    }

    if (tick_flag) {
        tick_flag = 0;
        slot_start_ms = HAL_GetTick();
        App_Tick();
        /* The dial sets the level for the slot that starts now */
        pot_pct = Sensor_ReadPotPercent();
        if (app_mode != APP_MODE_AUTO) {
            Workload_PotUpdate(pot_pct);
        }
    }

    if ((HAL_GetTick() - last_power_ms) >= POWER_SAMPLE_MS) {
        last_power_ms = HAL_GetTick();
        sample_power();
    }

    /* Busy part of the slot (duty cycle of the effective level), then sleep.
     * Any interrupt (tick, UART byte, button, SysTick) ends the sleep. */
    uint32_t busy_ms = SAMPLE_PERIOD_MS * Workload_GetDutyPercent() / 100u;
    if ((HAL_GetTick() - slot_start_ms) < busy_ms) {
        Workload_RunSlice(slot_start_ms, SAMPLE_PERIOD_MS, &app_event);
    } else if (!app_event && !cmd_line_ready) {
        __WFI();
    }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
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
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 180;
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

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
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
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
