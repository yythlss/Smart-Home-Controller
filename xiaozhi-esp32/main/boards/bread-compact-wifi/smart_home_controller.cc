#include "smart_home_controller.h"
#include "serial_hmi.h"
#include "settings.h"

#include <esp_app_desc.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <string>
#include <utility>

#define TAG "SmartHome"

namespace {
constexpr int kAirCurveId = 12;

const char* ResetReasonText(esp_reset_reason_t reason) {
    switch (reason) {
        case ESP_RST_POWERON: return "power_on";
        case ESP_RST_SW: return "software";
        case ESP_RST_PANIC: return "panic";
        case ESP_RST_INT_WDT: return "interrupt_watchdog";
        case ESP_RST_TASK_WDT: return "task_watchdog";
        case ESP_RST_WDT: return "watchdog";
        case ESP_RST_DEEPSLEEP: return "deep_sleep";
        case ESP_RST_BROWNOUT: return "brownout";
        default: return "other";
    }
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

const char* ComfortDescription(const EnvironmentSample& sample) {
    if (!sample.has_temperature || !sample.has_humidity) {
        return "待计算";
    }
    if (sample.has_mq135_raw && sample.air_score < 40) {
        return "空气较差";
    }
    if (sample.temperature_c >= 22.0f && sample.temperature_c <= 28.0f &&
        sample.humidity_percent >= 40.0f && sample.humidity_percent <= 65.0f &&
        sample.air_score >= 80) {
        return "舒适";
    }
    if (sample.temperature_c < 18.0f) {
        return "偏冷";
    }
    if (sample.temperature_c > 30.0f) {
        return "偏热";
    }
    if (sample.humidity_percent < 35.0f) {
        return "偏干";
    }
    if (sample.humidity_percent > 75.0f) {
        return "偏湿";
    }
    if (sample.air_score < 65) {
        return "空气一般";
    }
    return "基本舒适";
}

const char* AdviceForEnvironment(const EnvironmentSample& sample) {
    if (!sample.has_temperature && !sample.has_humidity && !sample.has_mq135_raw) {
        return "等待传感器";
    }
    if (sample.has_mq135_raw && sample.air_score < 40) {
        return "开净化器和新风";
    }
    if (sample.has_temperature && sample.temperature_c > 30.0f) {
        return "开空调降温";
    }
    if (sample.has_temperature && sample.temperature_c < 18.0f) {
        return "开空调升温";
    }
    if (sample.has_humidity && sample.humidity_percent < 35.0f) {
        return "开加湿器";
    }
    if (sample.has_humidity && sample.humidity_percent > 75.0f) {
        return "开新风除湿";
    }
    if (sample.air_score < 65) {
        return "保持通风";
    }
    return "环境舒适";
}
} // namespace

SmartHomeController::StateGuard::StateGuard(const SmartHomeController& owner) : owner_(owner) {
    owner_.LockState();
}

SmartHomeController::StateGuard::~StateGuard() {
    owner_.UnlockState();
}

SmartHomeController::SmartHomeController(SerialHmi* serial_hmi) : serial_hmi_(serial_hmi) {
    state_mutex_ = xSemaphoreCreateRecursiveMutex();
    if (state_mutex_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create controller state mutex");
    }
}

SmartHomeController::~SmartHomeController() {
    servo_task_running_ = false;
    if (servo_task_handle_ == nullptr && state_mutex_ != nullptr) {
        vSemaphoreDelete(state_mutex_);
        state_mutex_ = nullptr;
    }
}

void SmartHomeController::Initialize() {
    StateGuard guard(*this);
    if (initialized_) {
        return;
    }

    ConfigureLedc();
    RegisterMcpTools();
    manual_sample_ = DefaultManualSample();
    LoadPersistentSettings();
    ApplyAll();
    servo_task_running_ = true;
    if (xTaskCreate(ServoTaskEntry, "fresh_air_servo", 3072, this, 4, &servo_task_handle_) != pdPASS) {
        servo_task_handle_ = nullptr;
        servo_task_running_ = false;
        ESP_LOGE(TAG, "Failed to create fresh_air_servo task");
    }

    initialized_ = true;
    RecordEvent("system", "firmware", "智能家居控制器已启动");
    ESP_LOGI(TAG, "Smart home controller initialized");
}

void SmartHomeController::LockState() const {
    if (state_mutex_ != nullptr) {
        xSemaphoreTakeRecursive(state_mutex_, portMAX_DELAY);
    }
}

void SmartHomeController::UnlockState() const {
    if (state_mutex_ != nullptr) {
        xSemaphoreGiveRecursive(state_mutex_);
    }
}

uint64_t SmartHomeController::NowMs() const {
    return static_cast<uint64_t>(esp_timer_get_time() / 1000);
}

void SmartHomeController::LoadPersistentSettings() {
    Settings settings("smart_home", false);
    state_.auto_mode = settings.GetBool("auto_mode", false);
    state_.eco_mode = settings.GetBool("eco_mode", false);
    if (state_.auto_mode && state_.eco_mode) {
        state_.eco_mode = false;
    }
    automation_rule_.enabled = settings.GetBool("rule_on", false);
    automation_rule_.air_score_below = std::max(0, std::min(100, static_cast<int>(settings.GetInt("rule_air", 60))));
    automation_rule_.humidity_below = std::max(0, std::min(100, static_cast<int>(settings.GetInt("rule_hum", 35))));
    automation_rule_.temperature_above = std::max(-10, std::min(60, static_cast<int>(settings.GetInt("rule_temp", 30))));
    automation_rule_.purifier_level = ClampLevel(static_cast<int>(settings.GetInt("rule_pur", 3)));
    automation_rule_.fresh_air_level = ClampLevel(static_cast<int>(settings.GetInt("rule_fresh", 2)));
    automation_rule_.humidifier_level = ClampLevel(static_cast<int>(settings.GetInt("rule_humid", 2)));
}

void SmartHomeController::PersistModes() const {
    Settings settings("smart_home", true);
    settings.SetBool("auto_mode", state_.auto_mode);
    settings.SetBool("eco_mode", state_.eco_mode);
}

void SmartHomeController::PersistAutomationRule() const {
    Settings settings("smart_home", true);
    settings.SetBool("rule_on", automation_rule_.enabled);
    settings.SetInt("rule_air", automation_rule_.air_score_below);
    settings.SetInt("rule_hum", automation_rule_.humidity_below);
    settings.SetInt("rule_temp", automation_rule_.temperature_above);
    settings.SetInt("rule_pur", automation_rule_.purifier_level);
    settings.SetInt("rule_fresh", automation_rule_.fresh_air_level);
    settings.SetInt("rule_humid", automation_rule_.humidifier_level);
}

void SmartHomeController::RecordEvent(const char* type, const char* source, const char* message) {
    SmartHomeEvent& event = events_[event_write_index_];
    event = {};
    event.timestamp_ms = NowMs();
    std::snprintf(event.type, sizeof(event.type), "%s", type != nullptr ? type : "info");
    std::snprintf(event.source, sizeof(event.source), "%s", source != nullptr ? source : "system");
    std::snprintf(event.message, sizeof(event.message), "%s", message != nullptr ? message : "");
    event_write_index_ = (event_write_index_ + 1) % kEventHistorySize;
    if (event_count_ < kEventHistorySize) {
        ++event_count_;
    }
}

void SmartHomeController::ConfigureLedc() {
    ledc_timer_config_t led_timer = {};
    led_timer.speed_mode = SMART_HOME_LEDC_SPEED_MODE;
    led_timer.timer_num = SMART_HOME_LED_TIMER;
    led_timer.duty_resolution = LEDC_TIMER_13_BIT;
    led_timer.freq_hz = SMART_HOME_LED_PWM_HZ;
    led_timer.clk_cfg = LEDC_AUTO_CLK;
    ESP_ERROR_CHECK(ledc_timer_config(&led_timer));

    ledc_timer_config_t servo_timer = {};
    servo_timer.speed_mode = SMART_HOME_LEDC_SPEED_MODE;
    servo_timer.timer_num = SMART_HOME_SERVO_TIMER;
    servo_timer.duty_resolution = LEDC_TIMER_13_BIT;
    servo_timer.freq_hz = SMART_HOME_SERVO_PWM_HZ;
    servo_timer.clk_cfg = LEDC_AUTO_CLK;
    ESP_ERROR_CHECK(ledc_timer_config(&servo_timer));

    ledc_channel_config_t purifier = {};
    purifier.gpio_num = SMART_HOME_PURIFIER_LED_GPIO;
    purifier.speed_mode = SMART_HOME_LEDC_SPEED_MODE;
    purifier.channel = SMART_HOME_PURIFIER_LED_CHANNEL;
    purifier.timer_sel = SMART_HOME_LED_TIMER;
    purifier.duty = 0;
    ESP_ERROR_CHECK(ledc_channel_config(&purifier));

    ledc_channel_config_t humidifier = purifier;
    humidifier.gpio_num = SMART_HOME_HUMIDIFIER_LED_GPIO;
    humidifier.channel = SMART_HOME_HUMIDIFIER_CHANNEL;
    ESP_ERROR_CHECK(ledc_channel_config(&humidifier));

    ledc_channel_config_t servo = {};
    servo.gpio_num = SMART_HOME_FRESH_AIR_SERVO_GPIO;
    servo.speed_mode = SMART_HOME_LEDC_SPEED_MODE;
    servo.channel = SMART_HOME_SERVO_CHANNEL;
    servo.timer_sel = SMART_HOME_SERVO_TIMER;
    servo.duty = 0;
    ESP_ERROR_CHECK(ledc_channel_config(&servo));
}

int SmartHomeController::ClampLevel(int level) const {
    return std::max(0, std::min(3, level));
}

int SmartHomeController::NormalizeLevel(bool power, int level) const {
    if (!power) {
        return 0;
    }
    level = ClampLevel(level);
    return level == 0 ? 1 : level;
}

int SmartHomeController::NextToggleLevel(int level) const {
    return (ClampLevel(level) + 1) % 4;
}

const char* SmartHomeController::LevelText(int level) const {
    switch (ClampLevel(level)) {
        case 1:
            return "low";
        case 2:
            return "medium";
        case 3:
            return "high";
        default:
            return "off";
    }
}

const char* SmartHomeController::RadarZoneText(RadarZone zone) const {
    switch (zone) {
        case RadarZone::None: return "none";
        case RadarZone::Left: return "left";
        case RadarZone::Center: return "center";
        case RadarZone::Right: return "right";
        default: return "unknown";
    }
}

uint64_t SmartHomeController::RemainingOverrideMs(uint64_t deadline_ms) const {
    const uint64_t now = NowMs();
    return deadline_ms > now ? deadline_ms - now : 0;
}

bool SmartHomeController::OverrideActive(uint64_t deadline_ms) const {
    return RemainingOverrideMs(deadline_ms) > 0;
}

void SmartHomeController::ClearManualOverrides() {
    purifier_override_until_ms_ = 0;
    fresh_air_override_until_ms_ = 0;
    humidifier_override_until_ms_ = 0;
    light_override_until_ms_ = 0;
}

int SmartHomeController::ClampScore(int score) const {
    return std::max(0, std::min(100, score));
}

int SmartHomeController::EstimateMq135RawFromScore(int score) const {
    score = ClampScore(score);
    if (score >= 85) {
        return 350;
    }
    if (score >= 65) {
        return 800;
    }
    if (score >= 40) {
        return 1500;
    }
    return 2400;
}

EnvironmentSample SmartHomeController::DefaultManualSample() const {
    EnvironmentSample sample = {};
    sample.has_temperature = true;
    sample.temperature_c = 26.0f;
    sample.has_humidity = true;
    sample.humidity_percent = 55.0f;
    sample.has_mq135_raw = true;
    sample.air_score = 88;
    sample.mq135_raw = EstimateMq135RawFromScore(sample.air_score);
    return BuildDecoratedSample(sample, "manual");
}

EnvironmentSample SmartHomeController::BuildDecoratedSample(EnvironmentSample sample, const char* source) const {
    sample.air_score = ClampScore(sample.air_score);
    sample.manual_environment_mode = state_.manual_environment_mode;
    sample.environment_source = source != nullptr ? source : "sensor";
    sample.comfort = ComfortDescription(sample);
    sample.advice = AdviceForEnvironment(sample);
    return sample;
}

void SmartHomeController::SetLedDuty(ledc_channel_t channel, int percent) {
    percent = std::max(0, std::min(100, percent));
    const uint32_t max_duty = (1u << 13) - 1u;
    const uint32_t duty = (max_duty * static_cast<uint32_t>(percent)) / 100u;
    esp_err_t err = ledc_set_duty(SMART_HOME_LEDC_SPEED_MODE, channel, duty);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ledc_set_duty failed: channel=%d duty=%lu err=%s",
                 static_cast<int>(channel), static_cast<unsigned long>(duty), esp_err_to_name(err));
        return;
    }
    err = ledc_update_duty(SMART_HOME_LEDC_SPEED_MODE, channel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ledc_update_duty failed: channel=%d duty=%lu err=%s",
                 static_cast<int>(channel), static_cast<unsigned long>(duty), esp_err_to_name(err));
    }
}

