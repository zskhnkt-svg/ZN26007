/**
 * @file    sensor_task.h
 * @brief   Задача опроса SHT3x — на базе STM32 Sequencer + HW Timer Server
 *          (БЕЗ FreeRTOS)
 */

#ifndef CORE_INC_SENSOR_TASK_H_
#define CORE_INC_SENSOR_TASK_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Инициализация: регистрация задачи в Sequencer и запуск таймера.
 *         Вызывать ОДИН РАЗ из APPE_Init() после BLE стека.
 */
void SensorTask_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* CORE_INC_SENSOR_TASK_H_ */
