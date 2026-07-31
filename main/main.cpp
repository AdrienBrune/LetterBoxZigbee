
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_zigbee_core.h"
#include "string.h"
#include "nvs_flash.h"
#include "esp_systick_etm.h"
#include "debug.hpp"
#include "zigbee.hpp"
#include "reed_sensor.hpp"
#include "memory/pers_mem.hpp"

#define LED_PIN 15

#define TIMEOUT_INACTIVITY_MS (uint64_t)20000

void ledPulse()
{
    gpio_set_level((gpio_num_t)LED_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_level((gpio_num_t)LED_PIN, 0);
}

void led5Pulse()
{
    for (int i = 0; i < 5; i++)
    {
        ledPulse();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void led20Pulse()
{
    for (int i = 0; i < 4; i++)
    {
        led5Pulse();
    }
}

void _task_zigbee(void *pvParameters)
{
    if(initZigbee() != ESP_OK)
    {
        DebugLogger::getInstance().print(DEBUG_TASK, DEBUG_ERROR, "ZIGBEE init failure");
        return;
    }

    if(initDevice() != ESP_OK)
    {
        DebugLogger::getInstance().print(DEBUG_TASK, DEBUG_ERROR, "ZIGBEE device registration failure");
        return;
    }

    esp_zb_set_rx_on_when_idle(false);

    if(esp_zb_start(false) != ESP_OK)
    {
        DebugLogger::getInstance().print(DEBUG_TASK, DEBUG_ERROR, "ZIGBEE hasn't started");
        return;
    }

    DebugLogger::getInstance().print(DEBUG_TASK, DEBUG_INFO, "ZIGBEE main loop started");
    esp_zb_stack_main_loop();
}


void _task_report(void *pvParameters)
{
    TickType_t startTick = xTaskGetTickCount();

    gpio_set_level((gpio_num_t)LED_PIN, 1);

    esp_sleep_wakeup_cause_t wakeup = esp_sleep_get_wakeup_cause();
    switch (wakeup)
    {
        case ESP_SLEEP_WAKEUP_EXT1:
            DebugLogger::getInstance().print(DEBUG_TASK, DEBUG_INFO, "ESP32 woke up by gpio");
            break;
        case ESP_SLEEP_WAKEUP_TIMER:
            DebugLogger::getInstance().print(DEBUG_TASK, DEBUG_INFO, "ESP32 woke up by timer");
            break;
        default:
            break;
    }

    setupWakeup();

    ReedSensor door(PIN_DOOR), flap(PIN_FLAP);

    vTaskDelay(pdMS_TO_TICKS(500)); // delay before reading sensors
    door.Update();
    flap.Update();

    while (connected.load() != true)
    {
        if ((xTaskGetTickCount() - startTick) >= pdMS_TO_TICKS(TIMEOUT_INACTIVITY_MS))
        {
            led20Pulse();
            enterDeepSleep(); // zigbee connection failure ...
        }

        vTaskDelay(pdMS_TO_TICKS(100)); // wait for zigbee connexion
    }

    gpio_set_level((gpio_num_t)LED_PIN, 0);

    if (wakeup == ESP_SLEEP_WAKEUP_TIMER)
    {
        Battery battery;
        uint16_t level = 0;

        battery.Setup();
        if (battery.GetLevelPercent(level))
        {
            Memory::GetMemory().Set<uint32_t>(DATA_BATTERY, (uint32_t)level);
            updateBatteryStatus(ZB_EP_BATTERY, level);
            
            ledPulse();
        }

        vTaskDelay(pdMS_TO_TICKS(2000)); // let time for zigbee to send attribute
    }
    else // ESP_SLEEP_WAKEUP_EXT1
    {
        startTick = xTaskGetTickCount();

        while (1)
        {
            door.Update(); // first time skipped because HasChanged() not called yet
            flap.Update(); // first time skipped because HasChanged() not called yet
            if (door.HasChanged() || flap.HasChanged())
            {
                startTick = xTaskGetTickCount(); // reset timer
                
                updateReedStatus(ZB_EP_DOOR, door.IsOpened());
                updateReedStatus(ZB_EP_FLAP, flap.IsOpened());

                uint16_t batteryLevel = (uint16_t)Memory::GetMemory().Get<uint32_t>(DATA_BATTERY);
                updateBatteryStatus(ZB_EP_BATTERY, batteryLevel);

                ledPulse();
            }

            if (door.IsClosed() && flap.IsClosed())
            {
                if ((xTaskGetTickCount() - startTick) >= pdMS_TO_TICKS(TIMEOUT_INACTIVITY_MS))
                {
                    break; // ready to enter deep sleep
                }
            }

            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    led5Pulse();
    enterDeepSleep();
}


extern "C" void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        DebugLogger::getInstance().print(DEBUG_GENERIC, DEBUG_ERROR, "NVS flash init");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if (err != ESP_OK)
    {
        DebugLogger::getInstance().print(DEBUG_GENERIC, DEBUG_ERROR, "NVS init failure, program stopped");
        return;
    }

    esp_zb_platform_config_t config = {
        .radio_config = { .radio_mode = ZB_RADIO_MODE_NATIVE },
        .host_config = { .host_connection_mode = ZB_HOST_CONNECTION_MODE_NONE },
    };
    if(esp_zb_platform_config(&config) != ESP_OK)
    {
        DebugLogger::getInstance().print(DEBUG_GENERIC, DEBUG_ERROR, "ZIGBEE init failure, program stopped");
        return;
    }

    Memory::GetMemory().Load();

    gpio_set_direction((gpio_num_t)LED_PIN, GPIO_MODE_OUTPUT);

    DebugLogger::getInstance().print(DEBUG_GENERIC, DEBUG_INFO, "init completed");

    xTaskCreate(_task_zigbee, "zigbee", 8124, NULL, 3, NULL);
    xTaskCreate(_task_report, "report", 4096, NULL, 2, NULL);
}
