#include "dht11_sensor.h"

#include <cstdint>

#define TAG "DHT11"

namespace {
bool ValidateReading(float humidity, float temperature, const uint8_t data[5]) {
    if (data[1] > 9 || data[3] > 9 ||
        humidity > 100.0f || temperature > 60.0f) {
        ESP_LOGW(TAG, "Range error: raw=%u,%u,%u,%u humidity=%.1f temp=%.1f",
                 data[0], data[1], data[2], data[3], humidity, temperature);
        return false;
    }
    return true;
}
} // namespace

Dht11Sensor::Dht11Sensor(gpio_num_t pin) : pin_(pin) {
    // DHT11 是单总线协议：主机只能主动拉低总线，写 1 时应释放总线，由上拉保持高电平。
    // 这里使用开漏输入输出模式，比普通推挽输出更符合 DHT11 的总线时序。
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << pin_),
        .mode = GPIO_MODE_INPUT_OUTPUT_OD,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(pin_, 1);
    ESP_LOGI(TAG, "DHT11 initialized on GPIO %d (open-drain + pull-up)", pin_);
}

Dht11Sensor::~Dht11Sensor() {
    gpio_reset_pin(pin_);
}

bool Dht11Sensor::WaitForLevel(int level, int timeout_us) {
    int waited = 0;
    while (gpio_get_level(pin_) != level) {
        if (waited >= timeout_us) {
            return false;
        }
        esp_rom_delay_us(1);
        waited++;
    }
    return true;
}

void Dht11Sensor::SendStartSignal() {
    // 起始信号：主机拉低不少于 18ms，然后释放总线等待 DHT11 应答。
    gpio_set_level(pin_, 0);
    vTaskDelay(pdMS_TO_TICKS(20));

    gpio_set_level(pin_, 1);
    esp_rom_delay_us(40);
}

bool Dht11Sensor::Read() {
    uint8_t data[5] = {0};

    SendStartSignal();

    // DHT11 应答约为 80us 低电平 + 80us 高电平。这里给 200us 余量，便于排查接线或上拉问题。
    if (!WaitForLevel(0, 200)) {
        ESP_LOGW(TAG, "No response (low), check wiring: VCC=3.3V DATA=GPIO%d GND", pin_);
        return false;
    }
    if (!WaitForLevel(1, 200)) {
        ESP_LOGW(TAG, "No response (high)");
        return false;
    }
    if (!WaitForLevel(0, 200)) {
        ESP_LOGW(TAG, "Response high timeout");
        return false;
    }

    // 读取 40 位数据时需要微秒级判断高电平宽度。短暂进入临界区，降低 Wi-Fi/系统中断造成的误判概率。
    portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
    portENTER_CRITICAL(&mux);
    for (int i = 0; i < 40; i++) {
        int low_us = 0;
        while (gpio_get_level(pin_) == 0 && low_us < 100) {
            esp_rom_delay_us(1);
            low_us++;
        }
        if (low_us >= 100) {
            portEXIT_CRITICAL(&mux);
            ESP_LOGW(TAG, "Bit %d start low timeout", i);
            return false;
        }

        int high_us = 0;
        while (gpio_get_level(pin_) == 1 && high_us < 85) {
            esp_rom_delay_us(1);
            high_us++;
        }

        data[i / 8] <<= 1;
        if (high_us > 40) {
            data[i / 8] |= 1;
        }
    }
    portEXIT_CRITICAL(&mux);

    const uint8_t checksum = data[0] + data[1] + data[2] + data[3];
    if (checksum != data[4]) {
        ESP_LOGW(TAG, "Checksum error: %d+%d+%d+%d=%d != %d",
                 data[0], data[1], data[2], data[3], checksum, data[4]);
        return false;
    }

    const float humidity = static_cast<float>(data[0]) + static_cast<float>(data[1]) * 0.1f;
    const float temperature = static_cast<float>(data[2]) + static_cast<float>(data[3]) * 0.1f;
    if (!ValidateReading(humidity, temperature, data)) {
        return false;
    }

    humidity_ = humidity;
    temperature_ = temperature;

    ESP_LOGI(TAG, "OK: %.1f C, %.1f%%", temperature_, humidity_);
    return true;
}
