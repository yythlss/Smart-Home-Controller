#include "serial_hmi.h"
#include "utf8_to_gbk.h"

#include <esp_err.h>
#include <esp_log.h>
#include <freertos/task.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdint>
#include <cstring>

#define TAG "SerialHmi"

namespace {
constexpr uint8_t kTjcCommandEnd[3] = {0xFF, 0xFF, 0xFF};
constexpr size_t kTjcCommandBufferSize = 256;
constexpr int kFirstPageId = 0;
constexpr int kHomePageId = 0;
constexpr int kAirDetailPageId = 1;
constexpr int kAiSettingsPageId = 3;
constexpr int kLastPageId = 3;
constexpr int kPageSwitchDebounceMs = 300;
constexpr int kAirCurveId = 12;
constexpr int kAirCurveChannel = 0;

bool StartsWith(const char* value, const char* prefix) {
    if (value == nullptr || prefix == nullptr) {
        return false;
    }
    while (*prefix != '\0') {
        if (*value == '\0') {
            return false;
        }
        if (std::toupper(static_cast<unsigned char>(*value)) !=
            std::toupper(static_cast<unsigned char>(*prefix))) {
            return false;
        }
        ++value;
        ++prefix;
    }
    return true;
}

bool TokenEquals(const char* value, const char* expected) {
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

int PageIdFromName(const char* page_name) {
    // 固件只维护页面语义到 page id 的映射；滑动动画和图标布局仍由 HMI 工程负责。
    if (TokenEquals(page_name, "HOME") ||
        TokenEquals(page_name, "AIR_HOME") ||
        TokenEquals(page_name, "DASHBOARD")) {
        return 0;
    }
    if (TokenEquals(page_name, "AIR_DETAIL") ||
        TokenEquals(page_name, "DETAIL")) {
        return 1;
    }
    if (TokenEquals(page_name, "SMART_HOME") ||
        TokenEquals(page_name, "CONTROL")) {
        return 2;
    }
    if (TokenEquals(page_name, "SETTINGS") ||
        TokenEquals(page_name, "AI")) {
        return 3;
    }
    return -1;
}

} // namespace

SerialHmi::SerialHmi(uart_port_t uart_port, gpio_num_t tx_pin, gpio_num_t rx_pin, int baud_rate)
    : uart_port_(uart_port), tx_pin_(tx_pin), rx_pin_(rx_pin), baud_rate_(baud_rate) {
    tx_mutex_ = xSemaphoreCreateRecursiveMutex();
}

SerialHmi::~SerialHmi() {
    if (initialized_) {
        uart_driver_delete(uart_port_);
    }
    if (tx_mutex_ != nullptr) {
        vSemaphoreDelete(tx_mutex_);
    }
}

bool SerialHmi::Initialize() {
    uart_config_t uart_config = {};
    uart_config.baud_rate = baud_rate_;
    uart_config.data_bits = UART_DATA_8_BITS;
    uart_config.parity = UART_PARITY_DISABLE;
    uart_config.stop_bits = UART_STOP_BITS_1;
    uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart_config.source_clk = UART_SCLK_DEFAULT;

    esp_err_t ret = uart_driver_install(uart_port_, 1024, 1024, 0, nullptr, 0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "uart_driver_install failed: %s", esp_err_to_name(ret));
        return false;
    }

    ret = uart_param_config(uart_port_, &uart_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "uart_param_config failed: %s", esp_err_to_name(ret));
        return false;
    }

    ret = uart_set_pin(uart_port_, tx_pin_, rx_pin_, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "uart_set_pin failed: %s", esp_err_to_name(ret));
        return false;
    }

    initialized_ = true;
    ESP_LOGI(TAG, "TJC UART ready: UART%d TX=GPIO%d RX=GPIO%d baud=%d",
             uart_port_, tx_pin_, rx_pin_, baud_rate_);
    return true;
}

bool SerialHmi::SendCommand(const char* command) {
    if (command == nullptr || command[0] == '\0') {
        return false;
    }

    bool locked = false;
    if (tx_mutex_ != nullptr) {
        locked = xSemaphoreTakeRecursive(tx_mutex_, portMAX_DELAY) == pdTRUE;
        if (!locked) {
            ESP_LOGW(TAG, "Skip command because UART TX mutex timed out: %s", command);
            return false;
        }
    }

    const bool ok = SendCommandLocked(command);

    if (locked) {
        xSemaphoreGiveRecursive(tx_mutex_);
    }
    return ok;
}

