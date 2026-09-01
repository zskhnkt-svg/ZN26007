/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
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
#include "i2c.h"
#include "ipcc.h"
#include "rf.h"
#include "rtc.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ble.h"
#include "sht41.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define NAME_STORAGE_PAGE_ADDR   0x080CD000   /* последняя страница CPU1-области перед CPU2 */
#define NAME_STORAGE_MAX_LEN     20
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void PeriphCommonClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
uint16_t adc_inp;
uint8_t force_measure_now = 0;
RTC_DateTypeDef sdatestructureget;
RTC_TimeTypeDef stimestructureget;

#define SENSOR_PERIOD_MS   60000U  /* опрос датчика и отправка раз в минуту */
#define LED_BLINK_MS       15U     /* короткий блик после отправки */
#define LED_BOOT_BLINK_MS  100U    /* длительность каждого блика при старте */

/* Управление P-MOSFET ключом питания датчика SHT41.
   HIGH = закрыт (датчик выключен), LOW = открыт (датчик включен) */
#define SENSOR_PWR_GPIO_Port  GPIOA
#define SENSOR_PWR_Pin        GPIO_PIN_4
#define SENSOR_PWR_ON()   HAL_GPIO_WritePin(SENSOR_PWR_GPIO_Port, SENSOR_PWR_Pin, GPIO_PIN_RESET)
#define SENSOR_PWR_OFF()  HAL_GPIO_WritePin(SENSOR_PWR_GPIO_Port, SENSOR_PWR_Pin, GPIO_PIN_SET)
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
	uint32_t tick,tick_now = 0;
	tick = 0;
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();
  /* Config code for STM32_WPAN (HSE Tuning must be done before system clock configuration) */
  MX_APPE_Config();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* Configure the peripherals common clocks */
  PeriphCommonClock_Config();

  /* IPCC initialisation */
  MX_IPCC_Init();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_RTC_Init();
  MX_I2C1_Init();
  MX_RF_Init();
  /* USER CODE BEGIN 2 */
  /* HSEM 5 = CFG_HW_CLK48_CONFIG_SEMID (см. AN5289 §4.3) — общий клок CLK48
     между USB (CPU1) и RNG радио-стека (CPU2). Берём только на время
     критической секции (ADC calibration/DMA start) и сразу отпускаем,
     иначе CPU2 не может договориться о клоке и BLE-стек не поднимается. */
  LL_HSEM_1StepLock( HSEM, 5 );

	//HAL_ADCEx_Calibration_Start(&hadc1,ADC_SINGLE_ENDED);
	//HAL_ADC_Start_DMA(&hadc1,(uint32_t *)&adc_inp,1);

  LL_HSEM_ReleaseLock( HSEM, 5, 0 );

	extern uint8_t led_blink_en;
	extern uint8_t Notification_Status;

  /* Настройка ключа питания датчика (PA4).
     Сначала выставляем безопасное состояние (выключено), потом настраиваем режим —
     так не будет короткого "мигания" в неопределённом состоянии при инициализации. */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  SENSOR_PWR_OFF();
  {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = SENSOR_PWR_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(SENSOR_PWR_GPIO_Port, &GPIO_InitStruct);
  }

  SENSOR_PWR_ON();
  HAL_Delay(2000); /* дать сенсору время на power-up */
  SHT41_Init(&hi2c1);

  /* 5 бликов при старте — визуальное подтверждение, что плата включилась */
  for (uint8_t i = 0; i < 5; i++)
  {
      HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_SET);
      HAL_Delay(LED_BOOT_BLINK_MS);
      HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_RESET);
      HAL_Delay(LED_BOOT_BLINK_MS);
  }

  /* USER CODE END 2 */

  /* Init code for STM32_WPAN */
  MX_APPE_Init();

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */
    MX_APPE_Process();

    /* USER CODE BEGIN 3 */
		tick_now = HAL_GetTick();
		if(tick_now >= tick || force_measure_now)
		{
      force_measure_now = 0;
			tick = tick_now + SENSOR_PERIOD_MS;

			uint8_t text[60];
			int text_lenth;
			memset(text, 0, sizeof(text));

			/* Get the RTC current Time */
			HAL_RTC_GetTime(&hrtc, &stimestructureget, RTC_FORMAT_BIN);
			/* Get the RTC current Date */
			HAL_RTC_GetDate(&hrtc, &sdatestructureget, RTC_FORMAT_BIN);

			SENSOR_PWR_ON();
			HAL_Delay(2); /* время на старт SHT41 после подачи VDD */

			SHT41_Data_t sht_data;
			HAL_StatusTypeDef sht_status = SHT41_Read(&hi2c1, &sht_data);

			SENSOR_PWR_OFF();

			if (sht_status == HAL_OK) {
			    int16_t t_int = (int16_t)(sht_data.temperature * 10.0f);
			    uint16_t h_int = (uint16_t)(sht_data.humidity * 10.0f);
			    text_lenth = sprintf((char *)&text,
			        "20%02d.%02d.%02d %02d:%02d:%02d T=%d.%d H=%d.%d\r\n",
			        sdatestructureget.Year, sdatestructureget.Month, sdatestructureget.Date,
			        stimestructureget.Hours, stimestructureget.Minutes, stimestructureget.Seconds,
			        t_int / 10, t_int % 10, h_int / 10, h_int % 10);

      
			} else {
			    text_lenth = sprintf((char *)&text, "SHT41 err=%d\r\n", sht_status);
			}

			if(Notification_Status)
			{
			    P2PS_STM_App_Update_Char(P2P_NOTIFY_CHAR_UUID, text, (uint8_t)text_lenth);

			    /* короткий блик — только когда реально ушла отправка на телефон */
			    if(led_blink_en)
			    {
			        HAL_GPIO_WritePin(GPIOA,GPIO_PIN_10,GPIO_PIN_SET);
			        HAL_Delay(LED_BLINK_MS);
			        HAL_GPIO_WritePin(GPIOA,GPIO_PIN_10,GPIO_PIN_RESET);
			    }
			}
		}
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

  /** Configure LSE Drive Capability
  */
  HAL_PWR_EnableBkUpAccess();
  __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_MEDIUMHIGH);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_HSE
                              |RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV2;
  RCC_OscInitStruct.PLL.PLLN = 8;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure the SYSCLKSource, HCLK, PCLK1 and PCLK2 clocks dividers
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK4|RCC_CLOCKTYPE_HCLK2
                              |RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.AHBCLK2Divider = RCC_SYSCLK_DIV2;
  RCC_ClkInitStruct.AHBCLK4Divider = RCC_SYSCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief Peripherals Common Clock Configuration
  * @retval None
  */
