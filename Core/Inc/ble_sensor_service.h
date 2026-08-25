#ifndef CORE_INC_BLE_SENSOR_SERVICE_H_
#define CORE_INC_BLE_SENSOR_SERVICE_H_

#include "stm32wbxx_hal.h"
#include "sht3x.h"
#include <stdint.h>

/*
 * Кастомный BLE-сервис: Environmental Sensing
 *
 * Service UUID:    0x181A  (Environmental Sensing — стандартный GATT)
 * Characteristic:
 *   - Temperature  UUID 0x2A6E  (int16, °C * 100)
 *   - Humidity     UUID 0x2A6F  (uint16, % * 100)
 *
 * Данные отправляются через NOTIFY каждые SENSOR_UPDATE_PERIOD_MS мс.
 */

#define SENSOR_UPDATE_PERIOD_MS   2000U   /* период опроса датчика */

/* Статусы */
typedef enum {
    BLE_SENSOR_OK    = 0,
    BLE_SENSOR_ERROR = 1
} BleSensor_Status_t;

/* Инициализировать сервис (вызвать после GATT init) */
BleSensor_Status_t BleSensorService_Init(void);

/* Обновить характеристики из данных датчика и отправить Notify */
BleSensor_Status_t BleSensorService_Update(const SHT3x_Data_t *data);

/* Вызывать из основного цикла / таймера */
void BleSensorService_Process(void);

#endif /* CORE_INC_BLE_SENSOR_SERVICE_H_ */
