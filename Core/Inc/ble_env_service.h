/**
 * @file    ble_env_service.h
 * @brief   BLE Environmental Sensing Service (ESS)
 *          Характеристики: Temperature (0x2A6E) + Humidity (0x2A6F)
 *
 *  На телефоне открываешь любое BLE приложение (nRF Connect, LightBlue)
 *  и видишь Temperature и Humidity с Notify.
 */

#ifndef CORE_INC_BLE_ENV_SERVICE_H_
#define CORE_INC_BLE_ENV_SERVICE_H_

#include "stm32wbxx_hal.h"
#include "ble_types.h"
#include "ble_defs.h"
#include <stdint.h>

/* ---------------------------------------------------------------
 * UUID сервиса и характеристик (стандартные BT SIG)
 * --------------------------------------------------------------- */
#define ENV_SENSING_SERVICE_UUID        0x181A  /* Environmental Sensing */
#define TEMPERATURE_CHAR_UUID           0x2A6E  /* Temperature           */
#define HUMIDITY_CHAR_UUID              0x2A6F  /* Humidity              */

/* ---------------------------------------------------------------
 * Хэндлы (заполняются при регистрации)
 * --------------------------------------------------------------- */
extern uint16_t EnvService_Handle;
extern uint16_t TempChar_Handle;
extern uint16_t HumChar_Handle;

/* ---------------------------------------------------------------
 * API
 * --------------------------------------------------------------- */

/**
 * @brief  Зарегистрировать BLE ESS сервис
 *         Вызывать из APPE_Init() или после BLE стека
 */
tBleStatus BLE_Env_Service_Init(void);

/**
 * @brief  Обновить значение температуры и отправить Notify
 * @param  temp_x100  температура * 100 (например, 2350 = 23.50 °C)
 */
tBleStatus BLE_Env_UpdateTemperature(int16_t temp_x100);

/**
 * @brief  Обновить значение влажности и отправить Notify
 * @param  hum_x100  влажность * 100 (например, 5512 = 55.12 %RH)
 */
tBleStatus BLE_Env_UpdateHumidity(uint16_t hum_x100);

#endif /* CORE_INC_BLE_ENV_SERVICE_H_ */
