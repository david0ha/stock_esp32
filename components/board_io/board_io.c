/*
 * board_io.c — SHTC3 + PCF85063A + battery ADC over one shared I2C bus.
 * See board_io.h. Drivers are self-contained (no external sensor library) and
 * speak the ESP-IDF v6 i2c_master + adc_oneshot APIs directly.
 */
#include "board_io.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"

static const char *TAG = "board_io";

/* ---- I2C bus + device handles ------------------------------------------- */

/* Shared I2C bus pins (see docs/pinout.md): SDA=GPIO13, SCL=GPIO14. */
#define I2C_SDA_PIN       GPIO_NUM_13
#define I2C_SCL_PIN       GPIO_NUM_14
#define I2C_PORT          I2C_NUM_0
#define I2C_TIMEOUT_MS    1000

#define SHTC3_ADDR        0x70
#define PCF85063A_ADDR    0x51

static i2c_master_bus_handle_t s_bus;
static i2c_master_dev_handle_t s_shtc3;
static i2c_master_dev_handle_t s_rtc;

/* ---- battery ADC -------------------------------------------------------- */

#define BATT_ADC_UNIT     ADC_UNIT_1
#define BATT_ADC_CHANNEL  ADC_CHANNEL_3   /* GPIO4 */
#define BATT_DIVIDER      3.0f            /* board divides the cell voltage by 3 */
#define BATT_FULL_V       4.12f
#define BATT_EMPTY_V      3.00f

static adc_oneshot_unit_handle_t s_adc;
static adc_cali_handle_t         s_adc_cali;
static bool                      s_adc_ready;

/* ---- SHTC3 commands ----------------------------------------------------- */

#define SHTC3_WAKEUP            0x3517
#define SHTC3_SLEEP            0xB098
#define SHTC3_MEAS_T_RH_POLL   0x7866   /* T first, clock stretching disabled */
#define SHTC3_CRC_POLY         0x31

static uint8_t shtc3_crc(const uint8_t *data, int len) {
    uint8_t crc = 0xFF;
    for (int i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++)
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ SHTC3_CRC_POLY) : (uint8_t)(crc << 1);
    }
    return crc;
}

static bool shtc3_cmd(uint16_t cmd) {
    uint8_t b[2] = { (uint8_t)(cmd >> 8), (uint8_t)(cmd & 0xFF) };
    return i2c_master_transmit(s_shtc3, b, 2, I2C_TIMEOUT_MS) == ESP_OK;
}

/* ---- PCF85063A helpers -------------------------------------------------- */

#define PCF_REG_CTRL1     0x00
#define PCF_REG_SECONDS   0x04   /* sec,min,hour,day,wday,month,year (BCD) */
#define PCF_OS_FLAG       0x80   /* seconds bit7: oscillator stopped -> time lost */

static uint8_t dec2bcd(int v) { return (uint8_t)(((v / 10) << 4) | (v % 10)); }
static int     bcd2dec(uint8_t v) { return ((v >> 4) * 10) + (v & 0x0F); }

static bool rtc_read(uint8_t reg, uint8_t *buf, size_t len) {
    return s_rtc && i2c_master_transmit_receive(s_rtc, &reg, 1, buf, len, I2C_TIMEOUT_MS) == ESP_OK;
}

static bool rtc_write(uint8_t reg, const uint8_t *buf, size_t len) {
    if (!s_rtc) return false;
    uint8_t tmp[1 + 8];
    if (len > sizeof(tmp) - 1) return false;
    tmp[0] = reg;
    memcpy(&tmp[1], buf, len);
    return i2c_master_transmit(s_rtc, tmp, len + 1, I2C_TIMEOUT_MS) == ESP_OK;
}

/* ---- init --------------------------------------------------------------- */

static void i2c_init(void) {
    i2c_master_bus_config_t bus_cfg = {
        .clk_source                 = I2C_CLK_SRC_DEFAULT,
        .i2c_port                   = I2C_PORT,
        .scl_io_num                 = I2C_SCL_PIN,
        .sda_io_num                 = I2C_SDA_PIN,
        .glitch_ignore_cnt          = 7,
        .flags.enable_internal_pullup = true,
    };
    if (i2c_new_master_bus(&bus_cfg, &s_bus) != ESP_OK) {
        ESP_LOGE(TAG, "i2c bus init failed");
        return;
    }

    i2c_device_config_t shtc3_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = SHTC3_ADDR,
        .scl_speed_hz    = 400000,
    };
    if (i2c_master_bus_add_device(s_bus, &shtc3_cfg, &s_shtc3) != ESP_OK) {
        s_shtc3 = NULL;
        ESP_LOGW(TAG, "SHTC3 not found");
    }

    i2c_device_config_t rtc_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = PCF85063A_ADDR,
        .scl_speed_hz    = 300000,
    };
    if (i2c_master_bus_add_device(s_bus, &rtc_cfg, &s_rtc) != ESP_OK) {
        s_rtc = NULL;
        ESP_LOGW(TAG, "PCF85063A not found");
    } else {
        uint8_t ctrl1 = 0x00;   /* normal mode, run, 24h */
        rtc_write(PCF_REG_CTRL1, &ctrl1, 1);
    }
}