void PeriphCommonClock_Config(void)
{
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

  /** Initializes the peripherals clock
  */
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_SMPS|RCC_PERIPHCLK_RFWAKEUP;
  PeriphClkInitStruct.RFWakeUpClockSelection = RCC_RFWKPCLKSOURCE_LSE;
  PeriphClkInitStruct.SmpsClockSelection = RCC_SMPSCLKSOURCE_HSE;
  PeriphClkInitStruct.SmpsDivSelection = RCC_SMPSCLKDIV_RANGE1;

  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN Smps */

  /* USER CODE END Smps */
}

/* USER CODE BEGIN 4 */

/**
  * @brief  No-op stub for APP_DBG_MSG in the power-optimized build.
  *         Keeps link compatibility with any leftover diagnostic calls
  *         (e.g. in app_entry.c) without touching USB/CDC or burning cycles.
  * @param  fmt: unused
  * @retval None
  */
void Dbg_Print(const char *fmt, ...)
{
  (void)fmt;
}

void APP_SaveDeviceName(const char *name, uint8_t len)
{
  uint64_t data = 0;
  FLASH_EraseInitTypeDef erase = {0};
  uint32_t page_error;

  HAL_FLASH_Unlock();

  erase.TypeErase = FLASH_TYPEERASE_PAGES;
  erase.Page = (NAME_STORAGE_PAGE_ADDR - FLASH_BASE) / FLASH_PAGE_SIZE;
  erase.NbPages = 1;
  HAL_FLASHEx_Erase(&erase, &page_error);

  for (uint32_t i = 0; i < NAME_STORAGE_MAX_LEN; i += 8)
  {
    memcpy(&data, name + i, 8);
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD,
                       NAME_STORAGE_PAGE_ADDR + i, data);
  }

  HAL_FLASH_Lock();
}

void APP_LoadDeviceName(char *out_name, uint8_t max_len)
{
  memcpy(out_name, (void*)NAME_STORAGE_PAGE_ADDR, max_len);
  if ((uint8_t)out_name[0] == 0xFF)
  {
    strcpy(out_name, "Numa-Sensor");
  }
}

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