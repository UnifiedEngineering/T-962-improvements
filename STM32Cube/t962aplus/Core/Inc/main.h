/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define LCD_RS_Pin GPIO_PIN_13
#define LCD_RS_GPIO_Port GPIOC
#define LCD_RW_Pin GPIO_PIN_14
#define LCD_RW_GPIO_Port GPIOC
#define LCD_D0_Pin GPIO_PIN_0
#define LCD_D0_GPIO_Port GPIOC
#define LCD_D1_Pin GPIO_PIN_1
#define LCD_D1_GPIO_Port GPIOC
#define LCD_D2_Pin GPIO_PIN_2
#define LCD_D2_GPIO_Port GPIOC
#define LCD_D3_Pin GPIO_PIN_3
#define LCD_D3_GPIO_Port GPIOC
#define FAN_Pin GPIO_PIN_5
#define FAN_GPIO_Port GPIOA
#define LCD_D4_Pin GPIO_PIN_4
#define LCD_D4_GPIO_Port GPIOC
#define LCD_D5_Pin GPIO_PIN_5
#define LCD_D5_GPIO_Port GPIOC
#define KEY_S_Pin GPIO_PIN_10
#define KEY_S_GPIO_Port GPIOB
#define KEY_F3_Pin GPIO_PIN_11
#define KEY_F3_GPIO_Port GPIOB
#define KEY_F2_Pin GPIO_PIN_13
#define KEY_F2_GPIO_Port GPIOB
#define KEY_F1_Pin GPIO_PIN_15
#define KEY_F1_GPIO_Port GPIOB
#define LCD_D6_Pin GPIO_PIN_6
#define LCD_D6_GPIO_Port GPIOC
#define LCD_D7_Pin GPIO_PIN_7
#define LCD_D7_GPIO_Port GPIOC
#define LCD_E_Pin GPIO_PIN_9
#define LCD_E_GPIO_Port GPIOC
#define LCD_CS1_Pin GPIO_PIN_10
#define LCD_CS1_GPIO_Port GPIOC
#define LCD_CS2_Pin GPIO_PIN_11
#define LCD_CS2_GPIO_Port GPIOC
#define BEEP_Pin GPIO_PIN_9
#define BEEP_GPIO_Port GPIOB
/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
