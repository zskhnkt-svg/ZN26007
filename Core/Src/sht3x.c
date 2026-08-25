/**
 * @file    sht3x.c
 * @brief   Драйвер датчика SHT30/SHT31/SHT35
 *
 * Протокол:
 *  1. Отправить 2-байтную команду измерения (0x24, 0x00)
 *  2. Подождать ≥15 мс (High Repeatability)
 *  3. Принять 6 байт: [T_MSB][T_LSB][CRC_T][H_MSB][H_LSB][CRC_H]
 *  4. Проверить CRC (полином x^8 + x^5 + x^4 + 1, init=0xFF)
 *  5. Пересчитать в физические величины
 */

#include "sht3x.h"

/* ---------------------------------------------------------------
 * Команды датчика (2 байта каждая)
 * --------------------------------------------------------------- */
#define SHT3X_CMD_MEAS_HIGHREP_STRETCH   0x2C06U  /* High rep, clock stretching ON  */
#define SHT3X_CMD_MEAS_HIGHREP           0x2400U  /* High rep, clock stretching OFF */
#define SHT3X_CMD_SOFT_RESET             0x30A2U
#define SHT3X_CMD_READ_STATUS            0xF32DU

/* Время ожидания измерения (мс), High Repeatability */
#define SHT3X_MEAS_DELAY_MS              20U

/* ---------------------------------------------------------------
 * CRC-8: полином 0x31 (x^8 + x^5 + x^4 + 1), init = 0xFF
 * --------------------------------------------------------------- */
static uint8_t sht3x_crc8(const uint8_t *data, uint8_t len)
{
    uint8_t crc = 0xFF;
    for (uint8_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 0x80U) {
                crc = (uint8_t)((crc << 1U) ^ 0x31U);
            } else {
                crc <<= 1U;
            }
        }
    }
    return crc;
}

/* ---------------------------------------------------------------
 * Отправить 2-байтную команду
 * --------------------------------------------------------------- */
static SHT3x_Status_t sht3x_send_cmd(I2C_HandleTypeDef *hi2c, uint16_t cmd)
{
    uint8_t buf[2];
    buf[0] = (uint8_t)(cmd >> 8U);
    buf[1] = (uint8_t)(cmd & 0xFFU);

    HAL_StatusTypeDef ret = HAL_I2C_Master_Transmit(hi2c,
                                                     (uint16_t)(SHT3X_I2C_ADDR << 1U),
                                                     buf, 2,
                                                     SHT3X_I2C_TIMEOUT_MS);
    return (ret == HAL_OK) ? SHT3X_OK : SHT3X_ERR_I2C;
}

/* ---------------------------------------------------------------
 * Публичные функции
 * --------------------------------------------------------------- */

SHT3x_Status_t SHT3x_SoftReset(I2C_HandleTypeDef *hi2c)
{
    SHT3x_Status_t ret = sht3x_send_cmd(hi2c, SHT3X_CMD_SOFT_RESET);
    if (ret != SHT3X_OK) {
        return ret;
    }
    HAL_Delay(2U);   /* Датчик стартует за ≤1.5 мс */
    return SHT3X_OK;
}

SHT3x_Status_t SHT3x_Init(I2C_HandleTypeDef *hi2c)
{
    /* Проверяем связь: пытаемся начать I2C транзакцию */
    HAL_StatusTypeDef hal_ret = HAL_I2C_IsDeviceReady(hi2c,
                                                       (uint16_t)(SHT3X_I2C_ADDR << 1U),
                                                       3,
                                                       SHT3X_I2C_TIMEOUT_MS);
    if (hal_ret != HAL_OK) {
        return SHT3X_ERR_I2C;
    }

    /* Программный сброс */
    return SHT3x_SoftReset(hi2c);
}

SHT3x_Status_t SHT3x_ReadTempHum(I2C_HandleTypeDef *hi2c, SHT3x_Data_t *data)
{
    /* 1. Отправить команду однократного измерения */
    SHT3x_Status_t ret = sht3x_send_cmd(hi2c, SHT3X_CMD_MEAS_HIGHREP);
    if (ret != SHT3X_OK) {
        return ret;
    }

    /* 2. Ожидание завершения измерения */
    HAL_Delay(SHT3X_MEAS_DELAY_MS);

    /* 3. Принять 6 байт */
    uint8_t buf[6] = {0};
    HAL_StatusTypeDef hal_ret = HAL_I2C_Master_Receive(hi2c,
                                                        (uint16_t)(SHT3X_I2C_ADDR << 1U),
                                                        buf, 6,
                                                        SHT3X_I2C_TIMEOUT_MS);
    if (hal_ret != HAL_OK) {
        return SHT3X_ERR_I2C;
    }

    /* 4. Проверка CRC температуры */
    if (sht3x_crc8(&buf[0], 2) != buf[2]) {
        return SHT3X_ERR_CRC;
    }

    /* 5. Проверка CRC влажности */
    if (sht3x_crc8(&buf[3], 2) != buf[5]) {
        return SHT3X_ERR_CRC;
    }

    /* 6. Конвертация */
    uint16_t raw_temp = (uint16_t)((buf[0] << 8U) | buf[1]);
    uint16_t raw_hum  = (uint16_t)((buf[3] << 8U) | buf[4]);

    data->temperature = -45.0f + 175.0f * ((float)raw_temp / 65535.0f);
    data->humidity    = 100.0f * ((float)raw_hum  / 65535.0f);

    /* Ограничение диапазона влажности */
    if (data->humidity > 100.0f) { data->humidity = 100.0f; }
    if (data->humidity <   0.0f) { data->humidity =   0.0f; }

    return SHT3X_OK;
}
