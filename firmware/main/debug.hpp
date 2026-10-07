#pragma once

#include <string>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "driver/uart.h"

/* Debug enable per component (empty string = disabled) */
#define DEBUG_GENERIC   "generic"
#define DEBUG_TASK      "task"
#define DEBUG_ZIGBEE    "zigbee"
#define DEBUG_REED      "reed"
#define DEBUG_ADC       "adc"
#define DEBUG_SLEEP     "sleep"
#define DEBUG_EEPROM    "flash"

/* Debug levels */
#define DEBUG_INFO      1
#define DEBUG_WARNING   2
#define DEBUG_ERROR     4
#define DEBUG_FILTER    (DEBUG_INFO | DEBUG_WARNING | DEBUG_ERROR)

class DebugLogger
{
public:
    inline static DebugLogger& getInstance()
    {
        static DebugLogger instance;
        return instance;
    }

    inline void print(std::string componant, uint8_t filter, const char* format, ...)
    {
        if(componant == "") // Debug disabled
            return;
        
        if(!(DEBUG_FILTER & filter)) // Filter messages
            return;

        if(xSemaphoreTake(mSemaphore, portMAX_DELAY))
        {
            std::string filter_str;
            switch(filter)
            {
                case DEBUG_INFO: filter_str = "INFO"; break;
                case DEBUG_WARNING: filter_str = "WARN"; break;
                case DEBUG_ERROR: filter_str = "ERR!"; break;
                default: filter_str = "????"; break;
            }

            printf("[%s][%s] : ", filter_str.c_str(), componant.c_str());

            va_list args;
            va_start(args, format);
            static char buffer[256];
            vsnprintf(buffer, sizeof(buffer), format, args);
            va_end(args);

            printf("%s\n", buffer);

            xSemaphoreGive(mSemaphore);
        }
    }

private:
    DebugLogger()
    {
        mSemaphore = xSemaphoreCreateMutex();
    }

    ~DebugLogger()
    {
        if (mSemaphore)
        {
            vSemaphoreDelete(mSemaphore);
            mSemaphore = nullptr;
        }
    }

    DebugLogger(const DebugLogger&) = delete;
    DebugLogger& operator=(const DebugLogger&) = delete;

    SemaphoreHandle_t mSemaphore;
};