void SmartHomeController::ApplyPurifier() {
    static const int kDutyByLevel[4] = {0, 33, 66, 100};
    const int level = ClampLevel(state_.purifier_level);
    const int duty_percent = kDutyByLevel[level];
    ESP_LOGI(TAG, "Apply purifier output: gpio=%d level=%d duty=%d%%",
             static_cast<int>(SMART_HOME_PURIFIER_LED_GPIO), level, duty_percent);
    SetLedDuty(SMART_HOME_PURIFIER_LED_CHANNEL, duty_percent);
}

void SmartHomeController::ApplyHumidifier() {
    static const int kDutyByLevel[4] = {0, 33, 66, 100};
    const int level = ClampLevel(state_.humidifier_level);
    const int duty_percent = kDutyByLevel[level];
    ESP_LOGI(TAG, "Apply humidifier output: gpio=%d level=%d duty=%d%%",
             static_cast<int>(SMART_HOME_HUMIDIFIER_LED_GPIO), level, duty_percent);
    SetLedDuty(SMART_HOME_HUMIDIFIER_CHANNEL, duty_percent);
}

void SmartHomeController::SetServoAngle(int angle) {
    angle = std::max(0, std::min(180, angle));
    const int min_us = 500;
    const int max_us = 2500;
    const int period_us = 20000;
    const int pulse_us = min_us + ((max_us - min_us) * angle) / 180;
    const uint32_t max_duty = (1u << 13) - 1u;
    const uint32_t duty = (max_duty * static_cast<uint32_t>(pulse_us)) / period_us;
    esp_err_t err = ledc_set_duty(SMART_HOME_LEDC_SPEED_MODE, SMART_HOME_SERVO_CHANNEL, duty);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "servo ledc_set_duty failed: gpio=%d angle=%d pulse=%dus duty=%lu err=%s",
                 static_cast<int>(SMART_HOME_FRESH_AIR_SERVO_GPIO), angle, pulse_us,
                 static_cast<unsigned long>(duty), esp_err_to_name(err));
        return;
    }
    err = ledc_update_duty(SMART_HOME_LEDC_SPEED_MODE, SMART_HOME_SERVO_CHANNEL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "servo ledc_update_duty failed: gpio=%d angle=%d pulse=%dus duty=%lu err=%s",
                 static_cast<int>(SMART_HOME_FRESH_AIR_SERVO_GPIO), angle, pulse_us,
                 static_cast<unsigned long>(duty), esp_err_to_name(err));
    }
}

void SmartHomeController::SetServoProfileForLevel(int level) {
    switch (ClampLevel(level)) {
        case 1:
            servo_target_min_angle_ = 20;
            servo_target_max_angle_ = 70;
            servo_step_degrees_ = 2;
            servo_step_delay_ms_ = 90;
            break;
        case 2:
            servo_target_min_angle_ = 10;
            servo_target_max_angle_ = 120;
            servo_step_degrees_ = 3;
            servo_step_delay_ms_ = 60;
            break;
        case 3:
            servo_target_min_angle_ = 0;
            servo_target_max_angle_ = 180;
            servo_step_degrees_ = 4;
            servo_step_delay_ms_ = 40;
            break;
        default:
            servo_target_min_angle_ = 0;
            servo_target_max_angle_ = 0;
            servo_step_degrees_ = 2;
            servo_step_delay_ms_ = 90;
            SetServoAngle(0);
            break;
    }
}

