/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    sht41.c
  * @brief   SHT41 sensor driver implementation
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "sht41.h"
#include "string.h"

/* USER CODE BEGIN 0 */

/* CRC-8 calculation for SHT sensors */
static uint8_t SHT41_Crc8(const uint8_t *data, int len)
{
    uint8_t crc = 0xFF;
    for (int i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x31;
            } else {
                crc = crc << 1;
            }
        }
    }
    return crc;
}

/* USER CODE END 0 */

/**
  * @brief  Initialize SHT41 sensor
  * @param  hi2c pointer to I2C handle
  * @retval HAL status
  */
HAL_StatusTypeDef SHT41_Init(I2C_HandleTypeDef *hi2c)
{
    /* Perform soft reset */
    return SHT41_Reset(hi2c);
}

/**
  * @brief  Perform soft reset of SHT41
  * @param  hi2c pointer to I2C handle
  * @retval HAL status
  */
HAL_StatusTypeDef SHT41_Reset(I2C_HandleTypeDef *hi2c)
{
    uint8_t cmd[2] = {0x94, 0xBA};  /* Reset command */
    HAL_StatusTypeDef status;
    
    status = HAL_I2C_Master_Transmit(hi2c, SHT41_ADDR_7BIT, cmd, 2, 100);
    
    if (status == HAL_OK) {
        HAL_Delay(1);  /* Wait for reset to complete */
    }
    
    return status;
}

/**
  * @brief  Read raw sensor data from SHT41
  * @param  hi2c pointer to I2C handle
  * @param  temp_raw pointer to raw temperature value
  * @param  humid_raw pointer to raw humidity value
  * @retval HAL status
  */
HAL_StatusTypeDef SHT41_ReadRaw(I2C_HandleTypeDef *hi2c, uint16_t *temp_raw, uint16_t *humid_raw)
{
    HAL_StatusTypeDef status;
    uint8_t cmd = SHT41_CMD_MEASURE_HIGH;  /* Measurement command (high precision) */
    uint8_t rx_data[6];  /* 2 bytes temperature + 1 CRC + 2 bytes humidity + 1 CRC */
    
    /* Send measurement command */
    status = HAL_I2C_Master_Transmit(hi2c, SHT41_ADDR_7BIT, &cmd, 1, 100);
    if (status != HAL_OK) {
        return status;
    }
    
    /* Wait for measurement to complete (max 10ms for high precision) */
    HAL_Delay(10);
    
    /* Read measurement data */
    status = HAL_I2C_Master_Receive(hi2c, SHT41_ADDR_7BIT, rx_data, 6, 100);
    if (status != HAL_OK) {
        return status;
    }
    
    /* Verify CRC for temperature */
    uint8_t crc_temp = SHT41_Crc8(&rx_data[0], 2);
    if (crc_temp != rx_data[2]) {
        return HAL_ERROR;
    }
    
    /* Verify CRC for humidity */
    uint8_t crc_humid = SHT41_Crc8(&rx_data[3], 2);
    if (crc_humid != rx_data[5]) {
        return HAL_ERROR;
    }
    
    /* Extract raw values (big-endian) */
    *temp_raw = (rx_data[0] << 8) | rx_data[1];
    *humid_raw = (rx_data[3] << 8) | rx_data[4];
    
    return HAL_OK;
}

/**
  * @brief  Convert raw temperature value to degrees Celsius
  * @param  raw_temp raw temperature value from sensor
  * @retval Temperature in degrees Celsius
  */
float SHT41_ConvertTemperature(uint16_t raw_temp)
{
    return -45.0f + 175.0f * (raw_temp / 65536.0f);
}

/**
  * @brief  Convert raw humidity value to %RH
  * @param  raw_humid raw humidity value from sensor
  * @retval Humidity in %RH
  */
float SHT41_ConvertHumidity(uint16_t raw_humid)
{
    return -6.0f + 125.0f * (raw_humid / 65536.0f);
}

/**
  * @brief  Read temperature and humidity from SHT41
  * @param  hi2c pointer to I2C handle
  * @param  data pointer to SHT41_Data_t structure to store results
  * @retval HAL status
  */
HAL_StatusTypeDef SHT41_Read(I2C_HandleTypeDef *hi2c, SHT41_Data_t *data)
{
    HAL_StatusTypeDef status;
    uint16_t temp_raw, humid_raw;
    
    if (data == NULL) {
        return HAL_ERROR;
    }
    
    /* Read raw data */
    status = SHT41_ReadRaw(hi2c, &temp_raw, &humid_raw);
    if (status != HAL_OK) {
        return status;
    }
    
    /* Convert to physical values */
    data->temperature = SHT41_ConvertTemperature(temp_raw);
    data->humidity = SHT41_ConvertHumidity(humid_raw);
    
    return HAL_OK;
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
