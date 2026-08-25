/*
 * ble_sensor_service.c
 *
 * Регистрирует GATT-сервис Environmental Sensing (0x181A)
 * с двумя характеристиками Temperature (0x2A6E) и Humidity (0x2A6F).
 *
 * Интегрируется с STM32WB BLE стеком (STM32_WPAN / app_ble.c).
 * Этот файл добавляется в проект рядом с app_ble.c.
 *
 * ВАЖНО: функцию BleSensorService_Init() вызвать из SVCCTL_InitCustomSvc()
 *        (файл svc_ctl.c) или из APP_BLE_Init() после GATT инициализации.
 */

#include "ble_sensor_service.h"
#include "ble_common.h"
#include "dbg_trace.h"
#include <string.h>

/* ------------------------------------------------------------------ */
/*  GATT handles                                                         */
/* ------------------------------------------------------------------ */
static uint16_t hEnvSensingService  = 0;
static uint16_t hTempCharacteristic = 0;
static uint16_t hHumCharacteristic  = 0;

/* Флаг: клиент подписался на Notify */
static uint8_t tempNotifyEnabled = 0;
static uint8_t humNotifyEnabled  = 0;

/* Таймер опроса */
static uint32_t lastUpdateTick = 0;

/* ------------------------------------------------------------------ */
/*  BleSensorService_Init                                               */
/* ------------------------------------------------------------------ */
BleSensor_Status_t BleSensorService_Init(void)
{
    tBleStatus ret;
    uint16_t uuid16;

    /* --- Добавить сервис Environmental Sensing 0x181A --- */
    uuid16 = ENVIRONMENTAL_SENSING_SERVICE_UUID; /* 0x181A */
    Service_UUID_t svcUUID;
    svcUUID.Service_UUID_16 = uuid16;

    ret = aci_gatt_add_service(UUID_TYPE_16,
                               &svcUUID,
                               PRIMARY_SERVICE,
                               6,                     /* макс. атрибутов: сервис + 2 char + 2 CCC + 2 value */
                               &hEnvSensingService);
    if (ret != BLE_STATUS_SUCCESS) {
        APP_DBG_MSG("BleSensorService_Init: add_service failed 0x%02X\r\n", ret);
        return BLE_SENSOR_ERROR;
    }

    /* --- Temperature характеристика 0x2A6E --- */
    Char_UUID_t charUUID;
    charUUID.Char_UUID_16 = TEMPERATURE_CHAR_UUID; /* 0x2A6E */

    ret = aci_gatt_add_char(hEnvSensingService,
                            UUID_TYPE_16,
                            &charUUID,
                            2,                                    /* длина значения: int16 */
                            CHAR_PROP_READ | CHAR_PROP_NOTIFY,
                            ATTR_PERMISSION_NONE,
                            GATT_NOTIFY_ATTRIBUTE_WRITE | GATT_NOTIFY_READ_REQ_AND_WAIT_FOR_APPL_RESP,
                            16,
                            CHAR_VALUE_LEN_CONSTANT,
                            &hTempCharacteristic);
    if (ret != BLE_STATUS_SUCCESS) {
        APP_DBG_MSG("BleSensorService_Init: add_char TEMP failed 0x%02X\r\n", ret);
        return BLE_SENSOR_ERROR;
    }

    /* --- Humidity характеристика 0x2A6F --- */
    charUUID.Char_UUID_16 = HUMIDITY_CHAR_UUID; /* 0x2A6F */

    ret = aci_gatt_add_char(hEnvSensingService,
                            UUID_TYPE_16,
                            &charUUID,
                            2,                                    /* длина значения: uint16 */
                            CHAR_PROP_READ | CHAR_PROP_NOTIFY,
                            ATTR_PERMISSION_NONE,
                            GATT_NOTIFY_ATTRIBUTE_WRITE | GATT_NOTIFY_READ_REQ_AND_WAIT_FOR_APPL_RESP,
                            16,
                            CHAR_VALUE_LEN_CONSTANT,
                            &hHumCharacteristic);
    if (ret != BLE_STATUS_SUCCESS) {
        APP_DBG_MSG("BleSensorService_Init: add_char HUM failed 0x%02X\r\n", ret);
        return BLE_SENSOR_ERROR;
    }

    APP_DBG_MSG("BleSensorService_Init: OK. SvcHandle=0x%04X\r\n", hEnvSensingService);
    return BLE_SENSOR_OK;
}

/* ------------------------------------------------------------------ */
/*  BleSensorService_Update — обновить значения и отправить Notify      */
/* ------------------------------------------------------------------ */
BleSensor_Status_t BleSensorService_Update(const SHT3x_Data_t *data)
{
    if (data == NULL) return BLE_SENSOR_ERROR;

    tBleStatus ret;

    /* Temperature: int16, единицы 0.01 °C  */
    int16_t tempRaw = (int16_t)(data->temperature * 100.0f);
    uint8_t tempBuf[2] = {(uint8_t)(tempRaw & 0xFF), (uint8_t)((tempRaw >> 8) & 0xFF)};

    ret = aci_gatt_update_char_value(hEnvSensingService,
                                     hTempCharacteristic,
                                     0, 2, tempBuf);
    if (ret != BLE_STATUS_SUCCESS) {
        APP_DBG_MSG("BLE Update TEMP failed 0x%02X\r\n", ret);
    }

    /* Humidity: uint16, единицы 0.01 %RH */
    uint16_t humRaw = (uint16_t)(data->humidity * 100.0f);
    uint8_t humBuf[2] = {(uint8_t)(humRaw & 0xFF), (uint8_t)((humRaw >> 8) & 0xFF)};

    ret = aci_gatt_update_char_value(hEnvSensingService,
                                     hHumCharacteristic,
                                     0, 2, humBuf);
    if (ret != BLE_STATUS_SUCCESS) {
        APP_DBG_MSG("BLE Update HUM failed 0x%02X\r\n", ret);
    }

    APP_DBG_MSG("SHT3x -> T=%.2f C  RH=%.2f %%\r\n",
                data->temperature, data->humidity);

    return BLE_SENSOR_OK;
}

/* ------------------------------------------------------------------ */
/*  BleSensorService_Process — вызывать из главного цикла               */
/* ------------------------------------------------------------------ */
void BleSensorService_Process(void)
{
    uint32_t now = HAL_GetTick();
    if ((now - lastUpdateTick) < SENSOR_UPDATE_PERIOD_MS) return;
    lastUpdateTick = now;

    SHT3x_Data_t sensorData;
    SHT3x_Status_t st = SHT3x_ReadData(&sensorData);

    if (st == SHT3X_OK) {
        BleSensorService_Update(&sensorData);
    } else {
        APP_DBG_MSG("SHT3x read error: %d\r\n", st);
    }
}
