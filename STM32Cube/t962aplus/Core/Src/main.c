/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "retarget.h"
#include <stdio.h>
#include "eeprom.h"
#include "lcd.h"
#include "max31855.h"
#include "sched.h"
#include "rtc.h"
#include "keypad.h"
#include "reflow.h"
#include "nvstorage.h"
#include "setup.h"
#include "adc.h"
#include "io.h"
#include "sensor.h"
#include "version.h"
#include "buzzer.h"

// No version.c file generated for LPCXpresso builds, fall back to this
__attribute__((weak)) const char* Version_GetGitVersion(void) {
	return "Pre-alpha-ultra-early";
	//return "no version info";
}

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
ADC_HandleTypeDef hadc1;
ADC_HandleTypeDef hadc2;

I2C_HandleTypeDef hi2c1;

RTC_HandleTypeDef hrtc;

TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim4;

UART_HandleTypeDef huart1;

/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 384 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_TIM1_Init(void);
static void MX_TIM4_Init(void);
static void MX_ADC2_Init(void);
static void MX_RTC_Init(void);
static void MX_TIM2_Init(void);
void StartDefaultTask(void *argument);

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
extern uint8_t logobmp[];
extern uint8_t stopbmp[];
extern uint8_t editbmp[];
extern uint8_t f3editbmp[];
extern uint8_t selectbmp[];

static int32_t Main_Work(void);


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

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_I2C1_Init();
  MX_USART1_UART_Init();
  MX_TIM1_Init();
  MX_TIM4_Init();
  MX_ADC2_Init();
  MX_RTC_Init();
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */

  // De-HALize TIM3 so we can get a proper 32-bit counter
  TIM3->ARR = 0xffff;
  TIM3->CR2 = 0x20;
  TIM3->PSC = 8; // Div by 9 means count 8 times per us

  RetargetInit(&huart1);
  EEPROM_Init(&hi2c1);
  NV_Init();

  LCD_Init();
  LCD_BMPDisplay(logobmp, 0, 0);

  uint8_t buf[30];
  uint8_t len;

	len = IO_Partinfo(buf, sizeof(buf), "%s rev %c");
	LCD_disp_str((uint8_t*)buf, len, 0, 64 - 6, FONT6X6);
	printf("\nRunning on an %s", buf);

	len = snprintf(buf, sizeof(buf), "%s", Version_GetGitVersion());
	LCD_disp_str((uint8_t*)buf, len, LCD_ALIGN_CENTER(len), 0, FONT6X6);

  //len = snprintf(buf, sizeof(buf), "T-962A+ controller");
  //LCD_disp_str((uint8_t*)buf, len, LCD_ALIGN_CENTER(len), 0, FONT6X6);

  //len = snprintf(buf, sizeof(buf), "Unified Engineering");
  //LCD_disp_str((uint8_t*)buf, len, LCD_ALIGN_CENTER(len), 64-12, FONT6X6);
  //len = snprintf(buf, sizeof(buf), "Pre-alpha-ultra-early");
  //LCD_disp_str((uint8_t*)buf, len, LCD_ALIGN_CENTER(len), 64-6, FONT6X6);
  LCD_FB_Update();

  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1); // "Cool" on PA8
  HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_3); // "Heat" on PB8

  HAL_ADC_Start(&hadc1);
  HAL_ADC_Start(&hadc2);

  RTC_Init(&hrtc);
  Sched_Init(&htim2);

  SPI_TC_Init(&hi2c1);

	Keypad_Init();
	Buzzer_Init();
	ADC_Init();
//	RTC_Init();
//	OneWire_Init();
//	SPI_TC_Init();
	Reflow_Init();
