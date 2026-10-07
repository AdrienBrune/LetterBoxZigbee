#include "zigbee.hpp"
#include <cstdint>
#include "debug.hpp"
#include <map>
#include <atomic>

std::atomic<bool> connected = false;

static char modelName[] = { 0x0A, 'L', 'E', 'T', 'T', 'E', 'R', '-', 'B', 'O', 'X' };
static char manufacturerName[] = { 0x06, 'C', 'U', 'S', 'T', 'O', 'M'};


esp_zb_cluster_list_t *createReedClusterList()
{
    uint8_t appVersion = 1;
    uint8_t stackVersion = 1;
    uint8_t hwVersion = 1;

    esp_zb_cluster_list_t *list = esp_zb_zcl_cluster_list_create();
    
    // BASIC
    esp_zb_basic_cluster_cfg_t basic_cfg = { .zcl_version = 3, .power_source = 0x03 };
    esp_zb_attribute_list_t *basic_attr = esp_zb_basic_cluster_create(&basic_cfg);
    esp_zb_basic_cluster_add_attr(basic_attr, ESP_ZB_ZCL_ATTR_BASIC_APPLICATION_VERSION_ID, &appVersion);
    esp_zb_basic_cluster_add_attr(basic_attr, ESP_ZB_ZCL_ATTR_BASIC_STACK_VERSION_ID, &stackVersion);
    esp_zb_basic_cluster_add_attr(basic_attr, ESP_ZB_ZCL_ATTR_BASIC_HW_VERSION_ID, &hwVersion);
    esp_zb_basic_cluster_add_attr(basic_attr, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, modelName);
    esp_zb_basic_cluster_add_attr(basic_attr, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, manufacturerName);
    esp_zb_cluster_list_add_basic_cluster(list, basic_attr, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

    // BINARY INPUT
    esp_zb_binary_input_cluster_cfg_t binary_cfg = { .out_of_service = false, .status_flags = 0, .present_value = false };
    esp_zb_attribute_list_t *binary_attr = esp_zb_binary_input_cluster_create(&binary_cfg);
    esp_zb_cluster_list_add_binary_input_cluster(list, binary_attr, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

    return list;
}

esp_zb_cluster_list_t *createBatteryClusterList()
{
    esp_zb_cluster_list_t *list = esp_zb_zcl_cluster_list_create();
    
    // BASIC
    esp_zb_basic_cluster_cfg_t basic_cfg = { .zcl_version = 3, .power_source = 0x03 }; // 0x03 = Battery
    esp_zb_attribute_list_t *basic_attr = esp_zb_basic_cluster_create(&basic_cfg);
    esp_zb_cluster_list_add_basic_cluster(list, basic_attr, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

    // POWER CONFIGURATION
    uint8_t batteryPercentage = 200;
    uint8_t batteryVoltage = 30; 
    uint8_t batterySize = 0x0b;
    uint8_t batteryQty = 1;
    esp_zb_power_config_cluster_cfg_t power_cfg = { 0 }; 
    esp_zb_attribute_list_t *power_attr = esp_zb_power_config_cluster_create(&power_cfg);
    esp_zb_power_config_cluster_add_attr(power_attr, ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_VOLTAGE_ID, &batteryVoltage);
    esp_zb_power_config_cluster_add_attr(power_attr, ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_REMAINING_ID, &batteryPercentage);
    esp_zb_power_config_cluster_add_attr(power_attr, ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_SIZE_ID, &batterySize);
    esp_zb_power_config_cluster_add_attr(power_attr, ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_QUANTITY_ID, &batteryQty);
    esp_zb_cluster_list_add_power_config_cluster(list, power_attr, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

    return list;
}
   
void updateReedStatus(uint8_t endpoint, bool is_open)
{
    if (connected.load() == false)
    {
        DebugLogger::getInstance().print(DEBUG_ZIGBEE, DEBUG_ERROR, "not connected to zigbee, can't report attribute");
        return;
    }

    DebugLogger::getInstance().print(DEBUG_ZIGBEE, DEBUG_INFO, "Try to update ep%d attribute to %s", endpoint, is_open ? "open" : "close");

    if (esp_zb_lock_acquire(portMAX_DELAY))
    {
        DebugLogger::getInstance().print(DEBUG_ZIGBEE, DEBUG_INFO, "Try to change attribute localy");

        esp_zb_zcl_status_t status = esp_zb_zcl_set_attribute_val(
            endpoint,
            ESP_ZB_ZCL_CLUSTER_ID_BINARY_INPUT,
            ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
            ESP_ZB_ZCL_ATTR_BINARY_INPUT_PRESENT_VALUE_ID,
            &is_open,
            false // localy changed
        );
        if (status != ESP_ZB_ZCL_STATUS_SUCCESS)
        {
            DebugLogger::getInstance().print(DEBUG_ZIGBEE, DEBUG_ERROR, "set attribut localy failed");
            esp_zb_lock_release();
            return;
        }

        DebugLogger::getInstance().print(DEBUG_ZIGBEE, DEBUG_INFO, "Try to send attribute to coordinator");

        esp_zb_zcl_report_attr_cmd_t report_cmd;
        memset(&report_cmd, 0, sizeof(esp_zb_zcl_report_attr_cmd_t));
        report_cmd.zcl_basic_cmd.dst_addr_u.addr_short = 0x0000;
        report_cmd.zcl_basic_cmd.dst_endpoint = 1;
        report_cmd.zcl_basic_cmd.src_endpoint = endpoint;
        report_cmd.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
        report_cmd.clusterID = ESP_ZB_ZCL_CLUSTER_ID_BINARY_INPUT;
        report_cmd.attributeID = ESP_ZB_ZCL_ATTR_BINARY_INPUT_PRESENT_VALUE_ID;
        report_cmd.direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_CLI;
        report_cmd.dis_default_resp = 1;
        report_cmd.manuf_specific = 0;
        esp_zb_zcl_report_attr_cmd_req(&report_cmd);

        esp_zb_lock_release();
    } 
}

void updateBatteryStatus(uint8_t endpoint, uint16_t levelPercent)
{
    uint8_t levelPercent200 = levelPercent * 2;

    DebugLogger::getInstance().print(DEBUG_ZIGBEE, DEBUG_INFO, "Try to update ep%d attribute to %d%", endpoint, levelPercent);

    if (esp_zb_lock_acquire(portMAX_DELAY))
    {
        DebugLogger::getInstance().print(DEBUG_ZIGBEE, DEBUG_INFO, "Try to change attribute localy");

        esp_zb_zcl_set_attribute_val(
            endpoint,
            ESP_ZB_ZCL_CLUSTER_ID_POWER_CONFIG,
            ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
            ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_REMAINING_ID,
            &levelPercent200,
            false
        );

        DebugLogger::getInstance().print(DEBUG_ZIGBEE, DEBUG_INFO, "Try to send attribute to coordinator");

        esp_zb_zcl_report_attr_cmd_t report_cmd;
        memset(&report_cmd, 0, sizeof(esp_zb_zcl_report_attr_cmd_t));
        report_cmd.zcl_basic_cmd.dst_addr_u.addr_short = 0x0000;
        report_cmd.zcl_basic_cmd.dst_endpoint = 1;
        report_cmd.zcl_basic_cmd.src_endpoint = endpoint;
        report_cmd.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
        report_cmd.clusterID = ESP_ZB_ZCL_CLUSTER_ID_POWER_CONFIG;
        report_cmd.attributeID = ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_REMAINING_ID;
        report_cmd.direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_CLI;
        
        esp_zb_zcl_report_attr_cmd_req(&report_cmd);
        esp_zb_lock_release();
    }
}

static void bdb_start_top_level_commissioning_cb(uint8_t mode_mask)
{
    if(esp_zb_bdb_start_top_level_commissioning(mode_mask) != ESP_OK)
    {
        DebugLogger::getInstance().print(DEBUG_ZIGBEE, DEBUG_WARNING, "commissioning failed");
    }
}
void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct)
{
    uint32_t *p_sg_p = signal_struct->p_app_signal;
    esp_err_t err_status = signal_struct->esp_err_status;
    esp_zb_app_signal_type_t sig_type = static_cast<esp_zb_app_signal_type_t>(*p_sg_p);
    switch (sig_type)
    {
    case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
        DebugLogger::getInstance().print(DEBUG_ZIGBEE, DEBUG_INFO, "initialize Zigbee stack");
        esp_zb_scheduler_alarm(bdb_start_top_level_commissioning_cb, ESP_ZB_BDB_MODE_INITIALIZATION, 1000);
        break;
        
    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
        if (err_status == ESP_OK)
        {
            DebugLogger::getInstance().print(DEBUG_ZIGBEE, DEBUG_INFO, "Deferred driver initialization ...");
            DebugLogger::getInstance().print(DEBUG_ZIGBEE, DEBUG_INFO, "Device started up in %s factory-reset mode", esp_zb_bdb_is_factory_new() ? "" : "non");
            if (esp_zb_bdb_is_factory_new())
            {
                DebugLogger::getInstance().print(DEBUG_ZIGBEE, DEBUG_INFO, "start network steering");
                esp_zb_scheduler_alarm(bdb_start_top_level_commissioning_cb, ESP_ZB_BDB_MODE_NETWORK_STEERING, 1000);
            }
            else
            {
                DebugLogger::getInstance().print(DEBUG_ZIGBEE, DEBUG_INFO, "device reconnected");
                connected.store(true);
            }
        }
        else
        {
            DebugLogger::getInstance().print(DEBUG_ZIGBEE, DEBUG_INFO, "failed to initialize Zigbee stack (status: %s)", esp_err_to_name(err_status));
        }
        break;

    case ESP_ZB_BDB_SIGNAL_STEERING:
        if (err_status == ESP_OK)
        {
            esp_zb_ieee_addr_t extended_pan_id;
            esp_zb_get_extended_pan_id(extended_pan_id);
            DebugLogger::getInstance().print(DEBUG_ZIGBEE, DEBUG_INFO, "joined network successfully (Extended PAN ID: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, PAN ID: 0x%04hx, Channel:%d, Short Address: 0x%04hx)",
            extended_pan_id[7], extended_pan_id[6], extended_pan_id[5], extended_pan_id[4],
                     extended_pan_id[3], extended_pan_id[2], extended_pan_id[1], extended_pan_id[0],
                    esp_zb_get_pan_id(), esp_zb_get_current_channel(), esp_zb_get_short_address());
            connected.store(true);
        }
        else
        {
            DebugLogger::getInstance().print(DEBUG_ZIGBEE, DEBUG_INFO, "network steering was not successful (status: %s)", esp_err_to_name(err_status));
            esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb, ESP_ZB_BDB_MODE_NETWORK_STEERING, 1000);
        }
        break;        

    default:
        if(sig_type != ESP_ZB_COMMON_SIGNAL_CAN_SLEEP)
        {
            DebugLogger::getInstance().print(DEBUG_ZIGBEE, DEBUG_INFO, "ZDO signal: %s (0x%x), status: %s", esp_zb_zdo_signal_to_string(sig_type), sig_type, esp_err_to_name(err_status));
        }
        break;
    }
}

extern "C"  esp_err_t zbActionHandler(esp_zb_core_action_callback_id_t callback_id, const void *message)
{
    switch (callback_id)
    {
        case ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID:
        {
            esp_zb_zcl_set_attr_value_message_t *set_attr_msg = (esp_zb_zcl_set_attr_value_message_t *)message;
            DebugLogger::getInstance().print(DEBUG_ZIGBEE, DEBUG_INFO, "Attribute %x updated on ep %d, cluster 0x%x by coordinator", 
                     set_attr_msg->attribute.id, set_attr_msg->info.dst_endpoint, set_attr_msg->info.cluster);
            break;
        }

        case ESP_ZB_CORE_CMD_READ_ATTR_RESP_CB_ID:
            DebugLogger::getInstance().print(DEBUG_ZIGBEE, DEBUG_INFO, "Asked to read attribute");
            break;

        case ESP_ZB_CORE_REPORT_ATTR_CB_ID:
            DebugLogger::getInstance().print(DEBUG_ZIGBEE, DEBUG_INFO, "Attribute successfully reported to coordinator");
            break;

        case ESP_ZB_CORE_IAS_ZONE_ENROLL_RESPONSE_VALUE_CB_ID:
            DebugLogger::getInstance().print(DEBUG_ZIGBEE, DEBUG_INFO, "Enrolling successful");
            break;

        case ESP_ZB_CORE_CMD_DEFAULT_RESP_CB_ID:
            DebugLogger::getInstance().print(DEBUG_ZIGBEE, DEBUG_INFO, "Attribute report successful");
            break;

        default:
            DebugLogger::getInstance().print(DEBUG_ZIGBEE, DEBUG_INFO, "Unhandled action (ID:%d)", callback_id);
            break;
    }
    return ESP_OK;
}

esp_err_t initZigbee()
{
    esp_zb_cfg_t cfg{};
    cfg.esp_zb_role = ESP_ZB_DEVICE_TYPE_ED;
    cfg.install_code_policy = false;
    cfg.nwk_cfg.zed_cfg.ed_timeout = ESP_ZB_ED_AGING_TIMEOUT_64MIN;
    cfg.nwk_cfg.zed_cfg.keep_alive = 3600;
    esp_zb_init(&cfg);

    return ESP_OK;
}

esp_err_t initDevice()
{
    esp_zb_ep_list_t *ep_list = esp_zb_ep_list_create();

    // EP 1 - Reed Sensor 1
    esp_zb_endpoint_config_t ep1_config = { .endpoint = 1, .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID, 
                                            .app_device_id = ESP_ZB_HA_SIMPLE_SENSOR_DEVICE_ID, .app_device_version = 0 };
    esp_zb_ep_list_add_ep(ep_list, createReedClusterList(), ep1_config);

    // EP 2 - Reed Sensor 2
    esp_zb_endpoint_config_t ep2_config = { .endpoint = 2, .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID, 
                                            .app_device_id = ESP_ZB_HA_SIMPLE_SENSOR_DEVICE_ID, .app_device_version = 0 };
    esp_zb_ep_list_add_ep(ep_list, createReedClusterList(), ep2_config);

    // EP 3 - Battery Status
    esp_zb_endpoint_config_t ep3_config = { .endpoint = 3, .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID, 
                                            .app_device_id = ESP_ZB_HA_SIMPLE_SENSOR_DEVICE_ID, .app_device_version = 0 };
    esp_zb_ep_list_add_ep(ep_list, createBatteryClusterList(), ep3_config);

    esp_zb_device_register(ep_list);
    esp_zb_core_action_handler_register(zbActionHandler);
    return ESP_OK;
}