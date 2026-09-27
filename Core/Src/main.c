/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "i2c.h"
#include "ipcc.h"
#include "iwdg.h"
#include "rf.h"
#include "rtc.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ble.h"
#include "sht41.h"
#include "app_ble.h"   /* <-- ДОБАВИТЬ */
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */


#define SENSOR_PERIOD_MS         10000U   /* 10 сек для теста, в релизе 60000U */
#define LED_BLINK_MS             15U
#define LED_BOOT_BLINK_MS        100U

/* P-MOSFET: LOW = ON (датчик запитан), HIGH = OFF */
#define SENSOR_PWR_GPIO_Port     GPIOA
#define SENSOR_PWR_Pin           GPIO_PIN_4

/* I2C1 пины (WB55): PB8=SCL, PB9=SDA — измените если у вас другие! */
#define SENSOR_I2C_SCL_PORT      GPIOB
#define SENSOR_I2C_SCL_PIN       GPIO_PIN_8
#define SENSOR_I2C_SDA_PORT      GPIOB
#define SENSOR_I2C_SDA_PIN       GPIO_PIN_9
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
uint16_t adc_inp;
volatile uint8_t force_measure_now = 0;
volatile uint8_t device_sleep_mode = 0;   /* 0 = NORMAL, 1 = SLEEP */

static char     last_text[64];      /* буфер последнего измерения */
static int      last_text_len = 0;
static uint8_t  last_text_ready = 0; /* 1 = есть свежие данные для отправки */

RTC_DateTypeDef sdatestructureget;
RTC_TimeTypeDef stimestructureget;

/* Неблокирующий LED */
static volatile uint32_t led_off_tick = 0;
static volatile uint8_t  led_state    = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void PeriphCommonClock_Config(void);
/* USER CODE BEGIN PFP */
static void LED_Process(void);
static void I2C_Lines_To_GND(void);
static void I2C_Lines_To_AF(void);
static void Sensor_PowerOn(void);
static void Sensor_PowerOff(void);

extern APP_BLE_ConnStatus_t APP_BLE_Get_Server_Connection_Status(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

static inline void SENSOR_PWR_ON(void)
{
    HAL_GPIO_WritePin(SENSOR_PWR_GPIO_Port, SENSOR_PWR_Pin, GPIO_PIN_RESET);
}

static inline void SENSOR_PWR_OFF(void)
{
    HAL_GPIO_WritePin(SENSOR_PWR_GPIO_Port, SENSOR_PWR_Pin, GPIO_PIN_SET);
}

/* Переводим I2C в GPIO LOW перед отключением питания — иначе back-powering */
static void I2C_Lines_To_GND(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    HAL_GPIO_WritePin(SENSOR_I2C_SDA_PORT, SENSOR_I2C_SDA_PIN, GPIO_PIN_RESET);
    GPIO_InitStruct.Pin       = SENSOR_I2C_SDA_PIN;
    GPIO_InitStruct.Mode      = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(SENSOR_I2C_SDA_PORT, &GPIO_InitStruct);

    HAL_GPIO_WritePin(SENSOR_I2C_SCL_PORT, SENSOR_I2C_SCL_PIN, GPIO_PIN_RESET);
    GPIO_InitStruct.Pin       = SENSOR_I2C_SCL_PIN;
    HAL_GPIO_Init(SENSOR_I2C_SCL_PORT, &GPIO_InitStruct);
}

/* Возвращаем I2C в Alternate Function */
static void I2C_Lines_To_AF(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Mode      = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull      = GPIO_PULLUP;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_LOW;
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
    HAL_Delay(2);                 /* t_POWER_UP SHT41 */
    MX_I2C1_Init();
    HAL_Delay(1);
}

static void Sensor_PowerOff(void)
{
    HAL_I2C_DeInit(&hi2c1);
    I2C_Lines_To_GND();
    HAL_Delay(1);
    SENSOR_PWR_OFF();
    HAL_Delay(1);
}

static void LED_Process(void)  
{  
    uint32_t now = HAL_GetTick();
    // Правильное сравнение с учётом переполнения
    if (led_state && (int32_t)(now - led_off_tick) >= 0)  
    {  
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_RESET);  
        led_state = 0;  
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
    uint32_t last_sensor_tick = 0;
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
  MX_IWDG_Init();
  MX_RF_Init();
  /* USER CODE BEGIN 2 */
    /* HSEM 5 = CFG_HW_CLK48_CONFIG_SEMID */
    LL_HSEM_1StepLock(HSEM, 5);
    LL_HSEM_ReleaseLock(HSEM, 5, 0);

    /* Настройка пина питания датчика */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    SENSOR_PWR_OFF();
    {
        GPIO_InitTypeDef GPIO_InitStruct = {0};
        GPIO_InitStruct.Pin       = SENSOR_PWR_Pin;
        GPIO_InitStruct.Mode      = GPIO_MODE_OUTPUT_PP;
        GPIO_InitStruct.Pull      = GPIO_NOPULL;
        GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(SENSOR_PWR_GPIO_Port, &GPIO_InitStruct);
    }

    /* Включаем датчик, инициализируем */
    Sensor_PowerOn();
    SHT41_Init(&hi2c1);

    /* 5 коротких бликов при старте — только HAL_Delay, без __WFI! */
    for (uint8_t i = 0; i < 5; i++)
    {
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_SET);
        HAL_Delay(LED_BOOT_BLINK_MS);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_RESET);
        HAL_Delay(LED_BOOT_BLINK_MS);
    }

    MX_APPE_Init();
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
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSI1
                              |RCC_OSCILLATORTYPE_HSE|RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
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
void Dbg_Print(const char *fmt, ...)
{
  (void)fmt;
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
