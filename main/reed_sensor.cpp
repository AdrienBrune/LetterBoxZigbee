#include "reed_sensor.hpp"
#include "debug.hpp"
#include "esp_sleep.h"
#include "zigbee.hpp"
#include <map>
#include <algorithm>
#include <cmath>
#include <vector>
#include <numeric>

#define US_TO_SEC (uint64_t)1000000

void setupWakeup()
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_DOOR) | (1ULL << PIN_FLAP),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE, // external pull-up
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    uint64_t mask = (1ULL << PIN_DOOR) | (1ULL << PIN_FLAP);
    esp_sleep_enable_ext1_wakeup_io(mask, ESP_EXT1_WAKEUP_ANY_HIGH);

    esp_sleep_enable_timer_wakeup(REPORTING_MAX_PERIOD_SEC * US_TO_SEC);
    DebugLogger::getInstance().print(DEBUG_SLEEP, DEBUG_INFO, "Automatic wakeup setup to %d seconds", REPORTING_MAX_PERIOD_SEC);
}

void enterDeepSleep()
{
    while(1)
    {
        if (gpio_get_level(PIN_DOOR) == 1 || gpio_get_level(PIN_FLAP) == 1)
        {
            DebugLogger::getInstance().print(DEBUG_SLEEP, DEBUG_WARNING, "gpio not correctly setup, ESP32 can't enter deep sleep");
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
        else
        {
            DebugLogger::getInstance().print(DEBUG_SLEEP, DEBUG_INFO, "ESP32 will enter deep sleep ...");
            esp_deep_sleep_start();
        }
    }
}


void ReedSensor::Update()
{
    if (m_changed)
    {
        DebugLogger::getInstance().print(DEBUG_REED, DEBUG_WARNING, "reed sensor status reading requested but last value has not been used");
        return;
    }

    int status = gpio_get_level(m_pin);
    if (status != m_status)
    {
        m_changed = true;
        m_status = status;
    }
}


void Battery::Setup()
{
    adc_oneshot_unit_init_cfg_t config_unit = {
        .unit_id = ADC_UNIT_1,
        .clk_src = ADC_DIGI_CLK_SRC_DEFAULT,
        .ulp_mode = ADC_ULP_MODE_DISABLE
    };
    if (adc_oneshot_new_unit(&config_unit, &m_handle) != ESP_OK)
    {
        DebugLogger::getInstance().print(DEBUG_ADC, DEBUG_ERROR, "Setup ADC unit failure");
        return;
    }

    adc_oneshot_chan_cfg_t config_ch = {
        .atten = ADC_ATTEN_DB_6,
        .bitwidth = ADC_BITWIDTH_12,
    };
    if (adc_oneshot_config_channel(m_handle, ADC_CHANNEL_0, &config_ch) != ESP_OK)
    {
        DebugLogger::getInstance().print(DEBUG_ADC, DEBUG_ERROR, "Setup ADC channel failure");
        return;
    }
}

bool Battery::GetBatteryPercent(uint16_t& level)
{
    uint16_t adu = 0;
    if (!_GetAverageMeasurement(adu))
    {
        return false;
    }

    // experimental result
    static const std::map<uint16_t, float> batteryLevelMapAdc = {
        {4096, 100.0f},
        {3850, 86.0f},
        {3277, 5.9f},
        {2768, 0.0f}
    };

    // linear interpolation
    float percent = 0.0f;
    if (adu <= batteryLevelMapAdc.begin()->first)
    {
        percent = 0.0f;
    } 
    else if (adu >= batteryLevelMapAdc.rbegin()->first)
    {
        percent = 100.0f;
    } 
    else
    {
        auto itUpper = batteryLevelMapAdc.lower_bound(adu);
        auto itLower = std::prev(itUpper);

        uint16_t adu1 = itLower->first;
        float p1 = itLower->second;
        uint16_t adu2 = itUpper->first;
        float p2 = itUpper->second;

        percent = p1 + ((adu - adu1) * (p2 - p1) / (adu2 - adu1));
    }
    level = static_cast<uint16_t>(std::clamp(std::ceil(percent), 0.0f, 100.0f));

    DebugLogger::getInstance().print(DEBUG_ADC, DEBUG_INFO, "Battery: %2.0f (adu: %d)", level, adu);

    return true;
}

bool Battery::_GetAverageMeasurement(uint16_t& averageAdu)
{
    const int numSamples = 12;
    const int numSamplesRemoved = 6;
    std::vector<int> samples;

    samples.reserve(numSamples);
    for (int i = 0; i < numSamples; i++)
    {
        int adu = 0;
        if (adc_oneshot_read(m_handle, ADC_CHANNEL_0, &adu) != ESP_OK)
        {
            DebugLogger::getInstance().print(DEBUG_ADC, DEBUG_ERROR, "Measurement ADC failure");
            return false;
        }
        samples.push_back(adu);

        vTaskDelay(pdMS_TO_TICKS(500));
    }

    int sum = std::accumulate(samples.begin(), samples.end(), 0.0f);
    int average = sum / samples.size();
    std::sort(samples.begin(), samples.end(), 
        [average](int a, int b)
        {
            return std::abs(a - average) < std::abs(b - average);
        }
    );

    // remove 5 extremum values
    int refinedSum = std::accumulate(samples.begin(), samples.begin() + numSamplesRemoved, 0);
    averageAdu = static_cast<uint16_t>(refinedSum / numSamplesRemoved);

    return true;
}