void SmartHomeController::ApplyFreshAir() {
    const int level = ClampLevel(state_.fresh_air_level);
    SetServoProfileForLevel(level);
    ESP_LOGI(TAG, "Apply fresh air fan output: gpio=%d level=%d angle=%d..%d step=%d delay=%dms",
             static_cast<int>(SMART_HOME_FRESH_AIR_SERVO_GPIO), level,
             servo_target_min_angle_, servo_target_max_angle_,
             servo_step_degrees_, servo_step_delay_ms_);
}

void SmartHomeController::ApplyLight() {
    ESP_LOGI(TAG, "Apply light output: on=%d", state_.light_on ? 1 : 0);
    if (light_output_callback_) {
        light_output_callback_(state_.light_on);
    }
}

void SmartHomeController::ApplyAll() {
    ApplyPurifier();
    ApplyHumidifier();
    ApplyFreshAir();
    ApplyLight();
}

void SmartHomeController::SetPurifier(bool power, int level) {
    StateGuard guard(*this);
    const int previous_level = state_.purifier_level;
    active_scene_ = "custom";
    state_.purifier_level = NormalizeLevel(power, level);
    purifier_override_until_ms_ = NowMs() + kManualOverrideDurationMs;
    ApplyPurifier();
    if (previous_level != state_.purifier_level) {
        char message[64] = {};
        std::snprintf(message, sizeof(message), "净化器切换到%d档", state_.purifier_level);
        RecordEvent("device", "manual", message);
    }
}

void SmartHomeController::SetFreshAir(bool power, int level) {
    StateGuard guard(*this);
    const int previous_level = state_.fresh_air_level;
    active_scene_ = "custom";
    state_.fresh_air_level = NormalizeLevel(power, level);
    fresh_air_override_until_ms_ = NowMs() + kManualOverrideDurationMs;
    ApplyFreshAir();
    if (previous_level != state_.fresh_air_level) {
        char message[64] = {};
        std::snprintf(message, sizeof(message), "新风切换到%d档", state_.fresh_air_level);
        RecordEvent("device", "manual", message);
    }
}

void SmartHomeController::SetHumidifier(bool power, int level) {
    StateGuard guard(*this);
    const int previous_level = state_.humidifier_level;
    active_scene_ = "custom";
    state_.humidifier_level = NormalizeLevel(power, level);
    humidifier_override_until_ms_ = NowMs() + kManualOverrideDurationMs;
    ApplyHumidifier();
    if (previous_level != state_.humidifier_level) {
        char message[64] = {};
        std::snprintf(message, sizeof(message), "加湿器切换到%d档", state_.humidifier_level);
        RecordEvent("device", "manual", message);
    }
}

void SmartHomeController::SetAutoMode(bool enabled) {
    StateGuard guard(*this);
    const bool changed = state_.auto_mode != enabled;
    active_scene_ = "custom";
    state_.auto_mode = enabled;
    if (enabled) {
        state_.eco_mode = false;
        ClearManualOverrides();
        EvaluateAutoMode(last_sample_);
    }
    PersistModes();
    if (changed) {
        RecordEvent("mode", "manual", enabled ? "自动模式已开启" : "自动模式已关闭");
    }
}

void SmartHomeController::SetEcoMode(bool enabled) {
    StateGuard guard(*this);
    const bool changed = state_.eco_mode != enabled;
    active_scene_ = "custom";
    state_.eco_mode = enabled;
    if (enabled) {
        state_.auto_mode = false;
        ClearManualOverrides();
        EvaluateEcoMode(last_sample_);
    }
    PersistModes();
    if (changed) {
        RecordEvent("mode", "manual", enabled ? "节能模式已开启" : "节能模式已关闭");
    }
}

void SmartHomeController::SetLight(bool power) {
    StateGuard guard(*this);
    const bool changed = state_.light_on != power;
    active_scene_ = "custom";
    state_.light_on = power;
    light_override_until_ms_ = NowMs() + kManualOverrideDurationMs;
    ApplyLight();
    if (changed) {
        RecordEvent("device", "manual", power ? "灯光已开启" : "灯光已关闭");
    }
}

void SmartHomeController::SetPresenceDetectedCallback(PresenceDetectedCallback callback) {
    StateGuard guard(*this);
    presence_detected_callback_ = std::move(callback);
}

void SmartHomeController::SetLightOutputCallback(BinaryOutputCallback callback) {
    StateGuard guard(*this);
    light_output_callback_ = std::move(callback);
}

void SmartHomeController::SetAlarmOutputCallback(AlarmOutputCallback callback) {
    StateGuard guard(*this);
    alarm_output_callback_ = std::move(callback);
}

void SmartHomeController::UpdatePresence(bool occupied) {
    StateGuard guard(*this);
    const bool presence_changed = !state_.occupancy_known || state_.occupied != occupied;
    const bool should_wake = occupied && (!state_.occupancy_known || !state_.occupied);
    state_.occupancy_known = true;
    state_.occupied = occupied;
    ESP_LOGI(TAG, "Presence updated: occupied=%d", occupied ? 1 : 0);
    if (presence_changed) {
        RecordEvent("presence", "radar", occupied ? "检测到人员进入" : "持续无人，已判定离开");
    }

    if (!occupied) {
        ShutdownForNoOccupancy();
        return;
    }

    if (should_wake && presence_detected_callback_) {
        presence_detected_callback_();
    }
    if (state_.eco_mode) {
        EvaluateEcoMode(last_sample_);
    } else if (state_.auto_mode) {
        EvaluateAutoMode(last_sample_);
    } else {
        EvaluateLighting();
    }
}

void SmartHomeController::UpdateRadarObservation(int target_count) {
    UpdateRadarObservation(target_count, false, 0, 0, 0);
}

void SmartHomeController::UpdateRadarObservation(int target_count, bool has_position,
                                                 int nearest_x_mm, int nearest_y_mm,
                                                 int nearest_speed_mm_per_s) {
    StateGuard guard(*this);
    const int clamped_target_count = std::max(0, std::min(3, target_count));
    const bool target_count_changed = !state_.has_radar_data ||
                                      state_.radar_target_count != clamped_target_count;
    state_.has_radar_data = true;
    state_.radar_target_count = clamped_target_count;
    state_.has_radar_position = clamped_target_count > 0 && has_position;
    state_.radar_nearest_x_mm = state_.has_radar_position ? nearest_x_mm : 0;
    state_.radar_nearest_y_mm = state_.has_radar_position ? nearest_y_mm : 0;
    state_.radar_nearest_speed_mm_per_s = state_.has_radar_position ? nearest_speed_mm_per_s : 0;
    if (clamped_target_count == 0) {
        state_.radar_zone = RadarZone::None;
    } else if (!state_.has_radar_position) {
        state_.radar_zone = RadarZone::Unknown;
    } else if (nearest_x_mm <= -500) {
        state_.radar_zone = RadarZone::Left;
    } else if (nearest_x_mm >= 500) {
        state_.radar_zone = RadarZone::Right;
    } else {
        state_.radar_zone = RadarZone::Center;
    }
    if (target_count_changed) {
        ESP_LOGI(TAG, "Radar observation: targets=%d", state_.radar_target_count);
    }

    if (state_.radar_target_count > 0) {
        radar_clear_since_ms_ = 0;
        if (!state_.occupancy_known || !state_.occupied) {
            UpdatePresence(true);
        }
    } else {
        const uint64_t now = NowMs();
        if (radar_clear_since_ms_ == 0) {
            radar_clear_since_ms_ = now;
        } else if (state_.occupied && now - radar_clear_since_ms_ >= kRadarVacancyTimeoutMs) {
            UpdatePresence(false);
        }
    }
}

void SmartHomeController::UpdateAmbientLight(float ambient_light_percent) {
    StateGuard guard(*this);
    state_.has_ambient_light = true;
    state_.ambient_light_percent = std::max(0.0f, std::min(100.0f, ambient_light_percent));
    ESP_LOGI(TAG, "Ambient light updated: %.1f%%", state_.ambient_light_percent);
    EvaluateLighting();
}

