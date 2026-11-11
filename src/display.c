#include "display.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include <stdio.h>
#include "freertos/task.h"
#include <stdbool.h>
#include <string.h>
#include "update_schedules.h"
#include "route_config.h"

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

#define LCD_TEXT_SIZE 81

static void generate_schedule_text(const RealtimeStop *schedule, char *text, size_t text_size)
{
    size_t line_limit;

    if (text_size == 0) {
        return;
    }
    text[0] = '\0';
    line_limit = text_size - 1;
        if (schedule == NULL || schedule->departures == NULL ||
            schedule->route_config_index >= ROUTE_CONFIG_COUNT) {
        return;
    }
    if (line_limit > 20) {
        line_limit = 20;
    }

    int prefix_length = snprintf(text, text_size, "%s: ",
                                 ROUTE_CONFIGS[schedule->route_config_index].name);
    if (prefix_length < 0 || (size_t)prefix_length > line_limit) {
        text[0] = '\0';
        return;
    }

    size_t used = (size_t)prefix_length;
    size_t departure_count = schedule->departure_count;
    if (departure_count == 0 || schedule->search_date_unix == 0) {
        if (used + 2 <= line_limit) {
            memcpy(text + used, "--", 3);
        }
        return;
    }
    if (departure_count > MAX_MERGED_DEPARTURES) {
        departure_count = MAX_MERGED_DEPARTURES;
    }
    bool show_route_number = ROUTE_CONFIGS[schedule->route_config_index].route_count > 1;
    for (size_t i = 0; i < departure_count; i++) {
        const RealtimeDeparture *departure = &schedule->departures[i];
        if (departure->departure_time_unix <= schedule->search_date_unix) {
            continue;
        }
        unsigned int minutes_until = (departure->departure_time_unix - schedule->search_date_unix) / 60;
        char entry[16];
        int entry_length = show_route_number
            ? snprintf(entry, sizeof(entry), "%u:%u", (unsigned int)departure->route_number, minutes_until)
            : snprintf(entry, sizeof(entry), "%u", minutes_until);
        size_t separator_length = used == (size_t)prefix_length ? 0 : 2;
        if (entry_length < 0 || used + separator_length + (size_t)entry_length > line_limit) {
            break;
        }
        if (separator_length != 0) {
            memcpy(text + used, ", ", separator_length);
            used += separator_length;
        }
        memcpy(text + used, entry, (size_t)entry_length + 1);
        used += (size_t)entry_length;
    }
}

// ── Low-level ────────────────────────────────────────────────────────────────

static esp_err_t pcf_write(uint8_t byte)
{
    return i2c_master_transmit(s_lcd, &byte, 1, 50);
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

static void schedule_display_task(void *arg)
{
    uint32_t displayed_generation = 0;
    (void)arg;

    while (true) {
        char text[LCD_TEXT_SIZE] = {0};
        bool schedule_updated = false;
        size_t updated_route_index = ROUTE_CONFIG_COUNT;

        if (xSemaphoreTake(stop_singleton_mutex, pdMS_TO_TICKS(MAX_STOP_SINGETON_MUTEX_WAIT_MS)) == pdTRUE) {
            if (schedule_generation != displayed_generation) {
                generate_schedule_text(stop_singleton, text, sizeof(text));
                displayed_generation = schedule_generation;
                if (stop_singleton->route_config_index < ROUTE_CONFIG_COUNT) {
                    updated_route_index = stop_singleton->route_config_index;
                    schedule_updated = true;
                }
            }
            xSemaphoreGive(stop_singleton_mutex);
        }

        if (schedule_updated) {
            ESP_LOGD(TAG, "Updating LCD row %u: %s", (unsigned int)updated_route_index, text);
            lcd_set_cursor(0, updated_route_index);
            lcd_print("                    ");
            lcd_set_cursor(0, updated_route_index);
            lcd_print(text);
        }

        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

void display()
{
    ESP_LOGD(TAG, "In display routine");
    i2c_master_bus_handle_t i2c_bus;
    init_i2c(&i2c_bus, &s_lcd);
    for (uint8_t addr = 0x08; addr <= 0x77; ++addr) {
        esp_err_t err = i2c_master_probe(i2c_bus, addr, 50);

        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Found I2C device at 0x%02X", addr);
        } else if (err != ESP_ERR_NOT_FOUND) {
            ESP_LOGW(TAG, "Probe 0x%02X failed: %s",
                    addr, esp_err_to_name(err));
        }
    }
    lcd_init();

    lcd_set_cursor(0, 0);
    lcd_print("Hello, World!");
    BaseType_t task_created = xTaskCreate(schedule_display_task, "schedule_display", 4096, NULL, 4, NULL);
    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create schedule display task");
    }
    ESP_LOGD(TAG, "Finished display routine");
}