//	SystemFan_Init();

	Sched_SetWorkfunc(MAIN_WORK, Main_Work);
	Sched_SetState(MAIN_WORK, 1, TICKS_SECS(2)); // Enable in 2 seconds

	Buzzer_Beep(BUZZ_1KHZ, 255, TICKS_MS(100));



  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  printf("Just before osKernelStart\r\n");
  /* USER CODE END RTOS_EVENTS */

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */
  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
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
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_RTC|RCC_PERIPHCLK_ADC;
  PeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_HSE_DIV128;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */
  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ScanConvMode = ADC_SCAN_ENABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 16;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_28CYCLES_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel
  */
  sConfig.Rank = ADC_REGULAR_RANK_2;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel
  */
  sConfig.Rank = ADC_REGULAR_RANK_3;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel
  */
  sConfig.Rank = ADC_REGULAR_RANK_4;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel
  */
  sConfig.Rank = ADC_REGULAR_RANK_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel
  */
  sConfig.Rank = ADC_REGULAR_RANK_6;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel
  */
  sConfig.Rank = ADC_REGULAR_RANK_7;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel
  */
  sConfig.Rank = ADC_REGULAR_RANK_8;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_2;
  sConfig.Rank = ADC_REGULAR_RANK_9;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel
  */
  sConfig.Rank = ADC_REGULAR_RANK_10;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel
  */
  sConfig.Rank = ADC_REGULAR_RANK_11;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel
  */
  sConfig.Rank = ADC_REGULAR_RANK_12;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel
  */
  sConfig.Rank = ADC_REGULAR_RANK_13;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel
  */
  sConfig.Rank = ADC_REGULAR_RANK_14;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel
  */
  sConfig.Rank = ADC_REGULAR_RANK_15;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel
  */
  sConfig.Rank = ADC_REGULAR_RANK_16;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief ADC2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC2_Init(void)
{

  /* USER CODE BEGIN ADC2_Init 0 */

  /* USER CODE END ADC2_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC2_Init 1 */

  /* USER CODE END ADC2_Init 1 */
  /** Common config
  */
  hadc2.Instance = ADC2;
  hadc2.Init.ScanConvMode = ADC_SCAN_ENABLE;
  hadc2.Init.ContinuousConvMode = ENABLE;
  hadc2.Init.DiscontinuousConvMode = DISABLE;
  hadc2.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc2.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc2.Init.NbrOfConversion = 16;
  if (HAL_ADC_Init(&hadc2) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_6;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_28CYCLES_5;
  if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel
  */
  sConfig.Rank = ADC_REGULAR_RANK_2;
  if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel
  */
  sConfig.Rank = ADC_REGULAR_RANK_3;
  if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel
  */
  sConfig.Rank = ADC_REGULAR_RANK_4;
  if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel
  */
  sConfig.Rank = ADC_REGULAR_RANK_5;
  if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel
  */
  sConfig.Rank = ADC_REGULAR_RANK_6;
  if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel
  */
  sConfig.Rank = ADC_REGULAR_RANK_7;
  if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel
  */
  sConfig.Rank = ADC_REGULAR_RANK_8;
  if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel
  */
  sConfig.Rank = ADC_REGULAR_RANK_9;
  if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel
  */
  sConfig.Rank = ADC_REGULAR_RANK_10;
  if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel
  */
  sConfig.Rank = ADC_REGULAR_RANK_11;
  if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel
  */
  sConfig.Rank = ADC_REGULAR_RANK_12;
  if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel
  */
  sConfig.Rank = ADC_REGULAR_RANK_13;
  if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel
  */
  sConfig.Rank = ADC_REGULAR_RANK_14;
  if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel
  */
  sConfig.Rank = ADC_REGULAR_RANK_15;
  if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel
  */
  sConfig.Rank = ADC_REGULAR_RANK_16;
  if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC2_Init 2 */

  /* USER CODE END ADC2_Init 2 */

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
  * @brief RTC Initialization Function
  * @param None
  * @retval None
  */
static void MX_RTC_Init(void)
{

  /* USER CODE BEGIN RTC_Init 0 */

  /* USER CODE END RTC_Init 0 */

  RTC_TimeTypeDef sTime = {0};
  RTC_DateTypeDef DateToUpdate = {0};

  /* USER CODE BEGIN RTC_Init 1 */

  /* USER CODE END RTC_Init 1 */
  /** Initialize RTC Only
  */
  hrtc.Instance = RTC;
  hrtc.Init.AsynchPrediv = RTC_AUTO_1_SECOND;
  hrtc.Init.OutPut = RTC_OUTPUTSOURCE_NONE;
  if (HAL_RTC_Init(&hrtc) != HAL_OK)
  {
    Error_Handler();
  }

  /* USER CODE BEGIN Check_RTC_BKUP */

  /* USER CODE END Check_RTC_BKUP */

  /** Initialize RTC and set the Time and Date
  */
  sTime.Hours = 0;
  sTime.Minutes = 0;
  sTime.Seconds = 0;

  if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN) != HAL_OK)
  {
    Error_Handler();
  }
  DateToUpdate.WeekDay = RTC_WEEKDAY_MONDAY;
  DateToUpdate.Month = RTC_MONTH_JANUARY;
  DateToUpdate.Date = 1;
  DateToUpdate.Year = 0;

  if (HAL_RTC_SetDate(&hrtc, &DateToUpdate, RTC_FORMAT_BIN) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN RTC_Init 2 */

  /* USER CODE END RTC_Init 2 */

}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 56249;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 255;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */
  HAL_TIM_MspPostInit(&htim1);

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_SlaveConfigTypeDef sSlaveConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 0;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 65535;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_OC_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sSlaveConfig.SlaveMode = TIM_SLAVEMODE_EXTERNAL1;
  sSlaveConfig.InputTrigger = TIM_TS_ITR2;
  if (HAL_TIM_SlaveConfigSynchro(&htim2, &sSlaveConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_TIMING;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_OC_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

}

/**
  * @brief TIM4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM4_Init(void)
{

  /* USER CODE BEGIN TIM4_Init 0 */

  /* USER CODE END TIM4_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM4_Init 1 */

  /* USER CODE END TIM4_Init 1 */
  htim4.Instance = TIM4;
  htim4.Init.Prescaler = 56249;
  htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim4.Init.Period = 255;
  htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim4) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim4, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim4, &sConfigOC, TIM_CHANNEL_3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM4_Init 2 */

  /* USER CODE END TIM4_Init 2 */
  HAL_TIM_MspPostInit(&htim4);

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, LCD_RS_Pin|LCD_RW_Pin|LCD_D0_Pin|LCD_D1_Pin
                          |LCD_D2_Pin|LCD_D3_Pin|LCD_D4_Pin|LCD_D5_Pin
                          |LCD_D6_Pin|LCD_D7_Pin|LCD_E_Pin|LCD_CS1_Pin
                          |LCD_CS2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(FAN_GPIO_Port, FAN_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(BEEP_GPIO_Port, BEEP_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : LCD_RS_Pin LCD_RW_Pin LCD_D0_Pin LCD_D1_Pin
                           LCD_D2_Pin LCD_D3_Pin LCD_D4_Pin LCD_D5_Pin
                           LCD_D6_Pin LCD_D7_Pin LCD_E_Pin LCD_CS1_Pin
                           LCD_CS2_Pin */
  GPIO_InitStruct.Pin = LCD_RS_Pin|LCD_RW_Pin|LCD_D0_Pin|LCD_D1_Pin
                          |LCD_D2_Pin|LCD_D3_Pin|LCD_D4_Pin|LCD_D5_Pin
                          |LCD_D6_Pin|LCD_D7_Pin|LCD_E_Pin|LCD_CS1_Pin
                          |LCD_CS2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : FAN_Pin */
  GPIO_InitStruct.Pin = FAN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(FAN_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : KEY_S_Pin KEY_F3_Pin KEY_F2_Pin KEY_F1_Pin */
  GPIO_InitStruct.Pin = KEY_S_Pin|KEY_F3_Pin|KEY_F2_Pin|KEY_F1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : BEEP_Pin */
  GPIO_InitStruct.Pin = BEEP_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(BEEP_GPIO_Port, &GPIO_InitStruct);

}

/* USER CODE BEGIN 4 */
typedef enum eMainMode {
	MAIN_HOME = 0,
	MAIN_ABOUT,
	MAIN_SETUP,
	MAIN_BAKE,
	MAIN_SELECT_PROFILE,
	MAIN_EDIT_PROFILE,
	MAIN_REFLOW
} MainMode_t;

static int32_t Main_Work(void) {
	static MainMode_t mode = MAIN_HOME;
	static uint16_t setpoint = 0;
	if (setpoint == 0) {
		Reflow_LoadSetpoint();
		setpoint = Reflow_GetSetpoint();
	}
	static int timer = 0;

	// profile editing
	static uint8_t profile_time_idx = 0;
	static uint8_t current_edit_profile;

	int32_t retval = TICKS_MS(500);

	char buf[22];
	int len;

	uint32_t keyspressed = Keypad_Get();

	char serial_cmd[255] = "";
	char* cmd_select_profile = "select profile %d";
	char* cmd_bake = "bake %d %d";
	char* cmd_dump_profile = "dump profile %d";
	char* cmd_setting = "setting %d %f";

#if 0
	if (uart_isrxready()) {
		int len = uart_readline(serial_cmd, 255);

		if (len > 0) {
			int param, param1;
			float paramF;

			if (strcmp(serial_cmd, "about") == 0) {
				printf(format_about, Version_GetGitVersion());
				len = IO_Partinfo(buf, sizeof(buf), "\nPart number: %s rev %c\n");
				printf(buf);
				EEPROM_Dump();

				printf("\nSensor values:\n");
				Sensor_ListAll();

			} else if (strcmp(serial_cmd, "help") == 0 || strcmp(serial_cmd, "?") == 0) {
				printf(help_text);

			} else if (strcmp(serial_cmd, "list profiles") == 0) {
				printf("\nReflow profiles available:\n");

				Reflow_ListProfiles();
				printf("\n");

			} else if (strcmp(serial_cmd, "reflow") == 0) {
				printf("\nStarting reflow with profile: %s\n", Reflow_GetProfileName());
				mode = MAIN_HOME;
				// this is a bit dirty, but with the least code duplication.
				keyspressed = KEY_S;

			} else if (strcmp(serial_cmd, "list settings") == 0) {
				printf("\nCurrent settings:\n\n");
				for (int i = 0; i < Setup_getNumItems() ; i++) {
					printf("%d: ", i);
					Setup_printFormattedValue(i);
					printf("\n");
				}

			} else if (strcmp(serial_cmd, "stop") == 0) {
				printf("\nStopping bake/reflow");
				mode = MAIN_HOME;
				Reflow_SetMode(REFLOW_STANDBY);
				retval = 0;

			} else if (strcmp(serial_cmd, "quiet") == 0) {
				Reflow_ToggleStandbyLogging();
				printf("\nToggled standby logging\n");

			} else if (strcmp(serial_cmd, "values") == 0) {
				printf("\nActual measured values:\n");
				Sensor_ListAll();
				printf("\n");

			} else if (sscanf(serial_cmd, cmd_select_profile, &param) > 0) {
				// select profile
				Reflow_SelectProfileIdx(param);
				printf("\nSelected profile %d: %s\n", param, Reflow_GetProfileName());

			} else if (sscanf(serial_cmd, cmd_bake, &param, &param1) > 0) {
				if (param < SETPOINT_MIN) {
					printf("\nSetpoint must be >= %ddegC\n", SETPOINT_MIN);
					param = SETPOINT_MIN;
				}
				if (param > SETPOINT_MAX) {
					printf("\nSetpont must be <= %ddegC\n", SETPOINT_MAX);
					param = SETPOINT_MAX;
				}
				if (param1 < 1) {
					printf("\nTimer must be greater than 0\n");
					param1 = 1;
				}

				if (param1 < BAKE_TIMER_MAX) {
					printf("\nStarting bake with setpoint %ddegC for %ds after reaching setpoint\n", param, param1);
					timer = param1;
					Reflow_SetBakeTimer(timer);
				} else {
					printf("\nStarting bake with setpoint %ddegC\n", param);
				}

				setpoint = param;
				Reflow_SetSetpoint(setpoint);
				mode = MAIN_BAKE;
				Reflow_SetMode(REFLOW_BAKE);

			} else if (sscanf(serial_cmd, cmd_dump_profile, &param) > 0) {
				printf("\nDumping profile %d: %s\n ", param, Reflow_GetProfileName());
				Reflow_DumpProfile(param);

			} else if (sscanf(serial_cmd, cmd_setting, &param, &paramF) > 0) {
				Setup_setRealValue(param, paramF);
				printf("\nAdjusted setting: ");
				Setup_printFormattedValue(param);

			} else {
				printf("\nCannot understand command, ? for help\n");
			}
		}
	}
#endif

	// main menu state machine
	if (mode == MAIN_SETUP) {
		static uint8_t selected = 0;
		int y = 0;

		int keyrepeataccel = keyspressed >> 17; // Divide the value by 2
		if (keyrepeataccel < 1) keyrepeataccel = 1;
		if (keyrepeataccel > 30) keyrepeataccel = 30;

#if 0
		if (keyspressed & KEY_F1) {
			if (selected > 0) { // Prev row
				selected--;
			} else { // wrap
				selected = Setup_getNumItems() - 1;
			}
		}
#endif
		if (keyspressed & KEY_F1) {
			if (selected < (Setup_getNumItems() - 1)) { // Next row
				selected++;
			} else { // wrap
				selected = 0;
			}
		}

		if (keyspressed & KEY_F2) {
			Setup_decreaseValue(selected, keyrepeataccel);
		}
		if (keyspressed & KEY_F3) {
			Setup_increaseValue(selected, keyrepeataccel);
		}

		LCD_FB_Clear();
		len = snprintf(buf, sizeof(buf), "Setup/calibration");
		LCD_disp_str((uint8_t*)buf, len, LCD_ALIGN_CENTER(len), y, FONT6X6);
		y += 7;

		for (int i = 0; i < Setup_getNumItems() ; i++) {
			len = Setup_snprintFormattedValue(buf, sizeof(buf), i);
			LCD_disp_str((uint8_t*)buf, len, 0, y, FONT6X6 | (selected == i) ? INVERT : 0);
			y += 7;
		}

		// buttons
		y = 64 - 7;
		//LCD_disp_str((uint8_t*)" < ", 3, 0, y, FONT6X6 | INVERT);
		LCD_disp_str((uint8_t*)" > ", 3, 20, y, FONT6X6 | INVERT);
		LCD_disp_str((uint8_t*)" - ", 3, 45, y, FONT6X6 | INVERT);
		LCD_disp_str((uint8_t*)" + ", 3, 65, y, FONT6X6 | INVERT);
		LCD_disp_str((uint8_t*)" DONE ", 6, 91, y, FONT6X6 | INVERT);

		// Leave setup
		if (keyspressed & KEY_S) {
			mode = MAIN_HOME;
			Reflow_SetMode(REFLOW_STANDBY);
			retval = 0; // Force immediate refresh
		}
	} else if (mode == MAIN_ABOUT) {
		LCD_FB_Clear();
		LCD_BMPDisplay(logobmp, 0, 0);

		len = snprintf(buf, sizeof(buf), "T-962A+ controller");
		LCD_disp_str((uint8_t*)buf, len, LCD_ALIGN_CENTER(len), 0, FONT6X6);

		len = snprintf(buf, sizeof(buf), "%s", Version_GetGitVersion());
		LCD_disp_str((uint8_t*)buf, len, LCD_ALIGN_CENTER(len), 64 - 6, FONT6X6);

		LCD_BMPDisplay(stopbmp, 127 - 17, 0);

		// Leave about with any key.
		if (keyspressed & KEY_ANY) {
			mode = MAIN_HOME;
			retval = 0; // Force immediate refresh
		}
	} else if (mode == MAIN_REFLOW) {
		uint32_t ticks = RTC_Read();

		len = snprintf(buf, sizeof(buf), "%03u", Reflow_GetSetpoint());
		LCD_disp_str((uint8_t*)"SET", 3, 110, 7, FONT6X6);
		LCD_disp_str((uint8_t*)buf, len, 110, 13, FONT6X6);

		len = snprintf(buf, sizeof(buf), "%03u", Reflow_GetActualTemp());
		LCD_disp_str((uint8_t*)"ACT", 3, 110, 20, FONT6X6);
		LCD_disp_str((uint8_t*)buf, len, 110, 26, FONT6X6);

		len = snprintf(buf, sizeof(buf), "%03u", (unsigned int)ticks);
		LCD_disp_str((uint8_t*)"RUN", 3, 110, 33, FONT6X6);
		LCD_disp_str((uint8_t*)buf, len, 110, 39, FONT6X6);

		// Abort reflow
		if (Reflow_IsDone() || keyspressed & KEY_S) {
			printf("\nReflow %s\n", (Reflow_IsDone() ? "done" : "interrupted by keypress"));
			if (Reflow_IsDone()) {
				Buzzer_Beep(BUZZ_1KHZ, 255, TICKS_MS(100) * NV_GetConfig(REFLOW_BEEP_DONE_LEN));
			}
			mode = MAIN_HOME;
			Reflow_SetMode(REFLOW_STANDBY);
			retval = 0; // Force immediate refresh
		}

	} else if (mode == MAIN_SELECT_PROFILE) {
		int curprofile = Reflow_GetProfileIdx();
		LCD_FB_Clear();

		// Prev profile
		if (keyspressed & KEY_F1) {
			curprofile--;
		}
		// Next profile
		if (keyspressed & KEY_F2) {
			curprofile++;
		}

		Reflow_SelectProfileIdx(curprofile);

		Reflow_PlotProfile(-1);
		LCD_BMPDisplay(selectbmp, 127 - 17, 0);
		int eeidx = Reflow_GetEEProfileIdx();
		if (eeidx) { // Display edit button
			LCD_BMPDisplay(f3editbmp, 127 - 17, 29);
		}
		len = snprintf(buf, sizeof(buf), "%s", Reflow_GetProfileName());
		LCD_disp_str((uint8_t*)buf, len, 13, 0, FONT6X6);

		if (eeidx && keyspressed & KEY_F3) { // Edit ee profile
			mode = MAIN_EDIT_PROFILE;
			current_edit_profile = eeidx;
			retval = 0; // Force immediate refresh
		}

		// Select current profile
		if (keyspressed & KEY_S) {
			mode = MAIN_HOME;
			retval = 0; // Force immediate refresh
		}

	} else if (mode == MAIN_BAKE) {
		LCD_FB_Clear();
		LCD_disp_str((uint8_t*)"MANUAL/BAKE MODE", 16, 0, 0, FONT6X6);

		int keyrepeataccel = keyspressed >> 17; // Divide the value by 2
		if (keyrepeataccel < 1) keyrepeataccel = 1;
		if (keyrepeataccel > 30) keyrepeataccel = 30;

		// Setpoint-
		if (keyspressed & KEY_F1) {
			setpoint -= keyrepeataccel;
			if (setpoint < SETPOINT_MIN) setpoint = SETPOINT_MIN;
		}

		// Setpoint+
		if (keyspressed & KEY_F2) {
			setpoint += keyrepeataccel;
			if (setpoint > SETPOINT_MAX) setpoint = SETPOINT_MAX;
		}

		// timer --
		if (keyspressed & KEY_F3) {
			if (timer - keyrepeataccel < 0) {
				// infinite bake
				timer = -1;
			} else {
				timer -= keyrepeataccel;
			}
		}
		// timer ++
		if (keyspressed & KEY_F4) {
			timer += keyrepeataccel;
		}

		int y = 10;
		// display F1 button only if setpoint can be decreased
		char f1function = ' ';
		if (setpoint > SETPOINT_MIN) {
			LCD_disp_str((uint8_t*)"F1", 2, 0, y, FONT6X6 | INVERT);
			f1function = '-';
		}
		// display F2 button only if setpoint can be increased
		char f2function = ' ';
		if (setpoint < SETPOINT_MAX) {
			LCD_disp_str((uint8_t*)"F2", 2, LCD_ALIGN_RIGHT(2), y, FONT6X6 | INVERT);
			f2function = '+';
		}
		len = snprintf(buf, sizeof(buf), "%c SETPOINT %d` %c", f1function, (int)setpoint, f2function);
		LCD_disp_str((uint8_t*)buf, len, LCD_ALIGN_CENTER(len), y, FONT6X6);


		y = 18;
		if (timer == 0) {
			len = snprintf(buf, sizeof(buf), "inf TIMER stop +");
		} else if (timer < 0) {
			len = snprintf(buf, sizeof(buf), "no timer    stop");
		} else {
			len = snprintf(buf, sizeof(buf), "- TIMER %3d:%02d +", timer / 60, timer % 60);
		}
		LCD_disp_str((uint8_t*)buf, len, LCD_ALIGN_CENTER(len), y, FONT6X6);

		if (timer >= 0) {
			LCD_disp_str((uint8_t*)"F3", 2, 0, y, FONT6X6 | INVERT);
		}
		LCD_disp_str((uint8_t*)"F4", 2, LCD_ALIGN_RIGHT(2), y, FONT6X6 | INVERT);

		y = 26;
		if (timer > 0) {
			int time_left = Reflow_GetTimeLeft();
			if (Reflow_IsPreheating()) {
				len = snprintf(buf, sizeof(buf), "PREHEAT");
			} else if (Reflow_IsDone() || time_left < 0) {
				len = snprintf(buf, sizeof(buf), "DONE");
			} else {
				len = snprintf(buf, sizeof(buf), "%d:%02d", time_left / 60, time_left % 60);
			}
			LCD_disp_str((uint8_t*)buf, len, LCD_ALIGN_RIGHT(len), y, FONT6X6);
		}

		len = snprintf(buf, sizeof(buf), "ACT %3.1f`", Sensor_GetTemp(TC_AVERAGE));
		LCD_disp_str((uint8_t*)buf, len, 0, y, FONT6X6);

		y = 34;
		len = snprintf(buf, sizeof(buf), "  L %3.1f`", Sensor_GetTemp(TC_LEFT));
		LCD_disp_str((uint8_t*)buf, len, 0, y, FONT6X6);
		len = snprintf(buf, sizeof(buf), "  R %3.1f`", Sensor_GetTemp(TC_RIGHT));
		LCD_disp_str((uint8_t*)buf, len, LCD_CENTER, y, FONT6X6);

		if (Sensor_IsValid(TC_EXTRA1) || Sensor_IsValid(TC_EXTRA2)) {
			y = 42;
			if (Sensor_IsValid(TC_EXTRA1)) {
				len = snprintf(buf, sizeof(buf), " X1 %3.1f`", Sensor_GetTemp(TC_EXTRA1));
				LCD_disp_str((uint8_t*)buf, len, 0, y, FONT6X6);
			}
			if (Sensor_IsValid(TC_EXTRA2)) {
				len = snprintf(buf, sizeof(buf), " X2 %3.1f`", Sensor_GetTemp(TC_EXTRA2));
				LCD_disp_str((uint8_t*)buf, len, LCD_CENTER, y, FONT6X6);
			}
		}

		y = 50;
		len = snprintf(buf, sizeof(buf), "COLDJUNCTION");
		LCD_disp_str((uint8_t*)buf, len, 0, y, FONT6X6);

		y += 8;
		if (Sensor_IsValid(TC_COLD_JUNCTION)) {
			len = snprintf(buf, sizeof(buf), "%3.1f`", Sensor_GetTemp(TC_COLD_JUNCTION));
		} else {
			len = snprintf(buf, sizeof(buf), "NOT PRESENT");
		}
		LCD_disp_str((uint8_t*)buf, len, (12 * 6) - (len * 6), y, FONT6X6);

		LCD_BMPDisplay(stopbmp, 127 - 17, 0);

		Reflow_SetSetpoint(setpoint);

		if (timer > 0 && Reflow_IsDone()) {
			Buzzer_Beep(BUZZ_1KHZ, 255, TICKS_MS(100) * NV_GetConfig(REFLOW_BEEP_DONE_LEN));
			Reflow_SetBakeTimer(0);
			Reflow_SetMode(REFLOW_STANDBY);
		}

		if (keyspressed & KEY_F3 || keyspressed & KEY_F4) {
			if (timer == 0) {
				Reflow_SetMode(REFLOW_STANDBY);
			} else {
				if (timer == -1) {
					Reflow_SetBakeTimer(0);
				} else if (timer > 0) {
					Reflow_SetBakeTimer(timer);
					printf("\nSetting bake timer to %d\n", timer);
				}
				Reflow_SetMode(REFLOW_BAKE);
			}
		}

		// Abort bake
		if (keyspressed & KEY_S) {
			printf("\nEnd bake mode by keypress\n");

			mode = MAIN_HOME;
			Reflow_SetBakeTimer(0);
			Reflow_SetMode(REFLOW_STANDBY);
			retval = 0; // Force immediate refresh
		}

	} else if (mode == MAIN_EDIT_PROFILE) { // Edit ee1 or 2
		LCD_FB_Clear();
		int keyrepeataccel = keyspressed >> 17; // Divide the value by 2
		if (keyrepeataccel < 1) keyrepeataccel = 1;
		if (keyrepeataccel > 30) keyrepeataccel = 30;

		int16_t cursetpoint;
		Reflow_SelectEEProfileIdx(current_edit_profile);
		if (keyspressed & KEY_F1 && profile_time_idx > 0) { // Prev time
			profile_time_idx--;
		}
		if (keyspressed & KEY_F2 && profile_time_idx < 47) { // Next time
			profile_time_idx++;
		}
		cursetpoint = Reflow_GetSetpointAtIdx(profile_time_idx);

		if (keyspressed & KEY_F3) { // Decrease setpoint
			cursetpoint -= keyrepeataccel;
		}
		if (keyspressed & KEY_F4) { // Increase setpoint
			cursetpoint += keyrepeataccel;
		}
		if (cursetpoint < 0) cursetpoint = 0;
		if (cursetpoint > SETPOINT_MAX) cursetpoint = SETPOINT_MAX;
		Reflow_SetSetpointAtIdx(profile_time_idx, cursetpoint);

		Reflow_PlotProfile(profile_time_idx);
		LCD_BMPDisplay(editbmp, 127 - 17, 0);

		len = snprintf(buf, sizeof(buf), "%02u0s %03u`", profile_time_idx, cursetpoint);
		LCD_disp_str((uint8_t*)buf, len, 13, 0, FONT6X6);

		// Done editing
		if (keyspressed & KEY_S) {
			Reflow_SaveEEProfile();
			mode = MAIN_HOME;
			retval = 0; // Force immediate refresh
		}

	} else { // Main menu
		LCD_FB_Clear();

		len = snprintf(buf, sizeof(buf),"MAIN MENU");
		LCD_disp_str((uint8_t*)buf, len, 0, 6 * 0, FONT6X6);
		LCD_disp_str((uint8_t*)"F1", 2, 0, 8 * 1, FONT6X6 | INVERT);
		LCD_disp_str((uint8_t*)"ABOUT", 5, 14, 8 * 1, FONT6X6);
		LCD_disp_str((uint8_t*)"F2", 2, 0, 8 * 2, FONT6X6 | INVERT);
		LCD_disp_str((uint8_t*)"SETUP", 5, 14, 8 * 2, FONT6X6);
		LCD_disp_str((uint8_t*)"F3", 2, 0, 8 * 3, FONT6X6 | INVERT);
		//LCD_disp_str((uint8_t*)"BAKE/MANUAL MODE", 16, 14, 8 * 3, FONT6X6);
		//LCD_disp_str((uint8_t*)"F4", 2, 0, 8 * 4, FONT6X6 | INVERT);
		LCD_disp_str((uint8_t*)"SELECT PROFILE", 14, 14, 8 * 4, FONT6X6);
		LCD_disp_str((uint8_t*)"S", 1, 3, 8 * 5, FONT6X6 | INVERT);
		LCD_disp_str((uint8_t*)"RUN REFLOW PROFILE", 18, 14, 8 * 5, FONT6X6);

		len = snprintf(buf, sizeof(buf), "%s", Reflow_GetProfileName());
		LCD_disp_str((uint8_t*)buf, len, LCD_ALIGN_CENTER(len), 8 * 6, FONT6X6 | INVERT);

		len = snprintf(buf,sizeof(buf), "OVEN TEMPERATURE %d`", Reflow_GetActualTemp());
		LCD_disp_str((uint8_t*)buf, len, LCD_ALIGN_CENTER(len), 64 - 6, FONT6X6);

		// Make sure reflow complete beep is silenced when pressing any key
		if (keyspressed) {
			Buzzer_Beep(BUZZ_NONE, 0, 0);
		}

		// About
		if (keyspressed & KEY_F1) {
			mode = MAIN_ABOUT;
			retval = 0; // Force immediate refresh
		}
		if (keyspressed & KEY_F2) { // Setup/cal
			mode = MAIN_SETUP;
			Reflow_SetMode(REFLOW_STANDBYFAN);
			retval = 0; // Force immediate refresh
		}

#if 0
		// Bake mode
		if (keyspressed & KEY_F3) {
			mode = MAIN_BAKE;
			Reflow_Init();
			retval = 0; // Force immediate refresh
		}
#endif
		// Select profile
		if (keyspressed & KEY_F3) {
			mode = MAIN_SELECT_PROFILE;
			retval = 0; // Force immediate refresh
		}

		// Start reflow
		if (keyspressed & KEY_S) {
			mode = MAIN_REFLOW;
			LCD_FB_Clear();
			printf("\nStarting reflow with profile: %s", Reflow_GetProfileName());
			Reflow_Init();
			Reflow_PlotProfile(-1);
			LCD_BMPDisplay(stopbmp, 127 - 17, 0);
			len = snprintf(buf, sizeof(buf), "%s", Reflow_GetProfileName());
			LCD_disp_str((uint8_t*)buf, len, 13, 0, FONT6X6);
			Reflow_SetMode(REFLOW_REFLOW);
			retval = 0; // Force immediate refresh
		}
	}

	LCD_FB_Update();

	return retval;
}

/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN 5 */
	printf("Inside DefaultTask\r\n");

  /* Infinite loop */
  for(;;)
  {
    //osDelay(1);
    // Cool fan is controlled by TIM1_CH1, HEAT is controlled by TIM4_CH3
    //COOL_GPIO_Port->ODR = (GPIOB->IDR & GPIO_PIN_15) ? 0 : COOL_Pin;
    //    TIM4->CCR3 = (GPIOB->IDR & GPIO_PIN_15) ? 0x0100 : 0x2000;
    //    TIM1->CCR1 = (GPIOB->IDR & GPIO_PIN_13) ? 0x1000 : 0x5000;
    //Set_Heater( (GPIOB->IDR & GPIO_PIN_15) ? 0x10 : 0xff );
    //Set_Fan( (GPIOB->IDR & GPIO_PIN_13) ? 0x10 : 0x80 );

    GPIOA->BSRR = (GPIO_PIN_5 << ((GPIOB->IDR & GPIO_PIN_8) ? 0 : 16)); // Mimic SSR Heat pin on "FAN" pin as that's the one driving the Heat LED!??!??

    //printf("RTC: 0x%08x ticks=0x%08x tickcount=0x%08x\r\n", RTC_Read(), Sched_GetTick(), xTaskGetTickCount());
    Sched_Do(0);
  }
  /* USER CODE END 5 */
}

 /**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM3 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM3) {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

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

#ifdef  USE_FULL_ASSERT
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

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
