#include "wifi_board.h"
#include "codecs/no_audio_codec.h"
#include "display/display.h"
#include "system_reset.h"
#include "application.h"
#include "ambient_light_sensor.h"
#include "button.h"
#include "config.h"
#include "mcp_server.h"
#include "dht11_sensor.h"
#include "ld2450_sensor.h"
#include "mq135_sensor.h"
#include "serial_hmi.h"
#include "smart_home_controller.h"
#include "smart_home_http_server.h"
#include "led/single_led.h"
#include "assets/lang_config.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstdio>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>

#define TAG "TestBoard"

namespace {
bool EventTokenEquals(const char* value, const char* expected) {
    if (value == nullptr || expected == nullptr) {
        return false;
    }
    while (*value != '\0' && *expected != '\0') {
        if (std::toupper(static_cast<unsigned char>(*value)) !=
            std::toupper(static_cast<unsigned char>(*expected))) {
            return false;
        }
        ++value;
        ++expected;
    }
    return *value == '\0' && *expected == '\0';
}

void FormatHexBytes(const uint8_t* data, size_t size, char* output, size_t output_size) {
    if (output == nullptr || output_size == 0) {
        return;
    }
    if (data == nullptr || size == 0) {
        output[0] = '\0';
        return;
    }

    size_t offset = 0;
    for (size_t i = 0; i < size && offset + 4 < output_size; ++i) {
        const int written = snprintf(output + offset, output_size - offset, "%02X%s", data[i],
                                     (i + 1 < size) ? " " : "");
        if (written <= 0) {
            break;
        }
        offset += static_cast<size_t>(written);
    }
    output[output_size - 1] = '\0';
}

const char* AirStateFromScore(int score) {
    if (score >= 85) {
        return "优";
    }
    if (score >= 65) {
        return "良";
    }
    if (score >= 40) {
        return "一般";
    }
    return "差";
}
} // namespace

class CompactWifiBoard : public WifiBoard {
private:
    // 小智框架仍需要一个 Display 实例承接通知/聊天接口；当前物理 UI 由 TJC 串口屏负责，
    // 所以这里保留 NoDisplay，避免把串口屏 UART 和框架 Display 生命周期绑死。
    Display* display_ = nullptr;
    Button boot_button_;
    Button touch_button_;
    Button volume_up_button_;
    Button volume_down_button_;

    Dht11Sensor dht11_;
    Mq135Sensor mq135_;
    AmbientLightSensor ambient_light_;
    Ld2450Sensor ld2450_;
    // SerialHmi 是本板型唯一的 TJC UART 所有者：负责初始化 UART、发送控件命令、接收触摸事件。
    // 后续不要再新增第二套 uart_driver_install(TJC_UART_PORT, ...) 逻辑，否则会和事件轮询冲突。
    SerialHmi serial_hmi_;
    SmartHomeController smart_home_;
    SmartHomeHttpServer smart_home_http_;

    // ==================== 串口屏 ====================

    void InitializeSerialHmi() {
        display_ = new NoDisplay();
        if (!serial_hmi_.Initialize()) {
            ESP_LOGW(TAG, "Serial HMI init failed; XiaoZhi and sensor task will continue");
            return;
        }
        serial_hmi_.ShowBootScreen();
    }

    void UpdateSerialHmiFromEnvironment(const EnvironmentSample& environment) {
        SerialHmiAirQualityData hmi_data = {};
        hmi_data.has_temperature = environment.has_temperature;
        hmi_data.temperature_c = environment.temperature_c;
        hmi_data.has_humidity = environment.has_humidity;
        hmi_data.humidity_percent = environment.humidity_percent;
        hmi_data.has_mq135_raw = environment.has_mq135_raw;
        hmi_data.mq135_raw = environment.mq135_raw;
        hmi_data.air_state = AirStateFromScore(environment.air_score);
        hmi_data.air_score = environment.air_score;
        hmi_data.advice = environment.advice;
        hmi_data.ai_state = environment.manual_environment_mode ? "MANUAL" : "IDLE";
        hmi_data.comfort = environment.comfort;
        hmi_data.manual_environment_mode = environment.manual_environment_mode;
        hmi_data.environment_source = environment.environment_source;
        serial_hmi_.UpdateAirQuality(hmi_data);
    }

    // ==================== 传感器采集任务 ====================

