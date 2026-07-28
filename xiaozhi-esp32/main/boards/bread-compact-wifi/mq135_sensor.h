#ifndef MQ135_SENSOR_H
#define MQ135_SENSOR_H

#include <driver/gpio.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_log.h>

/**
 * @brief MQ135 空气质量传感器驱动
 *
 * 通过 ADC 读取模拟电压输出，判断空气污染程度。
 * 传感器 AO 输出随有害气体浓度升高而增大。
 */
class Mq135Sensor {
public:
    /**
     * @brief 构造函数
     * @param adc_unit    ADC 单元（ADC_UNIT_1 或 ADC_UNIT_2）
     * @param adc_channel ADC 通道
     * @param attenuation ADC 衰减系数（默认 11dB，量程 ~3.3V）
     */
    Mq135Sensor(adc_unit_t adc_unit, adc_channel_t adc_channel,
                adc_atten_t attenuation = ADC_ATTEN_DB_12);

    ~Mq135Sensor();

    /**
     * @brief 读取一次 ADC 原始值
     * @param[out] raw_value 原始 ADC 值（0-4095）
     * @return true 读取成功
     */
    bool ReadRaw(int& raw_value);

    /**
     * @brief 读取电压值（mV）
     * @param[out] voltage_mv 电压值
     * @return true 读取成功
     */
    bool ReadVoltage(int& voltage_mv);

    /**
     * @brief 获取空气质量等级
     *        0: 优（< 500），1: 良（500-1000）
     *        2: 轻度污染（1000-2000），3: 重度污染（> 2000）
     * @return 空气质量等级字符串
     */
    const char* GetAirQualityLevel(int raw_value);

    /**
     * @brief 最近一次读数：空气等级中文描述
     */
    const char* GetLastLevel() const { return last_level_; }

    /**
     * @brief 最近一次读数：原始 ADC 值
     */
    int GetLastRaw() const { return last_raw_; }

    // ADC1 can only have one oneshot unit owner. Other analog sensors reuse it.
    adc_oneshot_unit_handle_t GetAdcHandle() const { return adc_handle_; }

private:
    adc_oneshot_unit_handle_t adc_handle_ = nullptr;
    adc_unit_t adc_unit_;
    adc_channel_t adc_channel_;
    const char* last_level_ = "未知";
    int last_raw_ = 0;
};

#endif // MQ135_SENSOR_H
