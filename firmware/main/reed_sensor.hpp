#pragma once

#include "driver/rtc_io.h"
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"

#define PIN_DOOR    GPIO_NUM_4
#define PIN_FLAP    GPIO_NUM_5

#define OPENED 1
#define CLOSED 0

// Deep sleep functions
void setupWakeup();
void enterDeepSleep();


class ReedSensor
{
public:
    ReedSensor(gpio_num_t pin):
        m_pin(pin),
        m_status(CLOSED),
        m_changed(false)
    {}
    ~ReedSensor(){}
public:
    void Update();
    inline bool HasChanged() 
    {
        bool hasChanged = m_changed;
        m_changed = false;
        return hasChanged;
    }
    inline bool IsOpened() const { return m_status == OPENED ? true : false; }
    inline bool IsClosed() const { return !IsOpened(); }
private:
    gpio_num_t m_pin;
    bool m_status;
    bool m_changed;
};


class Battery
{
public:
    Battery():
        m_handle(nullptr)
    {}
    ~Battery()
    {
        if (m_handle != nullptr)
        {
            adc_oneshot_del_unit(m_handle);
            m_handle = nullptr;
        }
    }
    Battery(const Battery&) = delete;
    Battery& operator=(const Battery&) = delete;
public:
    bool Setup();
    bool GetBatteryPercent(uint16_t& level);
private:
    bool _GetAverageMeasurement(uint16_t& adu);
private:
    adc_oneshot_unit_handle_t m_handle;
};