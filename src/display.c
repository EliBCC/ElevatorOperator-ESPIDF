#include "display.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include <stdio.h>
#include "freertos/task.h"
#include <stdbool.h>

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#include "esp_log.h"

static const char *TAG = "display";

// Settings
static const i2c_port_num_t i2c_port = 0;           // -1 for auto-select
static const gpio_num_t i2c_sda_pin = 22;            // GPIO number for SDA
static const gpio_num_t i2c_scl_pin = 23;            // GPIO number for SCL
static const uint8_t i2c_glitch_ignore_cnt = 7;     // 7 is typical
static const uint16_t lcd_addr = 0x27;              // WYTP26-2004BL I2C address
static const uint32_t scl_speed_hz = 100000;        // 100kHz (standard mode)
static const uint32_t sleep_time_ms = 1000;

// PCF8574 → HD44780 pin mapping
#define LCD_RS  0x01    // P0
#define LCD_RW  0x02    // P1
#define LCD_EN  0x04    // P2
#define LCD_BL  0x08    // P3 (backlight)
// P4–P7 carry D4–D7

// HD44780 commands
#define CMD_CLEAR       0x01
#define CMD_ENTRY_MODE  0x06    // increment, no display shift
#define CMD_DISPLAY_ON  0x0C    // display on, cursor off, blink off
#define CMD_DISPLAY_OFF 0x08
#define CMD_FUNC_4BIT   0x28    // 4-bit, 2-line, 5x8 font

// DDRAM row start addresses for 20x4
static const uint8_t ROW_ADDR[4] = { 0x00, 0x40, 0x14, 0x54 };

static i2c_master_dev_handle_t s_lcd;
static uint8_t s_bl = LCD_BL;

// ── Low-level ────────────────────────────────────────────────────────────────

static esp_err_t pcf_write(uint8_t byte)
{
    return i2c_master_transmit(s_lcd, &byte, 1, pdMS_TO_TICKS(10));
}

// Pulse EN high → low with the supplied data byte
static esp_err_t lcd_pulse_en(uint8_t d)
{
    esp_err_t r;
    if ((r = pcf_write(d | LCD_EN))  != ESP_OK) return r;
    vTaskDelay(1);   // ≥450 ns; 1 tick (1 ms) is safe
    if ((r = pcf_write(d & ~LCD_EN)) != ESP_OK) return r;
    vTaskDelay(1);   // ≥37 µs command execution time
    return ESP_OK;
}

// Send one nibble (already in upper 4 bits), RS=0 for cmd, RS=LCD_RS for data
static esp_err_t lcd_nibble(uint8_t nibble_upper, uint8_t rs)
{
    return lcd_pulse_en((nibble_upper & 0xF0) | s_bl | rs);
}


// ── Command / character helpers ───────────────────────────────────────────────

static esp_err_t lcd_cmd(uint8_t cmd)
{
    esp_err_t r;
    if ((r = lcd_nibble(cmd,       0))      != ESP_OK) return r;  // high nibble
    if ((r = lcd_nibble(cmd << 4,  0))      != ESP_OK) return r;  // low nibble
    return ESP_OK;
}

static esp_err_t lcd_putc(char c)
{
    esp_err_t r;
    if ((r = lcd_nibble((uint8_t)c,       LCD_RS)) != ESP_OK) return r;
    if ((r = lcd_nibble((uint8_t)c << 4,  LCD_RS)) != ESP_OK) return r;
    return ESP_OK;
}

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

    ESP_LOGD(TAG, "Setup complete");
}

void lcd_init(void)
{
    vTaskDelay(pdMS_TO_TICKS(50));          // power-on wait >40 ms

    // HD44780 reset: send 0x3 nibble 3× to guarantee 8-bit mode
    lcd_nibble(0x30, 0);  vTaskDelay(pdMS_TO_TICKS(5));
    lcd_nibble(0x30, 0);  vTaskDelay(pdMS_TO_TICKS(1));
    lcd_nibble(0x30, 0);  vTaskDelay(pdMS_TO_TICKS(1));

    // Switch to 4-bit mode
    lcd_nibble(0x20, 0);  vTaskDelay(pdMS_TO_TICKS(1));

    // Configure: 4-bit, 2-line, 5×8; display off; clear; entry mode; display on
    lcd_cmd(CMD_FUNC_4BIT);
    lcd_cmd(CMD_DISPLAY_OFF);
    lcd_cmd(CMD_CLEAR);  vTaskDelay(pdMS_TO_TICKS(2));  // clear needs >1.5 ms
    lcd_cmd(CMD_ENTRY_MODE);
    lcd_cmd(CMD_DISPLAY_ON);

    ESP_LOGD(TAG, "LCD init OK");
}

void lcd_set_cursor(uint8_t col, uint8_t row)
{
    if (row > 3)  row = 3;
    if (col > 19) col = 19;
    lcd_cmd(0x80 | (ROW_ADDR[row] + col));
}

void lcd_print(const char *str)
{
    while (*str) lcd_putc(*str++);
}

void lcd_clear(void)
{
    lcd_cmd(CMD_CLEAR);
    vTaskDelay(pdMS_TO_TICKS(2));
}

void lcd_backlight(bool on)
{
    s_bl = on ? LCD_BL : 0;
    pcf_write(s_bl);
}

void display()
{
    i2c_master_bus_handle_t i2c_bus;
    init_i2c(&i2c_bus, &s_lcd);
    lcd_init();

    lcd_set_cursor(0, 0);
    lcd_print("Hello, World!");
}