void SmartHomeController::UpdateSensorHealth(bool dht_ok, bool mq135_ok, bool ambient_light_ok) {
    StateGuard guard(*this);
    const uint64_t now = NowMs();
    health_.dht_current_ok = dht_ok;
    health_.mq135_current_ok = mq135_ok;
    health_.ambient_light_current_ok = ambient_light_ok;
    health_.dht_consecutive_failures = dht_ok ? 0 : health_.dht_consecutive_failures + 1;
    health_.mq135_consecutive_failures = mq135_ok ? 0 : health_.mq135_consecutive_failures + 1;
    health_.ambient_light_consecutive_failures = ambient_light_ok ? 0 : health_.ambient_light_consecutive_failures + 1;
    if (dht_ok) health_.dht_last_success_ms = now;
    if (mq135_ok) health_.mq135_last_success_ms = now;
    if (ambient_light_ok) health_.ambient_light_last_success_ms = now;
}

void SmartHomeController::UpdateRadarHealth(uint32_t received_bytes, uint32_t valid_frames,
                                            uint32_t rejected_frames, bool received_frame) {
    StateGuard guard(*this);
    health_.radar_received_bytes = received_bytes;
    health_.radar_valid_frames = valid_frames;
    health_.radar_rejected_frames = rejected_frames;
    if (received_frame) {
        health_.radar_last_frame_ms = NowMs();
    }
}

void SmartHomeController::UpdateNetworkHealth(bool connected) {
    StateGuard guard(*this);
    health_.network_connected = connected;
}

void SmartHomeController::UpdateHmiHealth(bool initialized) {
    StateGuard guard(*this);
    health_.hmi_initialized = initialized;
}

void SmartHomeController::AcknowledgeAlarm() {
    StateGuard guard(*this);
    state_.alarm_active = false;
    alarm_reason_.clear();
    ESP_LOGI(TAG, "Environment alarm acknowledged");
    if (alarm_output_callback_) {
        alarm_output_callback_(false, "");
    }
    RecordEvent("alarm", "manual", "当前告警已确认");
}

void SmartHomeController::SetManualEnvironmentMode(bool enabled) {
    StateGuard guard(*this);
    state_.manual_environment_mode = enabled;
    if (enabled) {
        manual_sample_ = BuildDecoratedSample(manual_sample_, "manual");
        ApplyEnvironmentSample(manual_sample_);
    } else {
        last_sample_.manual_environment_mode = false;
        last_sample_.environment_source = "sensor";
    }
    ESP_LOGI(TAG, "Manual environment mode: %s", enabled ? "enabled" : "disabled");
    RecordEvent("environment", "manual", enabled ? "已启用手动环境数据" : "已恢复真实传感器数据");
}

void SmartHomeController::SetManualEnvironment(float temperature_c, float humidity_percent,
                                               int air_score, int mq135_raw) {
    StateGuard guard(*this);
    EnvironmentSample sample = {};
    sample.has_temperature = true;
    sample.temperature_c = std::max(-10.0f, std::min(60.0f, temperature_c));
    sample.has_humidity = true;
    sample.humidity_percent = std::max(0.0f, std::min(100.0f, humidity_percent));
    sample.has_mq135_raw = true;
    sample.air_score = ClampScore(air_score);
    sample.mq135_raw = mq135_raw >= 0 ? std::max(0, std::min(4095, mq135_raw))
                                      : EstimateMq135RawFromScore(sample.air_score);
    state_.manual_environment_mode = true;
    manual_sample_ = BuildDecoratedSample(sample, "manual");
    ApplyEnvironmentSample(manual_sample_);
    RecordEvent("environment", "manual", "手动环境数据已更新");
}

bool SmartHomeController::SetEnvironmentPreset(const char* preset) {
    StateGuard guard(*this);
    if (TokenEquals(preset, "GOOD") || TokenEquals(preset, "COMFORT")) {
        SetManualEnvironment(26.0f, 55.0f, 88);
        return true;
    }
    if (TokenEquals(preset, "HOT")) {
        SetManualEnvironment(33.0f, 58.0f, 72);
        return true;
    }
    if (TokenEquals(preset, "DRY")) {
        SetManualEnvironment(25.0f, 28.0f, 76);
        return true;
    }
    if (TokenEquals(preset, "WET") || TokenEquals(preset, "HUMID")) {
        SetManualEnvironment(26.0f, 82.0f, 70);
        return true;
    }
    if (TokenEquals(preset, "POLLUTED") || TokenEquals(preset, "BAD_AIR")) {
        SetManualEnvironment(27.0f, 60.0f, 28, 2400);
        return true;
    }
    return false;
}

void SmartHomeController::SetAutomationRule(const AutomationRuleConfig& config) {
    StateGuard guard(*this);
    automation_rule_.enabled = config.enabled;
    automation_rule_.air_score_below = std::max(0, std::min(100, config.air_score_below));
    automation_rule_.humidity_below = std::max(0, std::min(100, config.humidity_below));
    automation_rule_.temperature_above = std::max(-10, std::min(60, config.temperature_above));
    automation_rule_.purifier_level = ClampLevel(config.purifier_level);
    automation_rule_.fresh_air_level = ClampLevel(config.fresh_air_level);
    automation_rule_.humidifier_level = ClampLevel(config.humidifier_level);
    automation_rule_active_ = false;
    PersistAutomationRule();
    RecordEvent("automation", "manual",
        automation_rule_.enabled ? "自定义自动化规则已启用" : "自定义自动化规则已停用");
    if (state_.auto_mode) {
        EvaluateAutoMode(last_sample_);
    }
}

bool SmartHomeController::ApplyScene(const char* scene) {
    StateGuard guard(*this);
    if (scene == nullptr) {
        return false;
    }

    const uint64_t override_until = NowMs() + kManualOverrideDurationMs;
    ClearManualOverrides();
    if (TokenEquals(scene, "HOME")) {
        active_scene_ = "home";
        state_.occupancy_known = true;
        state_.occupied = true;
        state_.auto_mode = true;
        state_.eco_mode = false;
        state_.purifier_level = 1;
        state_.fresh_air_level = 1;
        state_.humidifier_level = 0;
        EvaluateLighting();
        RecordEvent("scene", "manual", "已切换到回家场景");
    } else if (TokenEquals(scene, "AWAY")) {
        active_scene_ = "away";
        state_.occupancy_known = true;
        state_.occupied = false;
        state_.auto_mode = false;
        state_.eco_mode = true;
        state_.purifier_level = 0;
        state_.fresh_air_level = 0;
        state_.humidifier_level = 0;
        state_.light_on = false;
        RecordEvent("scene", "manual", "已切换到离家场景");
    } else if (TokenEquals(scene, "SLEEP")) {
        active_scene_ = "sleep";
        state_.occupancy_known = true;
        state_.occupied = true;
        state_.auto_mode = false;
        state_.eco_mode = true;
        state_.purifier_level = 1;
        state_.fresh_air_level = 0;
        state_.humidifier_level = 1;
        state_.light_on = false;
        purifier_override_until_ms_ = override_until;
        humidifier_override_until_ms_ = override_until;
        light_override_until_ms_ = override_until;
        RecordEvent("scene", "manual", "已切换到睡眠场景");
    } else if (TokenEquals(scene, "VENTILATE")) {
        active_scene_ = "ventilate";
        state_.auto_mode = false;
        state_.eco_mode = false;
        state_.purifier_level = 1;
        state_.fresh_air_level = 3;
        state_.humidifier_level = 0;
        purifier_override_until_ms_ = override_until;
        fresh_air_override_until_ms_ = override_until;
        humidifier_override_until_ms_ = override_until;
        RecordEvent("scene", "manual", "已切换到通风场景");
    } else if (TokenEquals(scene, "CLEAN")) {
        active_scene_ = "clean";
        state_.auto_mode = false;
        state_.eco_mode = false;
        state_.purifier_level = 3;
        state_.fresh_air_level = 2;
        state_.humidifier_level = 0;
        purifier_override_until_ms_ = override_until;
        fresh_air_override_until_ms_ = override_until;
        humidifier_override_until_ms_ = override_until;
        RecordEvent("scene", "manual", "已切换到强力净化场景");
    } else {
        return false;
    }

    PersistModes();
    ApplyAll();
    return true;
}

SmartHomeState SmartHomeController::GetState() const {
    StateGuard guard(*this);
    return state_;
}