static void adc_init(void) {
    adc_oneshot_unit_init_cfg_t unit_cfg = { .unit_id = BATT_ADC_UNIT };
    if (adc_oneshot_new_unit(&unit_cfg, &s_adc) != ESP_OK) {
        ESP_LOGW(TAG, "adc unit init failed");
        return;
    }
    adc_oneshot_chan_cfg_t chan_cfg = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten    = ADC_ATTEN_DB_12,
    };
    if (adc_oneshot_config_channel(s_adc, BATT_ADC_CHANNEL, &chan_cfg) != ESP_OK) {
        ESP_LOGW(TAG, "adc channel config failed");
        return;
    }
    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id  = BATT_ADC_UNIT,
        .chan     = BATT_ADC_CHANNEL,
        .atten    = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    if (adc_cali_create_scheme_curve_fitting(&cali_cfg, &s_adc_cali) != ESP_OK) {
        ESP_LOGW(TAG, "adc calibration unavailable (raw voltage only)");
        s_adc_cali = NULL;
    }
    s_adc_ready = true;
}

void board_io_init(void) {
    i2c_init();
    adc_init();

    /* Wake + settle the SHTC3 so the first read after boot succeeds. */
    if (s_shtc3) {
        shtc3_cmd(SHTC3_WAKEUP);
        vTaskDelay(pdMS_TO_TICKS(2));
        shtc3_cmd(SHTC3_SLEEP);
    }
    ESP_LOGI(TAG, "init done (shtc3=%d rtc=%d adc=%d)",
             s_shtc3 != NULL, s_rtc != NULL, s_adc_ready);
}

/* ---- SHTC3 read --------------------------------------------------------- */

bool board_io_read_env(float *temp_c, float *rh) {
    if (!s_shtc3) return false;

    if (!shtc3_cmd(SHTC3_WAKEUP)) return false;
    vTaskDelay(pdMS_TO_TICKS(1));
    if (!shtc3_cmd(SHTC3_MEAS_T_RH_POLL)) { shtc3_cmd(SHTC3_SLEEP); return false; }
    vTaskDelay(pdMS_TO_TICKS(20));

    uint8_t d[6];
    bool ok = i2c_master_receive(s_shtc3, d, sizeof(d), I2C_TIMEOUT_MS) == ESP_OK;
    shtc3_cmd(SHTC3_SLEEP);
    if (!ok) return false;
    if (shtc3_crc(&d[0], 2) != d[2] || shtc3_crc(&d[3], 2) != d[5]) {
        ESP_LOGW(TAG, "SHTC3 CRC mismatch");
        return false;
    }

    uint16_t t_raw  = (uint16_t)(d[0] << 8 | d[1]);
    uint16_t rh_raw = (uint16_t)(d[3] << 8 | d[4]);
    if (temp_c) *temp_c = -45.0f + 175.0f * (float)t_raw / 65535.0f;
    if (rh)     *rh     = 100.0f * (float)rh_raw / 65535.0f;
    return true;
}

/* ---- RTC read / write --------------------------------------------------- */

bool board_io_rtc_get(struct tm *out_utc) {
    uint8_t b[7];
    if (!rtc_read(PCF_REG_SECONDS, b, sizeof(b))) return false;
    if (b[0] & PCF_OS_FLAG) return false;   /* oscillator stopped -> time lost */

    int year = 2000 + bcd2dec(b[6]);
    if (year < 2023) return false;          /* never set */

    struct tm t = {0};
    t.tm_sec  = bcd2dec(b[0] & 0x7F);
    t.tm_min  = bcd2dec(b[1] & 0x7F);
    t.tm_hour = bcd2dec(b[2] & 0x3F);
    t.tm_mday = bcd2dec(b[3] & 0x3F);
    t.tm_wday = b[4] & 0x07;
    t.tm_mon  = bcd2dec(b[5] & 0x1F) - 1;
    t.tm_year = year - 1900;
    t.tm_isdst = 0;
    if (out_utc) *out_utc = t;
    return true;
}

bool board_io_rtc_set(const struct tm *utc) {
    if (!utc) return false;
    uint8_t b[7];
    b[0] = dec2bcd(utc->tm_sec) & 0x7F;     /* writing seconds clears the OS flag */
    b[1] = dec2bcd(utc->tm_min);
    b[2] = dec2bcd(utc->tm_hour);
    b[3] = dec2bcd(utc->tm_mday);
    b[4] = (uint8_t)(utc->tm_wday & 0x07);
    b[5] = dec2bcd(utc->tm_mon + 1);
    b[6] = dec2bcd((utc->tm_year + 1900) - 2000);
    return rtc_write(PCF_REG_SECONDS, b, sizeof(b));
}

/* ---- battery ------------------------------------------------------------ */

float board_io_battery_voltage(void) {
    if (!s_adc_ready) return 0.0f;
    int raw = 0;
    if (adc_oneshot_read(s_adc, BATT_ADC_CHANNEL, &raw) != ESP_OK) return 0.0f;
    if (s_adc_cali) {
        int mv = 0;
        if (adc_cali_raw_to_voltage(s_adc_cali, raw, &mv) != ESP_OK) return 0.0f;
        return 0.001f * (float)mv * BATT_DIVIDER;
    }
    /* No calibration: approximate against the 12dB full-scale (~3.1V). */
    return ((float)raw / 4095.0f) * 3.1f * BATT_DIVIDER;
}

int board_io_battery_percent(void) {
    float v = board_io_battery_voltage();
    if (v <= 0.0f) return 0;
    if (v <= BATT_EMPTY_V) return 0;
    if (v >= BATT_FULL_V)  return 100;
    return (int)((v - BATT_EMPTY_V) / (BATT_FULL_V - BATT_EMPTY_V) * 100.0f + 0.5f);
}
