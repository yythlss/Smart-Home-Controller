#ifndef AMBIENT_LIGHT_SENSOR_H
#define AMBIENT_LIGHT_SENSOR_H

#include "ambient_light_filter.h"

#include <esp_adc/adc_oneshot.h>

class AmbientLightSensor {
public:
    AmbientLightSensor(adc_oneshot_unit_handle_t adc_handle, adc_channel_t adc_channel,
                       int dark_raw, int bright_raw,
                       adc_atten_t attenuation = ADC_ATTEN_DB_12);

    bool Initialize();
    bool Read(int& raw_value, float& brightness_percent);

private:
    adc_oneshot_unit_handle_t adc_handle_ = nullptr;
    adc_channel_t adc_channel_;
    adc_atten_t attenuation_;
    AmbientLightFilter filter_;
    bool initialized_ = false;
};

#endif // AMBIENT_LIGHT_SENSOR_H