    void SensorTask() {
        ESP_LOGI(TAG, "Sensor task running, interval=%d ms", SENSOR_READ_INTERVAL_MS);

        bool has_last_dht_reading = false;
        float last_temperature_c = 0.0f;
        float last_humidity_percent = 0.0f;

        while (true) {
            printf("\n---- Sensor Read ----\n");

            // DHT11
            bool dht_ok = dht11_.Read();
            if (dht_ok) {
                last_temperature_c = dht11_.GetTemperature();
                last_humidity_percent = dht11_.GetHumidity();
                has_last_dht_reading = true;
            }

            const bool display_dht_ok = dht_ok || has_last_dht_reading;
            float temp = display_dht_ok ? last_temperature_c : -99.0f;
            float humi = display_dht_ok ? last_humidity_percent : -1.0f;
            if (!dht_ok && has_last_dht_reading) {
                ESP_LOGW(TAG, "Use cached DHT11 reading after transient failure: %.1f C %.1f%%",
                         temp, humi);
            }
            printf("  DHT11  : %s%s  temp=%.1f°C  humi=%.1f%%\n",
                   dht_ok ? "OK" : "FAIL",
                   (!dht_ok && has_last_dht_reading) ? " (cached)" : "",
                   temp, humi);

            // MQ135
            int air_raw = 0;
            bool mq_ok = mq135_.ReadRaw(air_raw);
            const char* air_level = mq_ok ? mq135_.GetLastLevel() : "ERROR";
            printf("  MQ135  : %s  raw=%d  level=%s\n",
                   mq_ok ? "OK" : "FAIL", air_raw, air_level);

            int light_raw = 0;
            float light_percent = 0.0f;
            const bool light_ok = ambient_light_.Read(light_raw, light_percent);
            if (light_ok) {
                smart_home_.UpdateAmbientLight(light_percent);
            }
            printf("  Light  : %s  raw=%d  brightness=%.1f%%\n",
                   light_ok ? "OK" : "FAIL", light_raw, light_percent);

            EnvironmentSample environment = {};
            environment.has_temperature = display_dht_ok;
            environment.temperature_c = temp;
            environment.has_humidity = display_dht_ok;
            environment.humidity_percent = humi;
            environment.has_mq135_raw = mq_ok;
            environment.mq135_raw = air_raw;
            environment.air_score = mq_ok ? SerialHmi::EstimateAirScoreFromMq135Raw(air_raw) : 0;
            smart_home_.UpdateEnvironment(environment);
            UpdateSerialHmiFromEnvironment(smart_home_.GetLastSample());

            printf("---- Next: %d ms ----\n\n", SENSOR_READ_INTERVAL_MS);
            vTaskDelay(pdMS_TO_TICKS(SENSOR_READ_INTERVAL_MS));
        }
    }

    void RadarTask() {
        ESP_LOGI(TAG, "LD2450 radar task running on UART1");
        TickType_t last_detail_log_ticks = 0;
        TickType_t last_stats_log_ticks = 0;
        TickType_t last_raw_log_ticks = 0;

        while (true) {
            for (int frame_index = 0; frame_index < 4; ++frame_index) {
                Ld2450Snapshot snapshot = {};
                if (!ld2450_.Poll(snapshot, pdMS_TO_TICKS(LD2450_POLL_INTERVAL_MS))) {
                    break;
                }

                smart_home_.UpdateRadarObservation(snapshot.active_target_count);
                const TickType_t now = xTaskGetTickCount();
                if (now - last_detail_log_ticks < pdMS_TO_TICKS(1000)) {
                    continue;
                }

                last_detail_log_ticks = now;
                ESP_LOGI(TAG, "LD2450 targets=%u", snapshot.active_target_count);
                for (size_t index = 0; index < 3; ++index) {
                    const Ld2450Target& target = snapshot.targets[index];
                    if (!target.active) {
                        continue;
                    }
                    ESP_LOGI(TAG, "LD2450 target%u x=%d y=%d speed=%d resolution=%u",
                             static_cast<unsigned>(index + 1), target.x_mm, target.y_mm,
                             target.speed_mm_per_s, target.resolution_mm);
                }
            }

            const TickType_t now = xTaskGetTickCount();
            if (now - last_stats_log_ticks >= pdMS_TO_TICKS(5000)) {
                last_stats_log_ticks = now;
                ESP_LOGI(TAG, "LD2450 stats: bytes=%lu valid=%lu rejected=%lu",
                         static_cast<unsigned long>(ld2450_.GetReceivedByteCount()),
                         static_cast<unsigned long>(ld2450_.GetValidFrameCount()),
                         static_cast<unsigned long>(ld2450_.GetRejectedFrameCount()));
                if (ld2450_.GetValidFrameCount() == 0 && ld2450_.HasRawSample() &&
                    now - last_raw_log_ticks >= pdMS_TO_TICKS(5000)) {
                    last_raw_log_ticks = now;
                    char hex_buffer[512] = {};
                    // 打印最新一次 UART 读取
                    FormatHexBytes(ld2450_.GetRawSample(), ld2450_.GetRawSampleSize(), hex_buffer,
                                   sizeof(hex_buffer));
                    ESP_LOGW(TAG, "LD2450 raw sample len=%u data=%s",
                             static_cast<unsigned>(ld2450_.GetRawSampleSize()), hex_buffer);
                    // 打印完整接收缓冲区
                    if (ld2450_.HasBufferedData()) {
                        char buf_hex[512] = {};
                        FormatHexBytes(ld2450_.GetBufferedData(), ld2450_.GetBufferedDataSize(),
                                       buf_hex, sizeof(buf_hex));
                        ESP_LOGW(TAG, "LD2450 buffer len=%u data=%s",
                                 static_cast<unsigned>(ld2450_.GetBufferedDataSize()), buf_hex);
                    }
                }
            }
            vTaskDelay(pdMS_TO_TICKS(LD2450_POLL_INTERVAL_MS));
        }
    }

