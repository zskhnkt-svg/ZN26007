/**
 * @file    sht3x.h
 * @brief   Драйвер датчика SHT30/SHT31/SHT35 (I2C, HAL)
 *
 * Подключение:
 *   SDA -> PB9 (I2C1)
 *   SCL -> PB8 (I2C1)
 *   ADDR -> GND => адрес 0x44
 *   ADDR -> VCC => адрес 0x45
 */

#ifndef BSP_SHT3X_H
#define BSP_SHT3X_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32wbxx_hal.h"
#include <stdint.h>

/* ---------------------------------------------------------------
 * I2C адрес (7-бит)
 *   ADDR pin = GND -> 0x44
 *   ADDR pin = VCC -> 0x45
 * --------------------------------------------------------------- */
#define SHT3X_I2C_ADDR_LOW    0x44U   /* ADDR = GND */
#define SHT3X_I2C_ADDR_HIGH   0x45U   /* ADDR = VCC */

#ifndef SHT3X_I2C_ADDR
#define SHT3X_I2C_ADDR        SHT3X_I2C_ADDR_LOW
#endif

/* HAL I2C таймаут (мс) */
#define SHT3X_I2C_TIMEOUT_MS  50U

/* ---------------------------------------------------------------
 * Коды возврата
 * --------------------------------------------------------------- */
typedef enum {
    SHT3X_OK            =  0,
    SHT3X_ERR_I2C       = -1,   /* Ошибка шины I2C             */
    SHT3X_ERR_CRC       = -2,   /* Ошибка контрольной суммы    */
    SHT3X_ERR_TIMEOUT   = -3,   /* Таймаут ожидания данных     */
} SHT3x_Status_t;

/* ---------------------------------------------------------------
 * Структура данных
 * --------------------------------------------------------------- */
typedef struct {
    float temperature;   /* °C */
    float humidity;      /* %RH */
} SHT3x_Data_t;

/* ---------------------------------------------------------------
 * API
 * --------------------------------------------------------------- */

/**
 * @brief  Инициализация датчика (soft-reset + проверка связи)
 * @param  hi2c  Указатель на хэндл I2C (напр. &hi2c1)
 * @retval SHT3X_OK или код ошибки
 */
SHT3x_Status_t SHT3x_Init(I2C_HandleTypeDef *hi2c);

/**
 * @brief  Однократное измерение температуры и влажности
 *         (High Repeatability, без Clock Stretching)
 * @param  hi2c  Указатель на хэндл I2C
 * @param  data  Указатель на структуру для результатов
 * @retval SHT3X_OK или код ошибки
 */
SHT3x_Status_t SHT3x_ReadTempHum(I2C_HandleTypeDef *hi2c, SHT3x_Data_t *data);

/**
 * @brief  Программный сброс датчика
 * @param  hi2c  Указатель на хэндл I2C
 * @retval SHT3X_OK или код ошибки
 */
SHT3x_Status_t SHT3x_SoftReset(I2C_HandleTypeDef *hi2c);

#ifdef __cplusplus
}
#endif

#endif /* BSP_SHT3X_H */
