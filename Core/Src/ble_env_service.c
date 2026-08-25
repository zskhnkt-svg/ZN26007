/**
 * @file    ble_env_service.c
 * @brief   BLE Environmental Sensing Service — Temperature + Humidity Notify
 *
 * Как работает:
 *  1. BLE_Env_Service_Init()  — вызвать 1 раз при старте (в APP_BLE_Init или APPE_Init)
 *  2. Каждые N секунд из SensorTask:
 *       BLE_Env_UpdateTemperature(...)
 *       BLE_Env_UpdateHumidity(...)
 *  3. Телефон с Notify подписан — данные придут автоматически
 */

#include "ble_env_service.h"
#include "ble_common.h"
#include "dbg_trace.h"
#include <string.h>

uint16_t EnvService_Handle = 0;
uint16_t TempChar_Handle   = 0;
uint16_t HumChar_Handle    = 0;

/* ---------------------------------------------------------------
 * Инициализация сервиса
 * --------------------------------------------------------------- */
tBleStatus BLE_Env_Service_Init(void)
{
    tBleStatus ret;

    /* --- 1. Добавить сервис --- */
    Service_UUID_t svc_uuid;
    svc_uuid.Service_UUID_16 = ENV_SENSING_SERVICE_UUID;

    ret = aci_gatt_add_service(UUID_TYPE_16,
                               &svc_uuid,
                               PRIMARY_SERVICE,
                               6,                /* макс атрибутов: сервис + 2 хар-ки * 3 */
                               &EnvService_Handle);
    if (ret != BLE_STATUS_SUCCESS) {
        APP_DBG_MSG("ENV SVC: add service failed 0x%02X\n", ret);
        return ret;
    }

    /* --- 2. Характеристика Temperature (int16, единица 0.01 °C) --- */
    Char_UUID_t char_uuid;
    char_uuid.Char_UUID_16 = TEMPERATURE_CHAR_UUID;

    ret = aci_gatt_add_char(EnvService_Handle,
                            UUID_TYPE_16,
                            &char_uuid,
                            2,                          /* 2 байта int16 */
                            CHAR_PROP_READ | CHAR_PROP_NOTIFY,
                            ATTR_PERMISSION_NONE,
                            GATT_NOTIFY_READ_REQ_AND_WAIT_FOR_APPL_RESP,
                            10,
                            CHAR_VALUE_LEN_CONSTANT,
                            &TempChar_Handle);
    if (ret != BLE_STATUS_SUCCESS) {
        APP_DBG_MSG("ENV SVC: add temp char failed 0x%02X\n", ret);
        return ret;
    }

    /* --- 3. Характеристика Humidity (uint16, единица 0.01 %RH) --- */
    char_uuid.Char_UUID_16 = HUMIDITY_CHAR_UUID;

    ret = aci_gatt_add_char(EnvService_Handle,
                            UUID_TYPE_16,
                            &char_uuid,
                            2,                          /* 2 байта uint16 */
                            CHAR_PROP_READ | CHAR_PROP_NOTIFY,
                            ATTR_PERMISSION_NONE,
                            GATT_NOTIFY_READ_REQ_AND_WAIT_FOR_APPL_RESP,
                            10,
                            CHAR_VALUE_LEN_CONSTANT,
                            &HumChar_Handle);
    if (ret != BLE_STATUS_SUCCESS) {
        APP_DBG_MSG("ENV SVC: add hum char failed 0x%02X\n", ret);
        return ret;
    }

    APP_DBG_MSG("ENV SVC: initialized OK (svc=0x%04X temp=0x%04X hum=0x%04X)\n",
                EnvService_Handle, TempChar_Handle, HumChar_Handle);
    return BLE_STATUS_SUCCESS;
}

/* ---------------------------------------------------------------
 * Обновить температуру и уведомить подписчиков
 * --------------------------------------------------------------- */
tBleStatus BLE_Env_UpdateTemperature(int16_t temp_x100)
{
    uint8_t buf[2];
    buf[0] = (uint8_t)(temp_x100 & 0xFF);
    buf[1] = (uint8_t)((temp_x100 >> 8) & 0xFF);

    return aci_gatt_update_char_value(EnvService_Handle,
                                      TempChar_Handle,
                                      0, 2, buf);
}

/* ---------------------------------------------------------------
 * Обновить влажность и уведомить подписчиков
 * --------------------------------------------------------------- */
tBleStatus BLE_Env_UpdateHumidity(uint16_t hum_x100)
{
    uint8_t buf[2];
    buf[0] = (uint8_t)(hum_x100 & 0xFF);
    buf[1] = (uint8_t)((hum_x100 >> 8) & 0xFF);

    return aci_gatt_update_char_value(EnvService_Handle,
                                      HumChar_Handle,
                                      0, 2, buf);
}
