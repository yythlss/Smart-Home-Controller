#ifndef DHT11_SENSOR_H
#define DHT11_SENSOR_H

#include <driver/gpio.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

/**
 * @brief DHT11 温湿度传感器驱动
 *
 * 使用单总线协议，通过 GPIO 位操作实现通信。
 * 每次调用 Read() 读取一次温湿度数据。
 */
class Dht11Sensor {
public:
    /**
     * @brief 构造函数
     * @param pin 数据引脚（GPIO 号）
     */
    explicit Dht11Sensor(gpio_num_t pin);

    /** @brief 析构，释放 GPIO */
    ~Dht11Sensor();

    /**
     * @brief 读取一次传感器数据
     * @return true 读取成功（校验通过），false 读取失败
     */
    bool Read();

    /** @brief 获取最近一次读取的温度值（摄氏度） */
    float GetTemperature() const { return temperature_; }

    /** @brief 获取最近一次读取的湿度值（%RH） */
    float GetHumidity() const { return humidity_; }

private:
    gpio_num_t pin_;
    float temperature_ = 0.0f;
    float humidity_ = 0.0f;

    /** @brief 等待指定电平，超时返回 false */
    bool WaitForLevel(int level, int timeout_us);

    /** @brief 发送起始信号 */
    void SendStartSignal();
};

#endif // DHT11_SENSOR_H
