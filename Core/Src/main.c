/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  */
/* USER CODE END Header */
#include "main.h"
#include "i2c.h"
#include "ipcc.h"
#include "iwdg.h"
#include "rf.h"
#include "rtc.h"
#include "gpio.h"

/* USER CODE BEGIN Includes */
#include "ble.h"
#include "sht41.h"
#include "app_ble.h"
/* USER CODE END Includes */

/* USER CODE BEGIN PD */
#define SENSOR_PERIOD_MS 10000U
#define LED_BOOT_BLINK_MS 100U
#define SENSOR_PWR_GPIO_Port GPIOA
#define SENSOR_PWR_Pin GPIO_PIN_4
#define SENSOR_I2C_SCL_PORT GPIOB
#define SENSOR_I2C_SCL_PIN GPIO_PIN_8
#define SENSOR_I2C_SDA_PORT GPIOB
#define SENSOR_I2C_SDA_PIN GPIO_PIN_9
/* USER CODE END PD */

/* USER CODE BEGIN PV */
uint16_t adc_inp;
volatile uint8_t force_measure_now = 0;
volatile uint8_t device_sleep_mode = 0;
static SHT41_Data_t sensor_data;
static uint8_t sensor_data_valid = 0;
static volatile uint32_t led_off_tick = 0;
static volatile uint8_t led_state = 0;
/* USER CODE END PV */

void SystemClock_Config(void);
void PeriphCommonClock_Config(void);

/* USER CODE BEGIN 0 */
static inline void SENSOR_PWR_ON(void)
{
  HAL_GPIO_WritePin(SENSOR_PWR_GPIO_Port, SENSOR_PWR_Pin, GPIO_PIN_RESET);
}

static inline void SENSOR_PWR_OFF(void)
{
  HAL_GPIO_WritePin(SENSOR_PWR_GPIO_Port, SENSOR_PWR_Pin, GPIO_PIN_SET);
}

static void I2C_Lines_To_AF(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;
  GPIO_InitStruct.Pin = SENSOR_I2C_SDA_PIN;
  HAL_GPIO_Init(SENSOR_I2C_SDA_PORT, &GPIO_InitStruct);
  GPIO_InitStruct.Pin = SENSOR_I2C_SCL_PIN;
  HAL_GPIO_Init(SENSOR_I2C_SCL_PORT, &GPIO_InitStruct);
}

static void Sensor_PowerOn(void)
{
  I2C_Lines_To_AF();
  SENSOR_PWR_ON();
  HAL_Delay(2);
  HAL_I2C_DeInit(&hi2c1);
  MX_I2C1_Init();
  HAL_Delay(1);
}

static void LED_Process(void)
{
  uint32_t now = HAL_GetTick();
  if (led_state && (int32_t)(now - led_off_tick) >= 0)
  {
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_RESET);
    led_state = 0;
  }
}

static void Sensor_Process(void)
{
  if (SHT41_Read(&hi2c1, &sensor_data) == HAL_OK)
  {
    sensor_data_valid = 1;
  }
  else
  {
    sensor_data_valid = 0;
  }
}
/* USER CODE END 0 */

int main(void)
{
  uint32_t last_sensor_tick = 0;

  HAL_Init();
  MX_APPE_Config();
  SystemClock_Config();
  PeriphCommonClock_Config();
  MX_IPCC_Init();

  MX_GPIO_Init();
  MX_RTC_Init();
  MX_I2C1_Init();
  MX_IWDG_Init();
  MX_RF_Init();

  LL_HSEM_1StepLock(HSEM, 5);
  LL_HSEM_ReleaseLock(HSEM, 5, 0);

  __HAL_RCC_GPIOA_CLK_ENABLE();
  {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = SENSOR_PWR_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(SENSOR_PWR_GPIO_Port, &GPIO_InitStruct);
  }
  SENSOR_PWR_OFF();
  Sensor_PowerOn();
  (void)SHT41_Init(&hi2c1);

  for (uint8_t i = 0; i < 5; i++)
  {
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_SET);
    HAL_Delay(LED_BOOT_BLINK_MS);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_RESET);
    HAL_Delay(LED_BOOT_BLINK_MS);
  }

  /* BLE application initialization must happen exactly once. */
  MX_APPE_Init();

  while (1)
  {
    MX_APPE_Process();
    LED_Process();

    uint32_t now = HAL_GetTick();
    if ((int32_t)(now - last_sensor_tick) >= (int32_t)SENSOR_PERIOD_MS)
    {
      last_sensor_tick = now;
      Sensor_Process();
    }
  }
}

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  HAL_PWR_EnableBkUpAccess();
  __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_MEDIUMHIGH);
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /* These settings match the working main branch and are required by BLE. */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI48|RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_HSE|RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV2;
  RCC_OscInitStruct.PLL.PLLN = 8;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) Error_Handler();

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK4|RCC_CLOCKTYPE_HCLK2|RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.AHBCLK2Divider = RCC_SYSCLK_DIV2;
  RCC_ClkInitStruct.AHBCLK4Divider = RCC_SYSCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK) Error_Handler();
}

void PeriphCommonClock_Config(void)
{
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_SMPS|RCC_PERIPHCLK_RFWAKEUP;
  PeriphClkInitStruct.RFWakeUpClockSelection = RCC_RFWKPCLKSOURCE_HSE_DIV1024;
  PeriphClkInitStruct.SmpsClockSelection = RCC_SMPSCLKSOURCE_HSE;
  PeriphClkInitStruct.SmpsDivSelection = RCC_SMPSCLKDIV_RANGE1;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK) Error_Handler();
}

void Dbg_Print(const char *fmt, ...)
{
  (void)fmt;
}

void Error_Handler(void)
{
  __disable_irq();
  while (1) {}
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
  (void)file;
  (void)line;
}
#endif