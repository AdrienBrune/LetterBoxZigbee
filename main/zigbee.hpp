#pragma once

#include "ha/esp_zigbee_ha_standard.h"
#include "zcl_utility.h"
#include "zcl/esp_zigbee_zcl_command.h"
#include "zcl/esp_zigbee_zcl_ias_zone.h"
#include "zcl/esp_zigbee_zcl_power_config.h"
#include "esp_zigbee_core.h"
#include "atomic"

#define ZB_EP_DOOR      1
#define ZB_EP_FLAP      2
#define ZB_EP_BATTERY   3

#define REPORTING_MAX_PERIOD_SEC (uint64_t)(3600 * 12)

extern std::atomic<bool> connected;

esp_err_t initZigbee();
esp_err_t initDevice();
void updateReedStatus(uint8_t endpoint, bool opened);
void updateBatteryStatus(uint8_t endpoint, uint16_t levelPercent);