    // ==================== 串口屏触摸/滑动事件 ====================

    void ScreenEventTask() {
        ESP_LOGI(TAG, "Screen event task running");

        while (true) {
            SerialHmiEvent event = {};
            if (!serial_hmi_.PollEvent(event, pdMS_TO_TICKS(100))) {
                continue;
            }

            ESP_LOGI(TAG, "Screen event: raw=%s target=%s action=%s",
                     event.raw, event.target, event.action);

            if (event.type == SerialHmiEventType::kPageSelect) {
                // HMI 编辑器里的事件字符串容易出现大小写差异，这里统一按大小写不敏感处理。
                if (EventTokenEquals(event.target, "NEXT") ||
                    EventTokenEquals(event.target, "LEFT")) {
                    serial_hmi_.ShowNextPage();
                } else if (EventTokenEquals(event.target, "PREV") ||
                           EventTokenEquals(event.target, "RIGHT")) {
                    serial_hmi_.ShowPreviousPage();
                } else {
                    serial_hmi_.ShowNamedPage(event.target);
                }
                continue;
            }

            if (event.type == SerialHmiEventType::kSwipeAction) {
                if (EventTokenEquals(event.target, "LEFT")) {
                    serial_hmi_.ShowNextPage();
                } else if (EventTokenEquals(event.target, "RIGHT")) {
                    serial_hmi_.ShowPreviousPage();
                }
                continue;
            }

            if (event.type == SerialHmiEventType::kDeviceAction) {
                if (!smart_home_.HandleDeviceAction(event.target, event.action)) {
                    ESP_LOGW(TAG, "Unhandled device event: %s", event.raw);
                }
                continue;
            }

            if (event.type == SerialHmiEventType::kModeAction) {
                if (!smart_home_.HandleModeAction(event.target, event.action)) {
                    ESP_LOGW(TAG, "Unhandled mode event: %s", event.raw);
                }
                continue;
            }

            if (event.type == SerialHmiEventType::kEnvironmentAction) {
                if (smart_home_.HandleEnvironmentAction(event.target, event.action)) {
                    UpdateSerialHmiFromEnvironment(smart_home_.GetLastSample());
                } else {
                    ESP_LOGW(TAG, "Unhandled environment event: %s", event.raw);
                }
                continue;
            }
        }
    }

    // ==================== 按键 ====================

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });
        touch_button_.OnPressDown([this]() {
            Application::GetInstance().StartListening();
        });
        touch_button_.OnPressUp([this]() {
            Application::GetInstance().StopListening();
        });
        volume_up_button_.OnClick([this]() {
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() + 10;
            if (volume > 100) volume = 100;
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        });
        volume_up_button_.OnLongPress([this]() {
            GetAudioCodec()->SetOutputVolume(100);
            GetDisplay()->ShowNotification(Lang::Strings::MAX_VOLUME);
        });
        volume_down_button_.OnClick([this]() {
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() - 10;
            if (volume < 0) volume = 0;
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        });
        volume_down_button_.OnLongPress([this]() {
            GetAudioCodec()->SetOutputVolume(0);
            GetDisplay()->ShowNotification(Lang::Strings::MUTED);
        });
    }