SmartHomeHealth SmartHomeController::GetHealth() const {
    StateGuard guard(*this);
    return health_;
}

EnvironmentSample SmartHomeController::GetLastSample() const {
    StateGuard guard(*this);
    return last_sample_;
}

bool SmartHomeController::HandleDeviceAction(const char* target, const char* action) {
    StateGuard guard(*this);
    if (!TokenEquals(action, "TOGGLE")) {
        return false;
    }
    if (TokenEquals(target, "AIR_PURIFIER")) {
        const int next_level = NextToggleLevel(state_.purifier_level);
        SetPurifier(next_level != 0, next_level);
        ESP_LOGI(TAG, "Device action applied: target=AIR_PURIFIER level=%d", state_.purifier_level);
        return true;
    }
    if (TokenEquals(target, "FAN") || TokenEquals(target, "FRESH_AIR")) {
        const int next_level = NextToggleLevel(state_.fresh_air_level);
        SetFreshAir(next_level != 0, next_level);
        ESP_LOGI(TAG, "Device action applied: target=FAN level=%d", state_.fresh_air_level);
        return true;
    }
    if (TokenEquals(target, "HUMIDIFIER")) {
        const int next_level = NextToggleLevel(state_.humidifier_level);
        SetHumidifier(next_level != 0, next_level);
        ESP_LOGI(TAG, "Device action applied: target=HUMIDIFIER level=%d", state_.humidifier_level);
        return true;
    }
    return false;
}

bool SmartHomeController::HandleModeAction(const char* target, const char* action) {
    StateGuard guard(*this);
    if (!TokenEquals(action, "TOGGLE")) {
        return false;
    }
    if (TokenEquals(target, "AUTO")) {
        SetAutoMode(!state_.auto_mode);
        return true;
    }
    if (TokenEquals(target, "ECO")) {
        SetEcoMode(!state_.eco_mode);
        return true;
    }
    return false;
}

bool SmartHomeController::HandleEnvironmentAction(const char* target, const char* action) {
    StateGuard guard(*this);
    if (TokenEquals(target, "MANUAL") && TokenEquals(action, "TOGGLE")) {
        SetManualEnvironmentMode(!state_.manual_environment_mode);
        return true;
    }
    if (TokenEquals(target, "SCENE")) {
        return SetEnvironmentPreset(action);
    }
    return false;
}

void SmartHomeController::UpdateEnvironment(const EnvironmentSample& sample) {
    StateGuard guard(*this);
    if (state_.manual_environment_mode) {
        ApplyEnvironmentSample(manual_sample_);
        return;
    }
    ApplyEnvironmentSample(BuildDecoratedSample(sample, "sensor"));
}

void SmartHomeController::EvaluateAutoMode(const EnvironmentSample& sample) {
    if (state_.occupancy_known && !state_.occupied) {
        ShutdownForNoOccupancy();
        return;
    }

    if (sample.has_humidity && !OverrideActive(humidifier_override_until_ms_)) {
        if (sample.humidity_percent < 40.0f) {
            state_.humidifier_level = 2;
        } else if (sample.humidity_percent > 70.0f) {
            state_.humidifier_level = 0;
        }
    }

    if (sample.has_mq135_raw && !OverrideActive(purifier_override_until_ms_)) {
        if (sample.mq135_raw >= 2000) {
            state_.purifier_level = 3;
        } else if (sample.mq135_raw >= 1000) {
            state_.purifier_level = std::max(state_.purifier_level, 2);
        } else {
            state_.purifier_level = 0;
        }
    }

    if (!OverrideActive(fresh_air_override_until_ms_)) {
        if (sample.has_mq135_raw && sample.mq135_raw >= 2000) {
            state_.fresh_air_level = std::max(state_.fresh_air_level, 2);
        }
        if (sample.has_temperature && sample.temperature_c > 30.0f) {
            state_.fresh_air_level = std::max(state_.fresh_air_level, 2);
        } else if (sample.has_mq135_raw && sample.mq135_raw < 1000) {
            state_.fresh_air_level = 0;
        }
    }

    EvaluateAutomationRule(sample);
    EvaluateLighting();
    ApplyPurifier();
    ApplyHumidifier();
    ApplyFreshAir();
}

void SmartHomeController::EvaluateAutomationRule(const EnvironmentSample& sample) {
    if (!automation_rule_.enabled) {
        automation_rule_active_ = false;
        return;
    }

    bool triggered = false;
    if (sample.has_mq135_raw && sample.air_score < automation_rule_.air_score_below) {
        triggered = true;
        if (!OverrideActive(purifier_override_until_ms_)) {
            state_.purifier_level = std::max(state_.purifier_level, automation_rule_.purifier_level);
        }
        if (!OverrideActive(fresh_air_override_until_ms_)) {
            state_.fresh_air_level = std::max(state_.fresh_air_level, automation_rule_.fresh_air_level);
        }
    }
    if (sample.has_humidity && sample.humidity_percent < automation_rule_.humidity_below) {
        triggered = true;
        if (!OverrideActive(humidifier_override_until_ms_)) {
            state_.humidifier_level = std::max(state_.humidifier_level, automation_rule_.humidifier_level);
        }
    }
    if (sample.has_temperature && sample.temperature_c > automation_rule_.temperature_above) {
        triggered = true;
        if (!OverrideActive(fresh_air_override_until_ms_)) {
            state_.fresh_air_level = std::max(state_.fresh_air_level, automation_rule_.fresh_air_level);
        }
    }

    if (triggered != automation_rule_active_) {
        automation_rule_active_ = triggered;
        RecordEvent("automation", "system",
            triggered ? "环境达到阈值，自动化规则已执行" : "环境恢复，自动化规则已解除");
    }
}

void SmartHomeController::EvaluateEcoMode(const EnvironmentSample& sample) {
    if (state_.occupancy_known && !state_.occupied) {
        ShutdownForNoOccupancy();
        return;
    }

    int purifier_level = 0;
    int fresh_air_level = 0;
    int humidifier_level = 0;

    if (sample.has_mq135_raw) {
        if (sample.mq135_raw >= 2000 || sample.air_score < 40) {
            purifier_level = 2;
            fresh_air_level = 1;
        } else if (sample.mq135_raw >= 1000 || sample.air_score < 65) {
            purifier_level = 1;
        }
    }

    if (sample.has_humidity && sample.humidity_percent < 35.0f) {
        humidifier_level = 1;
    }
    if (sample.has_temperature && sample.temperature_c > 30.0f) {
        fresh_air_level = std::max(fresh_air_level, 1);
    }

    if (!OverrideActive(purifier_override_until_ms_)) {
        state_.purifier_level = purifier_level;
    }
    if (!OverrideActive(fresh_air_override_until_ms_)) {
        state_.fresh_air_level = fresh_air_level;
    }
    if (!OverrideActive(humidifier_override_until_ms_)) {
        state_.humidifier_level = humidifier_level;
    }
    EvaluateLighting();
    ApplyPurifier();
    ApplyHumidifier();
    ApplyFreshAir();
}

void SmartHomeController::EvaluateLighting() {
    if (OverrideActive(light_override_until_ms_)) {
        return;
    }
    if (state_.occupancy_known && !state_.occupied) {
        if (state_.light_on) {
            state_.light_on = false;
            ApplyLight();
        }
        return;
    }
    if (!state_.occupancy_known || !state_.has_ambient_light) {
        return;
    }

    bool next_light_on = state_.light_on;
    if (!state_.light_on && state_.occupied &&
        state_.ambient_light_percent <= kDarkThresholdPercent) {
        next_light_on = true;
    } else if (state_.light_on && state_.ambient_light_percent >= kLightOffThresholdPercent) {
        next_light_on = false;
    }
    if (state_.light_on != next_light_on) {
        state_.light_on = next_light_on;
        ApplyLight();
    }
}

void SmartHomeController::ShutdownForNoOccupancy() {
    ClearManualOverrides();
    state_.purifier_level = 0;
    state_.fresh_air_level = 0;
    state_.humidifier_level = 0;
    state_.light_on = false;
    ESP_LOGI(TAG, "No occupancy: all controllable devices are off");
    ApplyAll();
}

