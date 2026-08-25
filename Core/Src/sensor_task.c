/**
 * @file    sensor_task.c
 * @brief   Периодический опрос SHT3x каждые 2 с + отправка по BLE Notify
 *
 * Архитектура: STM32 Sequencer (stm32_seq) + HW Timer Server (hw_timerserver)
 * FreeRTOS НЕ используется — проект работает на bare-metal sequencer.
 *
 * Схема работы:
 *   SensorTask_Init()
 *       └─ UTIL_SEQ_RegTask(CFG_TASK_SENSOR_ID, ...)
 *       └─ HW_TS_Create(...)  ← создаём таймер
 *       └─ SHT3x_Init(&hi2c1)
 *       └─ HW_TS_Start(...)   ← запускаем таймер на 2 с
 *
 *   Каждые 2 с:
 *       HW timer ISR → SensorTimerCallback()
 *           └─ UTIL_SEQ_SetTask(CFG_TASK_SENSOR_ID)  ← планируем задачу
 *
 *   В главном цикле MX_APPE_Process():
 *       UTIL_SEQ_Run() → SensorTask_Exec()
 *           └─ SHT3x_ReadTempHum(...)
 *           └─ BLE_Env_UpdateTemperature(...)
 *           └─ BLE_Env_UpdateHumidity(...)
 *
 * КАК ДОБАВИТЬ В ПРОЕКТ:
 *  1. Скопировать sht3x.c/h, sensor_task.c/h, ble_env_service.c/h
 *     в папки Core/Src и Core/Inc соответственно.
 *  2. В Makefile добавить в C_SOURCES:
 *       Core/Src/sht3x.c
 *       Core/Src/sensor_task.c
 *       Core/Src/ble_env_service.c
 *  3. В app_conf.h добавить ID задачи (см. комментарий ниже).
 *  4. В app_entry.c вызвать SensorTask_Init() и BLE_Env_Service_Init()
 *     (см. комментарий в конце файла).
 */

#include "sensor_task.h"
#include "sht3x.h"
#include "ble_env_service.h"
#include "i2c.h"                /* hi2c1 объявлен здесь */
#include "app_common.h"
#include "dbg_trace.h"
#include "hw_if.h"              /* HW_TS_Create / HW_TS_Start */
#include "stm32_seq.h"          /* UTIL_SEQ_RegTask / UTIL_SEQ_SetTask */

/* ---------------------------------------------------------------
 * ID задачи в Sequencer.
 *
 * Добавьте в Core/Inc/app_conf.h в enum CFG_IdleTask_Id_With_BLE_t:
 *
 *   CFG_TASK_SENSOR_MEASURE_ID,   // <- добавить перед CFG_LAST_TASK_ID_WITH_BLE
 *
 * Если enum не найден — добавьте где-нибудь в app_conf.h:
 *   #define CFG_TASK_SENSOR_MEASURE_ID   (1UL << 5)
 * и ниже раскомментируйте строку с #define.
 * --------------------------------------------------------------- */
/* #define CFG_TASK_SENSOR_MEASURE_ID   (1UL << 5) */

/* Период опроса датчика в тиках HW Timer Server.
 * Один тик = 625 мкс (RTC wakeup 1600 Гц на LSE 32.768 кГц).
 * 2000 мс / 0.625 мс = 3200 тиков.
 */
#define SENSOR_PERIOD_TICKS   (2000U * 1000U / 625U)   /* 3200 = 2 с */

/* ---------------------------------------------------------------
 * Локальные переменные
 * --------------------------------------------------------------- */
static uint8_t SensorTimerId = 0;   /* ID таймера (выдаётся HW_TS_Create) */

/* ---------------------------------------------------------------
 * Callback таймера — вызывается из RTC IRQ-handler
 * Нельзя делать I2C здесь! Только планируем задачу.
 * --------------------------------------------------------------- */
