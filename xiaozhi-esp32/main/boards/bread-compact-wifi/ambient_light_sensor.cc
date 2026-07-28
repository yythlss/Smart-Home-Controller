#include "ambient_light_sensor.h"

#include <esp_err.h>
#include <esp_log.h>

#define TAG "AmbientLight"

AmbientLightSensor::AmbientLightSensor(adc_oneshot_unit_handle_t adc_handle,
                                       adc_channel_t adc_channel, int dark_raw, int bright_raw,
                                       adc_atten_t attenuation)
    : adc_handle_(adc_handle), adc_channel_(adc_channel), attenuation_(attenuation),
      filter_(dark_raw, bright_raw) {
}

bool AmbientLightSensor::Initialize() {
    if (initialized_) {
        return true;
    }
    if (adc_handle_ == nullptr) {
        ESP_LOGE(TAG, "ADC handle is unavailable");
        return false;
    }

    adc_oneshot_chan_cfg_t channel_config = {};
    channel_config.atten = attenuation_;
    channel_config.bitwidth = ADC_BITWIDTH_12;
    const esp_err_t err = adc_oneshot_config_channel(adc_handle_, adc_channel_, &channel_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure ADC channel %d: %s", adc_channel_, esp_err_to_name(err));
        return false;
    }

    initialized_ = true;
    ESP_LOGI(TAG, "Initialized on ADC channel %d", adc_channel_);
    return true;
}

bool AmbientLightSensor::Read(int& raw_value, float& brightness_percent) {
    if (!initialized_ && !Initialize()) {
        return false;
    }

    int raw = 0;
    const esp_err_t err = adc_oneshot_read(adc_handle_, adc_channel_, &raw);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "ADC read failed: %s", esp_err_to_name(err));
        return false;
    }

    raw_value = raw;
    brightness_percent = filter_.PushSample(raw);
    return true;
}