void SmartHomeController::SetAlarm(const char* reason) {
    if (reason == nullptr || reason[0] == '\0') {
        return;
    }
    const uint64_t now = NowMs();
    if (state_.alarm_active ||
        (last_alarm_ms_ != 0 && now - last_alarm_ms_ < kAlarmCooldownMs)) {
        return;
    }
    state_.alarm_active = true;
    last_alarm_ms_ = now;
    alarm_reason_ = reason;
    RecordEvent("alarm", "system", alarm_reason_.c_str());
    ESP_LOGW(TAG, "Environment alarm: %s", alarm_reason_.c_str());
    if (alarm_output_callback_) {
        alarm_output_callback_(true, alarm_reason_.c_str());
    }
}

void SmartHomeController::EvaluateEnvironmentAlarm(const EnvironmentSample& previous,
                                                   const EnvironmentSample& current) {
    char reason[128] = {};
    const char* candidate_type = nullptr;
    const EnvironmentSample& baseline = alarm_candidate_count_ > 0
        ? alarm_candidate_baseline_ : previous;
    if (baseline.has_temperature && current.has_temperature &&
        std::fabs(current.temperature_c - baseline.temperature_c) >= kTemperatureJumpThresholdC) {
        candidate_type = "temperature";
        std::snprintf(reason, sizeof(reason), "temperature changed %.1fC to %.1fC",
                      baseline.temperature_c, current.temperature_c);
    } else if (baseline.has_humidity && current.has_humidity &&
               std::fabs(current.humidity_percent - baseline.humidity_percent) >=
                   kHumidityJumpThresholdPercent) {
        candidate_type = "humidity";
        std::snprintf(reason, sizeof(reason), "humidity changed %.1f%% to %.1f%%",
                      baseline.humidity_percent, current.humidity_percent);
    } else if (baseline.has_mq135_raw && current.has_mq135_raw &&
               std::abs(current.air_score - baseline.air_score) >= kAirScoreJumpThreshold) {
        candidate_type = "air_score";
        std::snprintf(reason, sizeof(reason), "air score changed %d to %d",
                      baseline.air_score, current.air_score);
    } else if (baseline.has_mq135_raw && current.has_mq135_raw &&
               std::abs(current.mq135_raw - baseline.mq135_raw) >= kMq135JumpThreshold) {
        candidate_type = "mq135";
        std::snprintf(reason, sizeof(reason), "MQ135 changed %d to %d",
                      baseline.mq135_raw, current.mq135_raw);
    }

    if (candidate_type == nullptr) {
        alarm_candidate_type_.clear();
        alarm_candidate_count_ = 0;
        return;
    }
    if (alarm_candidate_type_ != candidate_type) {
        alarm_candidate_type_ = candidate_type;
        alarm_candidate_baseline_ = previous;
        alarm_candidate_count_ = 1;
        return;
    }
    ++alarm_candidate_count_;
    if (alarm_candidate_count_ >= kAlarmConfirmationSamples) {
        SetAlarm(reason);
        alarm_candidate_type_.clear();
        alarm_candidate_count_ = 0;
    }
}

void SmartHomeController::ApplyEnvironmentSample(const EnvironmentSample& sample) {
    EnvironmentSample decorated = BuildDecoratedSample(
        sample, state_.manual_environment_mode ? "manual" : sample.environment_source);
    if (decorated.sample_time_ms == 0) {
        decorated.sample_time_ms = NowMs();
    }
    if (has_alarm_baseline_) {
        EvaluateEnvironmentAlarm(last_sample_, decorated);
    }
    last_sample_ = decorated;
    has_alarm_baseline_ = true;
    RecordEnvironmentSample(last_sample_);
    if (state_.eco_mode) {
        EvaluateEcoMode(last_sample_);
    } else if (state_.auto_mode) {
        EvaluateAutoMode(last_sample_);
    }
}

void SmartHomeController::RecordEnvironmentSample(const EnvironmentSample& sample) {
    history_[history_write_index_] = sample;
    history_write_index_ = (history_write_index_ + 1) % kHistorySize;
    if (history_count_ < kHistorySize) {
        ++history_count_;
    }
    MaybeSendCurvePoint(kAirCurveId, 0, sample.air_score);
    if (sample.has_temperature) {
        MaybeSendCurvePoint(kCurveIdUnavailable, 0, static_cast<int>(sample.temperature_c));
    }
    if (sample.has_humidity) {
        MaybeSendCurvePoint(kCurveIdUnavailable, 0, static_cast<int>(sample.humidity_percent));
    }
}

void SmartHomeController::MaybeSendCurvePoint(int curve_id, int channel, int value) {
    if (curve_id < 0 || serial_hmi_ == nullptr) {
        return;
    }
    value = std::max(0, std::min(100, value));
    char command[32] = {};
    std::snprintf(command, sizeof(command), "add %d,%d,%d", curve_id, channel, value);
    serial_hmi_->SendCommand(command);
}

cJSON* SmartHomeController::BuildStateJson() const {
    StateGuard guard(*this);
    cJSON* json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "purifier_level", state_.purifier_level);
    cJSON_AddStringToObject(json, "purifier", LevelText(state_.purifier_level));
    cJSON_AddNumberToObject(json, "fresh_air_level", state_.fresh_air_level);
    cJSON_AddStringToObject(json, "fresh_air", LevelText(state_.fresh_air_level));
    cJSON_AddNumberToObject(json, "humidifier_level", state_.humidifier_level);
    cJSON_AddStringToObject(json, "humidifier", LevelText(state_.humidifier_level));
    cJSON_AddBoolToObject(json, "auto_mode", state_.auto_mode);
    cJSON_AddBoolToObject(json, "eco_mode", state_.eco_mode);
    cJSON_AddStringToObject(json, "active_scene", active_scene_.c_str());
    cJSON_AddBoolToObject(json, "manual_environment_mode", state_.manual_environment_mode);
    cJSON_AddBoolToObject(json, "occupancy_known", state_.occupancy_known);
    cJSON_AddBoolToObject(json, "occupied", state_.occupied);
    cJSON_AddBoolToObject(json, "has_ambient_light", state_.has_ambient_light);
    cJSON_AddNumberToObject(json, "ambient_light_percent", state_.ambient_light_percent);
    cJSON_AddBoolToObject(json, "light_on", state_.light_on);
    cJSON_AddBoolToObject(json, "has_radar_data", state_.has_radar_data);
    cJSON_AddNumberToObject(json, "radar_target_count", state_.radar_target_count);
    cJSON_AddBoolToObject(json, "has_radar_position", state_.has_radar_position);
    cJSON_AddNumberToObject(json, "radar_nearest_x_mm", state_.radar_nearest_x_mm);
    cJSON_AddNumberToObject(json, "radar_nearest_y_mm", state_.radar_nearest_y_mm);
    cJSON_AddNumberToObject(json, "radar_nearest_speed_mm_per_s", state_.radar_nearest_speed_mm_per_s);
    cJSON_AddStringToObject(json, "radar_zone", RadarZoneText(state_.radar_zone));
    cJSON_AddBoolToObject(json, "alarm_active", state_.alarm_active);
    cJSON_AddStringToObject(json, "alarm_reason", alarm_reason_.c_str());
    cJSON_AddStringToObject(json, "environment_source", last_sample_.environment_source);
    cJSON_AddNumberToObject(json, "sample_time_ms", static_cast<double>(last_sample_.sample_time_ms));
    cJSON_AddBoolToObject(json, "cached_temperature_humidity", last_sample_.cached_temperature_humidity);
    cJSON_AddBoolToObject(json, "has_temperature", last_sample_.has_temperature);
    cJSON_AddNumberToObject(json, "temperature_c", last_sample_.temperature_c);
    cJSON_AddBoolToObject(json, "has_humidity", last_sample_.has_humidity);
    cJSON_AddNumberToObject(json, "humidity_percent", last_sample_.humidity_percent);
    cJSON_AddBoolToObject(json, "has_mq135_raw", last_sample_.has_mq135_raw);
    cJSON_AddNumberToObject(json, "mq135_raw", last_sample_.mq135_raw);
    cJSON_AddNumberToObject(json, "air_score", last_sample_.air_score);
    cJSON_AddStringToObject(json, "air_state", AirStateFromScore(last_sample_.air_score));
    cJSON_AddStringToObject(json, "comfort", last_sample_.comfort);
    cJSON_AddStringToObject(json, "advice", last_sample_.advice);
    cJSON_AddNumberToObject(json, "purifier_override_remaining_seconds",
        static_cast<double>(RemainingOverrideMs(purifier_override_until_ms_) / 1000));
    cJSON_AddNumberToObject(json, "fresh_air_override_remaining_seconds",
        static_cast<double>(RemainingOverrideMs(fresh_air_override_until_ms_) / 1000));
    cJSON_AddNumberToObject(json, "humidifier_override_remaining_seconds",
        static_cast<double>(RemainingOverrideMs(humidifier_override_until_ms_) / 1000));
    cJSON_AddNumberToObject(json, "light_override_remaining_seconds",
        static_cast<double>(RemainingOverrideMs(light_override_until_ms_) / 1000));
    cJSON* automation = cJSON_CreateObject();
    cJSON_AddBoolToObject(automation, "enabled", automation_rule_.enabled);
    cJSON_AddBoolToObject(automation, "active", automation_rule_active_);
    cJSON_AddNumberToObject(automation, "air_score_below", automation_rule_.air_score_below);
    cJSON_AddNumberToObject(automation, "humidity_below", automation_rule_.humidity_below);
    cJSON_AddNumberToObject(automation, "temperature_above", automation_rule_.temperature_above);
    cJSON_AddNumberToObject(automation, "purifier_level", automation_rule_.purifier_level);
    cJSON_AddNumberToObject(automation, "fresh_air_level", automation_rule_.fresh_air_level);
    cJSON_AddNumberToObject(automation, "humidifier_level", automation_rule_.humidifier_level);
    cJSON_AddItemToObject(json, "automation_rule", automation);
    cJSON_AddItemToObject(json, "health", BuildHealthJson());
    return json;
}