public:
    CompactWifiBoard()
        : boot_button_(BOOT_BUTTON_GPIO)
        , touch_button_(TOUCH_BUTTON_GPIO)
        , volume_up_button_(VOLUME_UP_BUTTON_GPIO)
        , volume_down_button_(VOLUME_DOWN_BUTTON_GPIO)
        , dht11_(DHT11_GPIO)
        , mq135_(MQ135_ADC_UNIT, MQ135_ADC_CHANNEL)
        , ambient_light_(mq135_.GetAdcHandle(), AMBIENT_LIGHT_ADC_CHANNEL,
                         AMBIENT_LIGHT_DARK_RAW, AMBIENT_LIGHT_BRIGHT_RAW)
        , ld2450_(LD2450_UART_PORT, LD2450_UART_TX_PIN, LD2450_UART_RX_PIN,
                  LD2450_UART_BAUD_RATE)
        , serial_hmi_(TJC_UART_PORT, TJC_UART_TX_PIN, TJC_UART_RX_PIN, TJC_UART_BAUD_RATE)
        , smart_home_(&serial_hmi_)
        , smart_home_http_(&smart_home_) {

        printf("\n");
        printf("========================================\n");
        printf("  TestBoard - TJC 串口屏测试模式\n");
        printf("  输出: UART2(GPIO41/42) + USB Serial\n");
        printf("========================================\n\n");

        InitializeSerialHmi();
        InitializeButtons();
        if (!ambient_light_.Initialize()) {
            ESP_LOGW(TAG, "Ambient light sensor init failed; automatic lighting will wait for recovery");
        }
        if (!ld2450_.Initialize()) {
            ESP_LOGW(TAG, "LD2450 init failed; radar task will keep retrying");
        }
        smart_home_.SetPresenceDetectedCallback([]() {
            ESP_LOGI(TAG, "Presence detected, opening AI microphone");
            Application::GetInstance().StartListening();
        });
        smart_home_.SetAlarmOutputCallback([](bool active, const char* reason) {
            const std::string message = reason != nullptr ? reason : "";
            Application::GetInstance().Schedule([active, message]() {
                auto& app = Application::GetInstance();
                if (active) {
                    app.Alert("环境异常", message.c_str(), "triangle_exclamation",
                              Lang::Sounds::OGG_EXCLAMATION);
                } else {
                    app.DismissAlert();
                }
            });
        });
        smart_home_.Initialize();

        xTaskCreate(
            [](void* arg) {
                static_cast<CompactWifiBoard*>(arg)->SensorTask();
            },
            "sensor_task", 4096, this, 5, nullptr);

        xTaskCreate(
            [](void* arg) {
                static_cast<CompactWifiBoard*>(arg)->ScreenEventTask();
            },
            "screen_event_task", 4096, this, 5, nullptr);

        xTaskCreate(
            [](void* arg) {
                static_cast<CompactWifiBoard*>(arg)->RadarTask();
            },
            "ld2450_task", 4096, this, 5, nullptr);
    }

    virtual void SetNetworkEventCallback(NetworkEventCallback callback) override {
        WifiBoard::SetNetworkEventCallback(
            [this, callback = std::move(callback)](NetworkEvent event, const std::string& data) {
                if (callback) {
                    callback(event, data);
                }
                if (event == NetworkEvent::Connected) {
                    ESP_LOGI(TAG, "Network connected, starting mini program HTTP API");
                    smart_home_http_.Start();
                }
            });
    }

    virtual Led* GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }

    virtual AudioCodec* GetAudioCodec() override {
#ifdef AUDIO_I2S_METHOD_SIMPLEX
        static NoAudioCodecSimplex audio_codec(
            AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK,
            AUDIO_I2S_SPK_GPIO_DOUT,
            AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS,
            AUDIO_I2S_MIC_GPIO_DIN);
#else
        static NoAudioCodecDuplex audio_codec(
            AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS,
            AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN);
#endif
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }
};

DECLARE_BOARD(CompactWifiBoard);