static void SensorTimerCallback(void)
{
    UTIL_SEQ_SetTask(1U << CFG_TASK_SENSOR_MEASURE_ID, CFG_SCH_PRIO_0);
}

/* ---------------------------------------------------------------
 * Тело задачи — выполняется из UTIL_SEQ_Run() в главном цикле
 * --------------------------------------------------------------- */
static void SensorTask_Exec(void)
{
    SHT3x_Data_t   data;
    SHT3x_Status_t status;

    status = SHT3x_ReadTempHum(&hi2c1, &data);

    if (status == SHT3X_OK)
    {
        /* Формат BLE ESS: int16_t × 100 (единица 0.01 °C / 0.01 %RH) */
        int16_t  temp_ble = (int16_t)(data.temperature * 100.0f);
        uint16_t hum_ble  = (uint16_t)(data.humidity   * 100.0f);

        APP_DBG_MSG("SHT3x: T=%.2f C  H=%.2f %%\n",
                    data.temperature, data.humidity);

        BLE_Env_UpdateTemperature(temp_ble);
        BLE_Env_UpdateHumidity(hum_ble);
    }
    else
    {
        APP_DBG_MSG("SHT3x: read error %d — trying reinit\n", status);

        /* При ошибке пробуем переинициализировать датчик */
        SHT3x_Init(&hi2c1);
    }
}

/* ---------------------------------------------------------------
 * Публичная инициализация — вызывать из APPE_Init()
 * --------------------------------------------------------------- */
void SensorTask_Init(void)
{
    /* 1. Регистрируем задачу в Sequencer */
    UTIL_SEQ_RegTask(1U << CFG_TASK_SENSOR_MEASURE_ID,
                     UTIL_SEQ_RFU,
                     SensorTask_Exec);

    /* 2. Создаём повторяющийся аппаратный таймер */
    HW_TS_Create(CFG_TIM_PROC_ID_ISR,
                 &SensorTimerId,
                 hw_ts_Repeated,
                 SensorTimerCallback);

    /* 3. Инициализируем датчик */
    SHT3x_Status_t ret = SHT3x_Init(&hi2c1);
    if (ret != SHT3X_OK) {
        APP_DBG_MSG("SHT3x: init FAILED (err=%d)\n", ret);
    } else {
        APP_DBG_MSG("SHT3x: init OK\n");
    }

    /* 4. Запускаем таймер — первый запуск через SENSOR_PERIOD_TICKS */
    HW_TS_Start(SensorTimerId, SENSOR_PERIOD_TICKS);

    APP_DBG_MSG("SensorTask: started, period=2s\n");
}

/* ═══════════════════════════════════════════════════════════════
 * ЧТО ДОБАВИТЬ В app_entry.c
 * ═══════════════════════════════════════════════════════════════
 *
 * 1) В начало файла добавить include-ы:
 *
 *    #include "ble_env_service.h"
 *    #include "sensor_task.h"
 *
 * 2) Найти функцию APPE_Init() и в самом конце (после BLE инициализации),
 *    перед return или в конце тела функции добавить:
 *
 *    BLE_Env_Service_Init();   // регистрация BLE ESS сервиса
 *    SensorTask_Init();        // запуск таймера опроса датчика
 *
 *    Важно: эти вызовы должны быть ПОСЛЕ того, как BLE стек поднялся.
 *    Обычно это делается в колбеке APPE_SysUserEvtRx() при событии
 *    SHCI_SUB_EVT_CODE_READY — посмотрите в своём app_entry.c.
 *
 * 3) В Core/Inc/app_conf.h найти перечисление задач секвенсора
 *    (обычно CFG_IdleTask_Id_With_BLE_t) и добавить:
 *
 *    CFG_TASK_SENSOR_MEASURE_ID,   // <- добавить перед CFG_LAST_TASK_ID_WITH_BLE
 *
 * ═══════════════════════════════════════════════════════════════ */