cJSON* SmartHomeController::BuildHistoryJson() const {
    StateGuard guard(*this);
    cJSON* json = cJSON_CreateObject();
    cJSON* samples = cJSON_CreateArray();
    const size_t oldest_index = (history_write_index_ + kHistorySize - history_count_) % kHistorySize;

    for (size_t i = 0; i < history_count_; ++i) {
        const size_t index = (oldest_index + i) % kHistorySize;
        const EnvironmentSample& sample = history_[index];
        cJSON* item = cJSON_CreateObject();
        cJSON_AddNumberToObject(item, "sample_time_ms", static_cast<double>(sample.sample_time_ms));
        cJSON_AddBoolToObject(item, "cached_temperature_humidity", sample.cached_temperature_humidity);
        cJSON_AddBoolToObject(item, "has_temperature", sample.has_temperature);
        cJSON_AddNumberToObject(item, "temperature_c", sample.temperature_c);
        cJSON_AddBoolToObject(item, "has_humidity", sample.has_humidity);
        cJSON_AddNumberToObject(item, "humidity_percent", sample.humidity_percent);
        cJSON_AddBoolToObject(item, "has_mq135_raw", sample.has_mq135_raw);
        cJSON_AddNumberToObject(item, "mq135_raw", sample.mq135_raw);
        cJSON_AddNumberToObject(item, "air_score", sample.air_score);
        cJSON_AddStringToObject(item, "environment_source", sample.environment_source);
        cJSON_AddStringToObject(item, "comfort", sample.comfort);
        cJSON_AddStringToObject(item, "advice", sample.advice);
        cJSON_AddItemToArray(samples, item);
    }

    cJSON_AddNumberToObject(json, "count", static_cast<double>(history_count_));
    cJSON_AddNumberToObject(json, "capacity", static_cast<double>(kHistorySize));
    cJSON_AddItemToObject(json, "samples", samples);
    return json;
}

cJSON* SmartHomeController::BuildEventsJson() const {
    StateGuard guard(*this);
    cJSON* json = cJSON_CreateObject();
    cJSON* events = cJSON_CreateArray();
    const size_t oldest_index =
        (event_write_index_ + kEventHistorySize - event_count_) % kEventHistorySize;
    for (size_t i = 0; i < event_count_; ++i) {
        const SmartHomeEvent& event = events_[(oldest_index + i) % kEventHistorySize];
        cJSON* item = cJSON_CreateObject();
        cJSON_AddNumberToObject(item, "timestamp_ms", static_cast<double>(event.timestamp_ms));
        cJSON_AddStringToObject(item, "type", event.type);
        cJSON_AddStringToObject(item, "source", event.source);
        cJSON_AddStringToObject(item, "message", event.message);
        cJSON_AddItemToArray(events, item);
    }
    cJSON_AddNumberToObject(json, "count", static_cast<double>(event_count_));
    cJSON_AddNumberToObject(json, "capacity", static_cast<double>(kEventHistorySize));
    cJSON_AddItemToObject(json, "events", events);
    return json;
}

cJSON* SmartHomeController::BuildHealthJson() const {
    StateGuard guard(*this);
    const uint64_t now = NowMs();
    auto age_ms = [now](uint64_t last_success_ms) -> double {
        return last_success_ms == 0 ? -1.0 : static_cast<double>(now - last_success_ms);
    };

    cJSON* json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "uptime_ms", static_cast<double>(now));
    cJSON_AddNumberToObject(json, "free_heap_bytes", static_cast<double>(esp_get_free_heap_size()));
    cJSON_AddStringToObject(json, "reset_reason", ResetReasonText(esp_reset_reason()));
    const esp_app_desc_t* app = esp_app_get_description();
    cJSON_AddStringToObject(json, "firmware_version", app != nullptr ? app->version : "unknown");
    cJSON_AddBoolToObject(json, "network_connected", health_.network_connected);
    cJSON_AddBoolToObject(json, "hmi_initialized", health_.hmi_initialized);
    cJSON_AddBoolToObject(json, "api_auth_enabled", SMART_HOME_API_TOKEN[0] != '\0');

    wifi_ap_record_t ap = {};
    const bool has_wifi_info = esp_wifi_sta_get_ap_info(&ap) == ESP_OK;
    cJSON_AddBoolToObject(json, "has_wifi_rssi", has_wifi_info);
    cJSON_AddNumberToObject(json, "wifi_rssi_dbm", has_wifi_info ? ap.rssi : 0);

    cJSON_AddBoolToObject(json, "dht_current_ok", health_.dht_current_ok);
    cJSON_AddNumberToObject(json, "dht_age_ms", age_ms(health_.dht_last_success_ms));
    cJSON_AddBoolToObject(json, "dht_stale",
        health_.dht_last_success_ms == 0 || now - health_.dht_last_success_ms > kSensorStaleAfterMs);
    cJSON_AddNumberToObject(json, "dht_consecutive_failures", health_.dht_consecutive_failures);
    cJSON_AddBoolToObject(json, "mq135_current_ok", health_.mq135_current_ok);
    cJSON_AddNumberToObject(json, "mq135_age_ms", age_ms(health_.mq135_last_success_ms));
    cJSON_AddBoolToObject(json, "mq135_stale",
        health_.mq135_last_success_ms == 0 || now - health_.mq135_last_success_ms > kSensorStaleAfterMs);
    cJSON_AddNumberToObject(json, "mq135_consecutive_failures", health_.mq135_consecutive_failures);
    cJSON_AddBoolToObject(json, "ambient_light_current_ok", health_.ambient_light_current_ok);
    cJSON_AddNumberToObject(json, "ambient_light_age_ms", age_ms(health_.ambient_light_last_success_ms));
    cJSON_AddBoolToObject(json, "ambient_light_stale",
        health_.ambient_light_last_success_ms == 0 ||
        now - health_.ambient_light_last_success_ms > kSensorStaleAfterMs);
    cJSON_AddNumberToObject(json, "ambient_light_consecutive_failures",
                            health_.ambient_light_consecutive_failures);
    cJSON_AddNumberToObject(json, "radar_age_ms", age_ms(health_.radar_last_frame_ms));
    cJSON_AddBoolToObject(json, "radar_stale",
        health_.radar_last_frame_ms == 0 || now - health_.radar_last_frame_ms > kSensorStaleAfterMs);
    cJSON_AddNumberToObject(json, "radar_received_bytes", health_.radar_received_bytes);
    cJSON_AddNumberToObject(json, "radar_valid_frames", health_.radar_valid_frames);
    cJSON_AddNumberToObject(json, "radar_rejected_frames", health_.radar_rejected_frames);
    return json;
}

