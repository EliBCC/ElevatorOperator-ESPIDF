#include "display.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include <stdio.h>

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#include "esp_log.h"

static const char *TAG = "display";

// Settings
static const i2c_port_num_t i2c_port = 0;           // -1 for auto-select
static const gpio_num_t i2c_sda_pin = 4;            // GPIO number for SDA
static const gpio_num_t i2c_scl_pin = 2;            // GPIO number for SCL
static const uint8_t i2c_glitch_ignore_cnt = 7;     // 7 is typical
static const uint16_t lcd_addr = 0x27;              // WYTP26-2004BL I2C address
static const uint32_t scl_speed_hz = 100000;        // 100kHz (standard mode)
static const uint32_t sleep_time_ms = 1000;

void init_i2c(i2c_master_bus_handle_t *i2c_bus, i2c_master_dev_handle_t *lcd_dev)
{
    esp_err_t esp_ret;
    // Set I2C bus configuration
    i2c_master_bus_config_t bus_config = {
        .i2c_port = i2c_port,
        .sda_io_num = i2c_sda_pin,
        .scl_io_num = i2c_scl_pin,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = i2c_glitch_ignore_cnt,
        .flags.enable_internal_pullup = true,
    };

    // Initialize the I2C bus
    esp_ret = i2c_new_master_bus(&bus_config, i2c_bus);
    if (esp_ret != ESP_OK) {
        ESP_LOGD(TAG, "Error: Failed to initialize I2C bus\r\n");
        abort();
    }
    // Set I2C device configuration
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = lcd_addr,
        .scl_speed_hz = scl_speed_hz,
    };

    esp_ret = i2c_master_bus_add_device(*i2c_bus, &dev_config, lcd_dev);
    if (esp_ret != ESP_OK) {
        ESP_LOGD(TAG, "Error: Failed to initialize I2C device\r\n");
        abort();
    }
}

void display()
{
    i2c_master_bus_handle_t i2c_bus;
    i2c_master_dev_handle_t lcd_dev;
    init_i2c(&i2c_bus, &lcd_dev);
}