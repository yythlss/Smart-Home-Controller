#ifndef SERIAL_HMI_H
#define SERIAL_HMI_H

#include <driver/gpio.h>
#include <driver/uart.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <cstddef>

enum class SerialHmiEventType {
    kUnknown,
    // 页面入口或透明热区事件，例如 BTN,PAGE,AIR_DETAIL / BTN,PAGE,NEXT。
    kPageSelect,
    // 智能家居设备按钮事件，例如 BTN,DEVICE,FAN,TOGGLE。
    kDeviceAction,
    // 模式按钮事件，例如 BTN,MODE,AUTO,TOGGLE。
    kModeAction,
    // 手动环境/模拟场景事件，例如 BTN,ENV,SCENE,HOT。
    kEnvironmentAction,
    // HMI 侧滑动事件，例如 SWIPE,LEFT / SWIPE,RIGHT。
    kSwipeAction,
};

struct SerialHmiEvent {
    SerialHmiEventType type = SerialHmiEventType::kUnknown;
    // 事件目标，按事件类型分别表示页面名、设备名、模式名或滑动方向。
    char target[32] = {};
    // 可选动作字段，设备/模式事件常用，例如 TOGGLE。
    char action[24] = {};
    // 原始事件字符串，保留给日志和排查 HMI 事件配置使用。
    char raw[96] = {};
};

struct SerialHmiAirQualityData {
    // 这些 has_* 标志用于区分真实读数和占位显示，避免把失败读数误显示成有效数据。
    bool has_temperature = false;
    float temperature_c = 0.0f;

    bool has_humidity = false;
    float humidity_percent = 0.0f;

    bool has_mq135_raw = false;
    int mq135_raw = 0;
    const char* air_state = "UNKNOWN";

    // 预留给后续真实传感器；当前 page1 不显示这些浓度占位。
    bool has_pm25 = false;
    int pm25_ugm3 = 0;

    bool has_co2 = false;
    int co2_ppm = 0;

    bool has_tvoc = false;
    int tvoc_ppb = 0;

    int air_score = 0;
    const char* advice = "Waiting for sensor data";
    const char* ai_state = "IDLE";
    const char* comfort = "待计算";
    bool manual_environment_mode = false;
    const char* environment_source = "sensor";
};

class SerialHmi {
public:
    SerialHmi(uart_port_t uart_port, gpio_num_t tx_pin, gpio_num_t rx_pin, int baud_rate);
    ~SerialHmi();

    bool Initialize();
    // 发送 TJC/USART HMI 原生命令；函数会自动追加 FF FF FF 结束符。
    bool SendCommand(const char* command);
    // 写文本控件，最终命令形如 t_temp.txt="26.0 C"。
    bool SetText(const char* widget, const char* text);
    // 写数值控件，常用于进度条、滑块等 .val 属性。
    bool SetValue(const char* widget, int value);
    void ShowBootScreen();
    // 固件只负责 page id 切换；滑动动效和页面布局由 HMI 工程实现。
    bool ShowPage(int page_id);
    bool ShowNamedPage(const char* page_name);
    bool ShowNextPage();
    bool ShowPreviousPage();
    // 缓存最新传感器数据并按当前页面刷新相关控件。
    void UpdateAirQuality(const SerialHmiAirQualityData& data);
    // 轮询屏幕发回的 ASCII 事件，支持换行或 0xFF 作为事件结束。
    bool PollEvent(SerialHmiEvent& event, TickType_t timeout_ticks = 0);

    static int EstimateAirScoreFromMq135Raw(int raw_value);
    static const char* AdviceFromMq135Raw(int raw_value);

private:
    static constexpr size_t kAirCurveHistorySize = 30;

    bool ParseEventLine(const char* line, SerialHmiEvent& event);
    void ResetEventBuffer();
    void RefreshCurrentPage();
    void BeginBatchRefresh();
    void EndBatchRefresh();
    void RefreshHomePage(const SerialHmiAirQualityData& data);
    void RefreshAirDetailPage(const SerialHmiAirQualityData& data);
    void RefreshAiSettingsPage(const SerialHmiAirQualityData& data);
    void RecordAirCurveScore(int score);
    bool ClearCurve(int curve_id, int channel);
    bool AddCurvePoint(int curve_id, int channel, int value);
    void ReplayAirCurveHistory();
    static int ClampScore(int score);
    static void CopyToken(char* destination, size_t destination_size, const char* source);
    static void EscapeText(const char* input, char* output, size_t output_size);

    uart_port_t uart_port_;
    gpio_num_t tx_pin_;
    gpio_num_t rx_pin_;
    int baud_rate_;
    bool initialized_ = false;
    int current_page_id_ = -1;
    TickType_t last_page_switch_ticks_ = 0;
    bool has_last_air_quality_data_ = false;
    SerialHmiAirQualityData last_air_quality_data_ = {};
    int air_curve_scores_[kAirCurveHistorySize] = {};
    size_t air_curve_write_index_ = 0;
    size_t air_curve_count_ = 0;
    SemaphoreHandle_t tx_mutex_ = nullptr;
    char event_buffer_[96] = {};
    size_t event_buffer_len_ = 0;
};

#endif // SERIAL_HMI_H
