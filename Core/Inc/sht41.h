/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    sht41.h
  * @brief   This file contains all the function prototypes for
  *          the SHT41 sensor driver
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __SHT41_H__
#define __SHT41_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "i2c.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Define SHT41 I2C address and commands */
#define SHT41_ADDR          0x44  /* SHT41 I2C address */
#define SHT41_ADDR_7BIT     (SHT41_ADDR << 1)  /* 7-bit address for HAL */

/* SHT41 Commands */
#define SHT41_CMD_READ_SERIAL       0x3682  /* Read serial number */
#define SHT41_CMD_RESET             0x94BA  /* Soft reset */
#define SHT41_CMD_MEASURE_HIGH      0xFD    /* Measure with high precision */
#define SHT41_CMD_MEASURE_MEDIUM    0xF6    /* Measure with medium precision */
#define SHT41_CMD_MEASURE_LOW       0xE0    /* Measure with low precision */
#define SHT41_CMD_HEATER_ON         0x39E4  /* Heater on */
#define SHT41_CMD_HEATER_OFF        0x3668  /* Heater off */

/* Structure to hold sensor data */
typedef struct {
  float temperature;  /* Temperature in °C */
  float humidity;     /* Humidity in %RH */
} SHT41_Data_t;

/* Function prototypes */
HAL_StatusTypeDef SHT41_Init(I2C_HandleTypeDef *hi2c);
HAL_StatusTypeDef SHT41_Reset(I2C_HandleTypeDef *hi2c);
HAL_StatusTypeDef SHT41_Read(I2C_HandleTypeDef *hi2c, SHT41_Data_t *data);
HAL_StatusTypeDef SHT41_ReadRaw(I2C_HandleTypeDef *hi2c, uint16_t *temp_raw, uint16_t *humid_raw);
float SHT41_ConvertTemperature(uint16_t raw_temp);
float SHT41_ConvertHumidity(uint16_t raw_humid);

/* USER CODE BEGIN Prototypes */

/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __SHT41_H__ */