bool SerialHmi::SendCommandLocked(const char* command) {
    if (command == nullptr || command[0] == '\0') {
        return false;
    }

    bool ok = true;
    if (initialized_) {
        // USART HMI/TJC 文本控件通常按 GBK 字库显示中文；USB 日志仍打印 UTF-8 原文，方便 monitor 排查。
        char encoded_command[kTjcCommandBufferSize] = {};
        const size_t encoded_len = Utf8ToGbk(command, encoded_command, sizeof(encoded_command));
        if (encoded_len >= sizeof(encoded_command) - 1) {
            ESP_LOGW(TAG, "TJC command may be truncated after GBK encoding: %s", command);
        }

        const int command_len = static_cast<int>(encoded_len);
        const int written_command = uart_write_bytes(uart_port_, encoded_command, command_len);
        const int written_end = uart_write_bytes(uart_port_, kTjcCommandEnd, sizeof(kTjcCommandEnd));
        ok = (written_command == command_len) && (written_end == static_cast<int>(sizeof(kTjcCommandEnd)));
        if (!ok) {
            ESP_LOGW(TAG, "UART write incomplete: command=%d/%d end=%d/3",
                     written_command, command_len, written_end);
        }
    } else {
        ok = false;
        ESP_LOGW(TAG, "TJC UART not initialized, command only printed: %s", command);
    }

    std::printf("[TJC] %s\n", command);
    return ok;
}

