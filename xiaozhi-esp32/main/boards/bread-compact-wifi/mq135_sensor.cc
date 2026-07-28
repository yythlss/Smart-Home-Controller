#include "mq135_sensor.h"

#define TAG "MQ135"

Mq135Sensor::Mq135Sensor(adc_unit_t adc_unit, adc_channel_t adc_channel,
                         adc_atten_t attenuation)
    : adc_unit_(adc_unit), adc_channel_(adc_channel) {

    // 初始化 ADC 单元
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = adc_unit_,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &adc_handle_));

    // 配置 ADC 通道
    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = attenuation,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle_, adc_channel_, &chan_cfg));

    ESP_LOGI(TAG, "MQ135 sensor initialized on ADC%d channel %d",
             adc_unit_ + 1, adc_channel_);
}

Mq135Sensor::~Mq135Sensor() {
    if (adc_handle_) {
        adc_oneshot_del_unit(adc_handle_);
    }
}

bool Mq135Sensor::ReadRaw(int& raw_value) {
    int raw = 0;
    esp_err_t ret = adc_oneshot_read(adc_handle_, adc_channel_, &raw);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "ADC read error: %d", ret);
        return false;
    }
    raw_value = raw;
    last_raw_ = raw;
    last_level_ = GetAirQualityLevel(raw);
    ESP_LOGI(TAG, "Raw: %d, Level: %s", raw, last_level_);
    return true;
}

bool Mq135Sensor::ReadVoltage(int& voltage_mv) {
    int raw = 0;
    if (!ReadRaw(raw)) return false;
    // ADC 12bit, 11dB 衰减：量程约 0-3100mV，公式换算
    // 使用 3300mV 参考电压
    voltage_mv = (raw * 3300) / 4095;
    return true;
}

const char* Mq135Sensor::GetAirQualityLevel(int raw_value) {
    if (raw_value < 500) {
        return "优";
    } else if (raw_value < 1000) {
        return "良";
    } else if (raw_value < 2000) {
        return "轻度污染";
    } else {
        return "重度污染";
    }
}