void SmartHomeController::RegisterMcpTools() {
    auto& mcp_server = McpServer::GetInstance();

    mcp_server.AddTool("self.home.get_state",
        "Get smart-home state, sensor/manual environment values, comfort description, and advice.",
        PropertyList(),
        [this](const PropertyList&) -> ReturnValue {
            return BuildStateJson();
        });

    mcp_server.AddTool("self.home.set_purifier",
        "控制空气净化器。User intents: 开净化器、关闭净化器、净化器一档/二档/三档. Level range is 0-3.",
        PropertyList({
            Property("power", kPropertyTypeBoolean),
            Property("level", kPropertyTypeInteger, 0, 3),
        }),
        [this](const PropertyList& properties) -> ReturnValue {
            SetPurifier(properties["power"].value<bool>(), properties["level"].value<int>());
            return true;
        });

    mcp_server.AddTool("self.home.set_fresh_air",
        "控制新风/180度角度舵机扇叶。User intents: 开风扇、关闭风扇、开新风、通风、风扇一档/二档/三档. Level range is 0-3.",
        PropertyList({
            Property("power", kPropertyTypeBoolean),
            Property("level", kPropertyTypeInteger, 0, 3),
        }),
        [this](const PropertyList& properties) -> ReturnValue {
            SetFreshAir(properties["power"].value<bool>(), properties["level"].value<int>());
            return true;
        });

    mcp_server.AddTool("self.home.set_humidifier",
        "控制加湿器。User intents: 开加湿器、关闭加湿器、空气太干. Level range is 0-3.",
        PropertyList({
            Property("power", kPropertyTypeBoolean),
            Property("level", kPropertyTypeInteger, 0, 3),
        }),
        [this](const PropertyList& properties) -> ReturnValue {
            SetHumidifier(properties["power"].value<bool>(), properties["level"].value<int>());
            return true;
        });

    mcp_server.AddTool("self.home.set_auto",
        "开启或关闭自动模式。自动模式会根据温度、湿度、空气评分或手动模拟环境调整净化、新风和加湿。",
        PropertyList({
            Property("power", kPropertyTypeBoolean),
        }),
        [this](const PropertyList& properties) -> ReturnValue {
            SetAutoMode(properties["power"].value<bool>());
            return true;
        });

    mcp_server.AddTool("self.home.set_eco",
        "开启或关闭节能模式。节能模式按环境使用较低档位，无人时关闭全部设备。",
        PropertyList({
            Property("power", kPropertyTypeBoolean),
        }),
        [this](const PropertyList& properties) -> ReturnValue {
            SetEcoMode(properties["power"].value<bool>());
            return true;
        });

    mcp_server.AddTool("self.home.set_light",
        "控制家中照明灯。power=true 开灯，power=false 关灯。",
        PropertyList({
            Property("power", kPropertyTypeBoolean),
        }),
        [this](const PropertyList& properties) -> ReturnValue {
            SetLight(properties["power"].value<bool>());
            return BuildStateJson();
        });

    mcp_server.AddTool("self.home.update_context",
        "更新雷达占用状态和环境亮度，用于硬件接入前联调。occupied 表示有人，ambient_light_percent 为0到100，越小越暗。",
        PropertyList({
            Property("occupied", kPropertyTypeBoolean),
            Property("ambient_light_percent", kPropertyTypeInteger, 100, 0, 100),
        }),
        [this](const PropertyList& properties) -> ReturnValue {
            UpdatePresence(properties["occupied"].value<bool>());
            UpdateAmbientLight(static_cast<float>(properties["ambient_light_percent"].value<int>()));
            return BuildStateJson();
        });

    mcp_server.AddTool("self.home.acknowledge_alarm",
        "确认并清除当前环境突变报警。",
        PropertyList(),
        [this](const PropertyList&) -> ReturnValue {
            AcknowledgeAlarm();
            return BuildStateJson();
        });

    mcp_server.AddTool("self.home.get_environment_briefing",
        "获取适合语音播报的室内环境摘要。",
        PropertyList(),
        [this](const PropertyList&) -> ReturnValue {
            cJSON* json = BuildStateJson();
            const EnvironmentSample sample = GetLastSample();
            char briefing[256] = {};
            std::snprintf(briefing, sizeof(briefing),
                          "temperature %.1f C, humidity %.1f percent, air score %d, comfort %s, advice %s",
                          sample.temperature_c, sample.humidity_percent,
                          sample.air_score, sample.comfort, sample.advice);
            cJSON_AddStringToObject(json, "briefing", briefing);
            return json;
        });

    mcp_server.AddTool("self.home.set_manual_environment",
        "设置手动模拟环境数据，用于无法制造真实高温、干燥或污染环境时测试自动模式。参数为整数，temperature_c 摄氏度，humidity_percent 百分比，air_score 0-100。",
        PropertyList({
            Property("enabled", kPropertyTypeBoolean),
            Property("temperature_c", kPropertyTypeInteger, 26, -10, 60),
            Property("humidity_percent", kPropertyTypeInteger, 55, 0, 100),
            Property("air_score", kPropertyTypeInteger, 88, 0, 100),
        }),
        [this](const PropertyList& properties) -> ReturnValue {
            const bool enabled = properties["enabled"].value<bool>();
            if (!enabled) {
                SetManualEnvironmentMode(false);
                return BuildStateJson();
            }
            SetManualEnvironment(static_cast<float>(properties["temperature_c"].value<int>()),
                                 static_cast<float>(properties["humidity_percent"].value<int>()),
                                 properties["air_score"].value<int>());
            return BuildStateJson();
        });

    mcp_server.AddTool("self.home.set_environment_preset",
        "按场景设置手动模拟环境。preset: GOOD=舒适, HOT=高温建议开空调, DRY=干燥建议开加湿器, POLLUTED=空气差建议开净化器和新风。",
        PropertyList({
            Property("preset", kPropertyTypeString),
        }),
        [this](const PropertyList& properties) -> ReturnValue {
            const auto preset = properties["preset"].value<std::string>();
            if (!SetEnvironmentPreset(preset.c_str())) {
                return false;
            }
            return BuildStateJson();
        });

    mcp_server.AddTool("self.home.get_advice",
        "获取当前环境舒适度和建议，例如开空调、开加湿器、开净化器或保持通风。",
        PropertyList(),
        [this](const PropertyList&) -> ReturnValue {
            StateGuard guard(*this);
            cJSON* json = cJSON_CreateObject();
            cJSON_AddStringToObject(json, "comfort", last_sample_.comfort);
            cJSON_AddStringToObject(json, "advice", last_sample_.advice);
            cJSON_AddStringToObject(json, "environment_source", last_sample_.environment_source);
            cJSON_AddBoolToObject(json, "manual_environment_mode", state_.manual_environment_mode);
            return json;
        });
}

void SmartHomeController::ServoTaskEntry(void* arg) {
    static_cast<SmartHomeController*>(arg)->ServoTaskLoop();
}

void SmartHomeController::ServoTaskLoop() {
    int angle = 0;
    int direction = 1;

    while (true) {
        int level = 0;
        int target_min_angle = 0;
        int target_max_angle = 0;
        int step_degrees = 2;
        int step_delay_ms = 80;
        {
            StateGuard guard(*this);
            if (!servo_task_running_) {
                break;
            }
            level = ClampLevel(state_.fresh_air_level);
            target_min_angle = servo_target_min_angle_;
            target_max_angle = servo_target_max_angle_;
            step_degrees = servo_step_degrees_;
            step_delay_ms = servo_step_delay_ms_;
        }
        if (level == 0) {
            angle = 0;
            direction = 1;
            SetServoAngle(0);
            vTaskDelay(pdMS_TO_TICKS(300));
            continue;
        }

        SetServoAngle(angle);
        angle += direction * step_degrees;
        if (angle >= target_max_angle) {
            angle = target_max_angle;
            direction = -1;
        } else if (angle <= target_min_angle) {
            angle = target_min_angle;
            direction = 1;
        }
        vTaskDelay(pdMS_TO_TICKS(step_delay_ms));
    }

    {
        StateGuard guard(*this);
        servo_task_handle_ = nullptr;
    }
    vTaskDelete(nullptr);
}