bool SerialHmi::BeginTxTransaction() {
    if (tx_mutex_ == nullptr) {
        ESP_LOGE(TAG, "UART TX mutex is unavailable");
        return false;
    }
    if (xSemaphoreTakeRecursive(tx_mutex_, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to lock UART TX transaction");
        return false;
    }
    return true;
}

void SerialHmi::EndTxTransaction() {
    xSemaphoreGiveRecursive(tx_mutex_);
}

bool SerialHmi::SetText(const char* widget, const char* text) {
    if (widget == nullptr || widget[0] == '\0') {
        return false;
    }

    char escaped[96] = {};
    EscapeText(text != nullptr ? text : "--", escaped, sizeof(escaped));

    char command[144] = {};
    std::snprintf(command, sizeof(command), "%s.txt=\"%s\"", widget, escaped);
    return SendCommand(command);
}

bool SerialHmi::SetValue(const char* widget, int value) {
    if (widget == nullptr || widget[0] == '\0') {
        return false;
    }

    char command[64] = {};
    std::snprintf(command, sizeof(command), "%s.val=%d", widget, value);
    return SendCommand(command);
}

void SerialHmi::ShowBootScreen() {
    ShowPage(0);

    SerialHmiAirQualityData boot_data = {};
    boot_data.advice = "System starting";
    boot_data.ai_state = "IDLE";
    UpdateAirQuality(boot_data);
}

bool SerialHmi::ShowPage(int page_id) {
    if (page_id < kFirstPageId || page_id > kLastPageId) {
        ESP_LOGW(TAG, "Ignore invalid page id: %d", page_id);
        return false;
    }

    if (!BeginTxTransaction()) {
        return false;
    }

    if (page_id == current_page_id_) {
        ESP_LOGI(TAG, "Page already active: %d", page_id);
        if (has_last_air_quality_data_) {
            SendCommandLocked("ref_stop");
            RefreshCurrentPageLocked(false);
            SendCommandLocked("ref_star");
        }
        EndTxTransaction();
        return true;
    }

    const TickType_t now = xTaskGetTickCount();
    if (last_page_switch_ticks_ != 0 &&
        now - last_page_switch_ticks_ < pdMS_TO_TICKS(kPageSwitchDebounceMs)) {
        ESP_LOGW(TAG, "Ignore rapid page switch: %d -> %d", current_page_id_, page_id);
        EndTxTransaction();
        return false;
    }

    char command[24] = {};
    std::snprintf(command, sizeof(command), "page %d", page_id);
    const bool page_changed = SendCommandLocked(command);
    if (page_changed) {
        current_page_id_ = page_id;
        last_page_switch_ticks_ = xTaskGetTickCount();
        vTaskDelay(pdMS_TO_TICKS(80));
        if (has_last_air_quality_data_) {
            SendCommandLocked("ref_stop");
            RefreshCurrentPageLocked(true);
            SendCommandLocked("ref_star");
        }
    }
    EndTxTransaction();
    return page_changed;
}

bool SerialHmi::ShowNamedPage(const char* page_name) {
    const int page_id = PageIdFromName(page_name);
    if (page_id < 0) {
        ESP_LOGW(TAG, "Ignore unknown page name: %s", page_name != nullptr ? page_name : "(null)");
        return false;
    }
    return ShowPage(page_id);
}

bool SerialHmi::ShowNextPage() {
    const int next_page_id = current_page_id_ >= kLastPageId ? kFirstPageId : current_page_id_ + 1;
    return ShowPage(next_page_id);
}

bool SerialHmi::ShowPreviousPage() {
    const int previous_page_id = current_page_id_ <= kFirstPageId ? kLastPageId : current_page_id_ - 1;
    return ShowPage(previous_page_id);
}

void SerialHmi::UpdateAirQuality(const SerialHmiAirQualityData& data) {
    if (!BeginBatchRefresh()) {
        ESP_LOGW(TAG, "Skip HMI data refresh because UART transaction could not start");
        return;
    }

    // 缓存最新传感器数据，页面切换后可以立即刷新当前页控件。
    last_air_quality_data_ = data;
    has_last_air_quality_data_ = true;
    if (data.has_mq135_raw || data.manual_environment_mode || data.air_score > 0) {
        RecordAirCurveScore(data.air_score);
        air_curve_point_pending_ = true;
    }
    RefreshCurrentPageLocked(false);
    EndBatchRefresh();
}

void SerialHmi::RefreshCurrentPage(bool page_entered) {
    if (!has_last_air_quality_data_) {
        return;
    }

    if (!BeginBatchRefresh()) {
        return;
    }
    RefreshCurrentPageLocked(page_entered);
    EndBatchRefresh();
}

void SerialHmi::RefreshCurrentPageLocked(bool page_entered) {
    // 按当前页面定向刷新，避免给不存在于当前页的控件持续发送命令。
    switch (current_page_id_) {
        case kHomePageId:
            RefreshHomePage(last_air_quality_data_);
            break;
        case kAirDetailPageId:
            RefreshAirDetailPage(last_air_quality_data_, page_entered);
            break;
        case kAiSettingsPageId:
            RefreshAiSettingsPage(last_air_quality_data_);
            break;
        default:
            break;
    }

}

bool SerialHmi::BeginBatchRefresh() {
    if (!BeginTxTransaction()) {
        return false;
    }
    SendCommandLocked("ref_stop");
    return true;
}

void SerialHmi::EndBatchRefresh() {
    SendCommandLocked("ref_star");
    EndTxTransaction();
}

void SerialHmi::RefreshHomePage(const SerialHmiAirQualityData& data) {
    char text[64] = {};
    const int score = ClampScore(data.air_score);

    SetText("t_ai_state", data.ai_state != nullptr ? data.ai_state : "IDLE");

    if (data.has_temperature) {
        std::snprintf(text, sizeof(text), "%.1f C", data.temperature_c);
    } else {
        std::snprintf(text, sizeof(text), "-- C");
    }
    SetText("t_temp", text);

    if (data.has_humidity) {
        std::snprintf(text, sizeof(text), "%.1f %%", data.humidity_percent);
    } else {
        std::snprintf(text, sizeof(text), "-- %%");
    }
    SetText("t_humi", text);

    SetText("t_air_state", data.air_state != nullptr ? data.air_state : "UNKNOWN");

    if (data.has_mq135_raw) {
        std::snprintf(text, sizeof(text), "%s(%d)",
                      data.air_state != nullptr ? data.air_state : "UNKNOWN",
                      data.mq135_raw);
    } else {
        std::snprintf(text, sizeof(text), "MQ:--");
    }
    SetText("t_air", text);

    SetValue("j_air", score);
    SetText("t_advice", data.advice != nullptr ? data.advice : "Waiting for sensor data");
}

void SerialHmi::RefreshAirDetailPage(const SerialHmiAirQualityData& data, bool page_entered) {
    char text[64] = {};
    const int score = ClampScore(data.air_score);

    std::snprintf(text, sizeof(text), "%d/100", score);
    SetText("t_air_score", text);
    SetValue("j_air_detail", score);
    SetText("t_air_state", data.air_state != nullptr ? data.air_state : "UNKNOWN");

    if (data.has_mq135_raw) {
        std::snprintf(text, sizeof(text), "%d", data.mq135_raw);
    } else {
        std::snprintf(text, sizeof(text), "--");
    }
    SetText("t_air_raw", text);

    if (data.has_temperature) {
        std::snprintf(text, sizeof(text), "%.1f C", data.temperature_c);
    } else {
        std::snprintf(text, sizeof(text), "-- C");
    }
    SetText("t_temp_d", text);

    if (data.has_humidity) {
        std::snprintf(text, sizeof(text), "%.1f %%", data.humidity_percent);
    } else {
        std::snprintf(text, sizeof(text), "-- %%");
    }
    SetText("t_humi_d", text);

    SetText("t_comfort", data.comfort != nullptr ? data.comfort : "待计算");
    if (page_entered) {
        ReplayAirCurveHistory();
        air_curve_point_pending_ = false;
    } else if (air_curve_point_pending_ && air_curve_count_ > 0) {
        const size_t newest_index =
            (air_curve_write_index_ + kAirCurveHistorySize - 1) % kAirCurveHistorySize;
        AddCurvePoint(kAirCurveId, kAirCurveChannel, air_curve_scores_[newest_index]);
        air_curve_point_pending_ = false;
    }
}

void SerialHmi::RefreshAiSettingsPage(const SerialHmiAirQualityData& data) {
    SetText("t_ai_state", data.ai_state != nullptr ? data.ai_state : "IDLE");
    SetText("t_link_state", "UART2 9600");
}

void SerialHmi::RecordAirCurveScore(int score) {
    air_curve_scores_[air_curve_write_index_] = ClampScore(score);
    air_curve_write_index_ = (air_curve_write_index_ + 1) % kAirCurveHistorySize;
    if (air_curve_count_ < kAirCurveHistorySize) {
        ++air_curve_count_;
    }
}

bool SerialHmi::ClearCurve(int curve_id, int channel) {
    char command[32] = {};
    std::snprintf(command, sizeof(command), "cle %d,%d", curve_id, channel);
    return SendCommand(command);
}

bool SerialHmi::AddCurvePoint(int curve_id, int channel, int value) {
    char command[32] = {};
    std::snprintf(command, sizeof(command), "add %d,%d,%d", curve_id, channel, ClampScore(value));
    return SendCommand(command);
}

void SerialHmi::ReplayAirCurveHistory() {
    if (current_page_id_ == kAirDetailPageId) {
        ClearCurve(kAirCurveId, kAirCurveChannel);
        const size_t oldest_index = (air_curve_write_index_ + kAirCurveHistorySize - air_curve_count_) % kAirCurveHistorySize;
        for (size_t i = 0; i < air_curve_count_; ++i) {
            const size_t index = (oldest_index + i) % kAirCurveHistorySize;
            AddCurvePoint(kAirCurveId, kAirCurveChannel, air_curve_scores_[index]);
        }
    }
}

bool SerialHmi::PollEvent(SerialHmiEvent& event, TickType_t timeout_ticks) {
    if (!initialized_) {
        return false;
    }

    uint8_t bytes[64] = {};
    const int read_len = uart_read_bytes(uart_port_, bytes, sizeof(bytes), timeout_ticks);
    if (read_len <= 0) {
        return false;
    }

    for (int i = 0; i < read_len; ++i) {
        const uint8_t ch = bytes[i];
        if (ch == '\r') {
            continue;
        }

        if (ch == '\n' || ch == 0xFF) {
            if (event_buffer_len_ == 0) {
                continue;
            }
            event_buffer_[event_buffer_len_] = '\0';
            const bool parsed = ParseEventLine(event_buffer_, event);
            ResetEventBuffer();
            if (parsed) {
                return true;
            }
            continue;
        }

        if (std::isprint(static_cast<unsigned char>(ch)) &&
            event_buffer_len_ + 1 < sizeof(event_buffer_)) {
            event_buffer_[event_buffer_len_++] = static_cast<char>(ch);
        }
    }

    return false;
}

int SerialHmi::EstimateAirScoreFromMq135Raw(int raw_value) {
    if (raw_value < 0) {
        return 0;
    }
    if (raw_value < 500) {
        return 90;
    }
    if (raw_value < 1000) {
        return 75;
    }
    if (raw_value < 2000) {
        return 45;
    }
    return 20;
}

const char* SerialHmi::AdviceFromMq135Raw(int raw_value) {
    if (raw_value < 0) {
        return "Check air sensor";
    }
    if (raw_value < 500) {
        return "Air is good";
    }
    if (raw_value < 1000) {
        return "Keep ventilation";
    }
    if (raw_value < 2000) {
        return "Open window or purifier";
    }
    return "Ventilate now";
}

bool SerialHmi::ParseEventLine(const char* line, SerialHmiEvent& event) {
    if (line == nullptr || line[0] == '\0') {
        return false;
    }

    event = SerialHmiEvent{};
    CopyToken(event.raw, sizeof(event.raw), line);

    if (StartsWith(line, "BTN,PAGE,")) {
        event.type = SerialHmiEventType::kPageSelect;
        CopyToken(event.target, sizeof(event.target), line + std::strlen("BTN,PAGE,"));
        return true;
    }

    if (StartsWith(line, "BTN,DEVICE,")) {
        event.type = SerialHmiEventType::kDeviceAction;
        const char* payload = line + std::strlen("BTN,DEVICE,");
        const char* comma = std::strchr(payload, ',');
        if (comma == nullptr) {
            CopyToken(event.target, sizeof(event.target), payload);
        } else {
            char target[32] = {};
            const size_t target_len = std::min(static_cast<size_t>(comma - payload), sizeof(target) - 1);
            std::memcpy(target, payload, target_len);
            CopyToken(event.target, sizeof(event.target), target);
            CopyToken(event.action, sizeof(event.action), comma + 1);
        }
        return true;
    }

    if (StartsWith(line, "BTN,MODE,")) {
        event.type = SerialHmiEventType::kModeAction;
        const char* payload = line + std::strlen("BTN,MODE,");
        const char* comma = std::strchr(payload, ',');
        if (comma == nullptr) {
            CopyToken(event.target, sizeof(event.target), payload);
        } else {
            char target[32] = {};
            const size_t target_len = std::min(static_cast<size_t>(comma - payload), sizeof(target) - 1);
            std::memcpy(target, payload, target_len);
            CopyToken(event.target, sizeof(event.target), target);
            CopyToken(event.action, sizeof(event.action), comma + 1);
        }
        return true;
    }

    if (StartsWith(line, "BTN,ENV,")) {
        event.type = SerialHmiEventType::kEnvironmentAction;
        const char* payload = line + std::strlen("BTN,ENV,");
        const char* comma = std::strchr(payload, ',');
        if (comma == nullptr) {
            CopyToken(event.target, sizeof(event.target), payload);
        } else {
            char target[32] = {};
            const size_t target_len = std::min(static_cast<size_t>(comma - payload), sizeof(target) - 1);
            std::memcpy(target, payload, target_len);
            CopyToken(event.target, sizeof(event.target), target);
            CopyToken(event.action, sizeof(event.action), comma + 1);
        }
        return true;
    }

    if (StartsWith(line, "SWIPE,")) {
        event.type = SerialHmiEventType::kSwipeAction;
        CopyToken(event.target, sizeof(event.target), line + std::strlen("SWIPE,"));
        return true;
    }

    ESP_LOGW(TAG, "Unknown screen event: %s", line);
    return false;
}

void SerialHmi::ResetEventBuffer() {
    event_buffer_len_ = 0;
    event_buffer_[0] = '\0';
}

int SerialHmi::ClampScore(int score) {
    return std::max(0, std::min(100, score));
}

void SerialHmi::CopyToken(char* destination, size_t destination_size, const char* source) {
    if (destination == nullptr || destination_size == 0) {
        return;
    }
    destination[0] = '\0';
    if (source == nullptr) {
        return;
    }
    std::snprintf(destination, destination_size, "%s", source);
}

void SerialHmi::EscapeText(const char* input, char* output, size_t output_size) {
    if (output == nullptr || output_size == 0) {
        return;
    }
    output[0] = '\0';
    if (input == nullptr) {
        return;
    }

    size_t out = 0;
    for (size_t i = 0; input[i] != '\0' && out + 1 < output_size; ++i) {
        const char ch = input[i];
        if (ch == '"' || ch == '\\') {
            output[out++] = ' ';
        } else if (static_cast<unsigned char>(ch) < 0x20) {
            output[out++] = ' ';
        } else {
            output[out++] = ch;
        }
    }
    output[out] = '\0';
}
