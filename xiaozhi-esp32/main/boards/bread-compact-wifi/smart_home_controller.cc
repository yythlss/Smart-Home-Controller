#include "smart_home_controller.h"
#include "serial_hmi.h"

#include <esp_err.h>
#include <esp_log.h>
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

SmartHomeController::SmartHomeController(SerialHmi* serial_hmi) : serial_hmi_(serial_hmi) {
}

SmartHomeController::~SmartHomeController() {
    servo_task_running_ = false;
}

void SmartHomeController::Initialize() {
    if (initialized_) {
        return;
    }

    ConfigureLedc();
    RegisterMcpTools();
    manual_sample_ = DefaultManualSample();
    ApplyAll();
    servo_task_running_ = true;
    if (xTaskCreate(ServoTaskEntry, "fresh_air_servo", 3072, this, 4, &servo_task_handle_) != pdPASS) {
        servo_task_handle_ = nullptr;
        servo_task_running_ = false;
        ESP_LOGE(TAG, "Failed to create fresh_air_servo task");
    }

    initialized_ = true;
    ESP_LOGI(TAG, "Smart home controller initialized");
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
    if (power) {
        state_.eco_mode = false;
    }
    state_.purifier_level = NormalizeLevel(power, level);
    ApplyPurifier();
}

void SmartHomeController::SetFreshAir(bool power, int level) {
    if (power) {
        state_.eco_mode = false;
    }
    state_.fresh_air_level = NormalizeLevel(power, level);
    ApplyFreshAir();
}

void SmartHomeController::SetHumidifier(bool power, int level) {
    if (power) {
        state_.eco_mode = false;
    }
    state_.humidifier_level = NormalizeLevel(power, level);
    ApplyHumidifier();
}

void SmartHomeController::SetAutoMode(bool enabled) {
    state_.auto_mode = enabled;
    if (enabled) {
        state_.eco_mode = false;
        EvaluateAutoMode(last_sample_);
    }
}

void SmartHomeController::SetEcoMode(bool enabled) {
    state_.eco_mode = enabled;
    if (enabled) {
        state_.auto_mode = false;
        EvaluateEcoMode(last_sample_);
    }
}

void SmartHomeController::SetLight(bool power) {
    state_.light_on = power;
    ApplyLight();
}

void SmartHomeController::SetPresenceDetectedCallback(PresenceDetectedCallback callback) {
    presence_detected_callback_ = std::move(callback);
}

void SmartHomeController::SetLightOutputCallback(BinaryOutputCallback callback) {
    light_output_callback_ = std::move(callback);
}

void SmartHomeController::SetAlarmOutputCallback(AlarmOutputCallback callback) {
    alarm_output_callback_ = std::move(callback);
}

void SmartHomeController::UpdatePresence(bool occupied) {
    const bool should_wake = occupied && (!state_.occupancy_known || !state_.occupied);
    state_.occupancy_known = true;
    state_.occupied = occupied;
    ESP_LOGI(TAG, "Presence updated: occupied=%d", occupied ? 1 : 0);

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
    const int clamped_target_count = std::max(0, std::min(3, target_count));
    const bool target_count_changed = !state_.has_radar_data ||
                                      state_.radar_target_count != clamped_target_count;
    state_.has_radar_data = true;
    state_.radar_target_count = clamped_target_count;
    if (target_count_changed) {
        ESP_LOGI(TAG, "Radar observation: targets=%d", state_.radar_target_count);
    }

    // A live target is safe evidence of occupancy. A clear doorway is not proof that the home is empty.
    if (state_.radar_target_count > 0 && (!state_.occupancy_known || !state_.occupied)) {
        UpdatePresence(true);
    }
}

void SmartHomeController::UpdateAmbientLight(float ambient_light_percent) {
    state_.has_ambient_light = true;
    state_.ambient_light_percent = std::max(0.0f, std::min(100.0f, ambient_light_percent));
    ESP_LOGI(TAG, "Ambient light updated: %.1f%%", state_.ambient_light_percent);
    EvaluateLighting();
}

void SmartHomeController::AcknowledgeAlarm() {
    state_.alarm_active = false;
    alarm_reason_.clear();
    ESP_LOGI(TAG, "Environment alarm acknowledged");
    if (alarm_output_callback_) {
        alarm_output_callback_(false, "");
    }
}

void SmartHomeController::SetManualEnvironmentMode(bool enabled) {
    state_.manual_environment_mode = enabled;
    if (enabled) {
        manual_sample_ = BuildDecoratedSample(manual_sample_, "manual");
        ApplyEnvironmentSample(manual_sample_);
    } else {
        last_sample_.manual_environment_mode = false;
        last_sample_.environment_source = "sensor";
    }
    ESP_LOGI(TAG, "Manual environment mode: %s", enabled ? "enabled" : "disabled");
}

void SmartHomeController::SetManualEnvironment(float temperature_c, float humidity_percent,
                                               int air_score, int mq135_raw) {
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
}

bool SmartHomeController::SetEnvironmentPreset(const char* preset) {
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

SmartHomeState SmartHomeController::GetState() const {
    return state_;
}

EnvironmentSample SmartHomeController::GetLastSample() const {
    return last_sample_;
}

bool SmartHomeController::HandleDeviceAction(const char* target, const char* action) {
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

    if (sample.has_humidity) {
        if (sample.humidity_percent < 40.0f) {
            state_.humidifier_level = 2;
        } else if (sample.humidity_percent > 70.0f) {
            state_.humidifier_level = 0;
        }
    }

    if (sample.has_mq135_raw) {
        if (sample.mq135_raw >= 2000) {
            state_.purifier_level = 3;
            state_.fresh_air_level = std::max(state_.fresh_air_level, 2);
        } else if (sample.mq135_raw >= 1000) {
            state_.purifier_level = std::max(state_.purifier_level, 2);
        } else {
            state_.purifier_level = 0;
        }
    }

    if (sample.has_temperature && sample.temperature_c > 30.0f) {
        state_.fresh_air_level = std::max(state_.fresh_air_level, 2);
    } else if (sample.has_mq135_raw && sample.mq135_raw < 1000) {
        state_.fresh_air_level = 0;
    }

    EvaluateLighting();
    ApplyPurifier();
    ApplyHumidifier();
    ApplyFreshAir();
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

    state_.purifier_level = purifier_level;
    state_.fresh_air_level = fresh_air_level;
    state_.humidifier_level = humidifier_level;
    EvaluateLighting();
    ApplyPurifier();
    ApplyHumidifier();
    ApplyFreshAir();
}

void SmartHomeController::EvaluateLighting() {
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
    state_.alarm_active = true;
    alarm_reason_ = reason;
    ESP_LOGW(TAG, "Environment alarm: %s", alarm_reason_.c_str());
    if (alarm_output_callback_) {
        alarm_output_callback_(true, alarm_reason_.c_str());
    }
}

void SmartHomeController::EvaluateEnvironmentAlarm(const EnvironmentSample& previous,
                                                   const EnvironmentSample& current) {
    char reason[128] = {};
    if (previous.has_temperature && current.has_temperature &&
        std::fabs(current.temperature_c - previous.temperature_c) >= kTemperatureJumpThresholdC) {
        std::snprintf(reason, sizeof(reason), "temperature changed %.1fC to %.1fC",
                      previous.temperature_c, current.temperature_c);
    } else if (previous.has_humidity && current.has_humidity &&
               std::fabs(current.humidity_percent - previous.humidity_percent) >=
                   kHumidityJumpThresholdPercent) {
        std::snprintf(reason, sizeof(reason), "humidity changed %.1f%% to %.1f%%",
                      previous.humidity_percent, current.humidity_percent);
    } else if (std::abs(current.air_score - previous.air_score) >= kAirScoreJumpThreshold) {
        std::snprintf(reason, sizeof(reason), "air score changed %d to %d",
                      previous.air_score, current.air_score);
    } else if (previous.has_mq135_raw && current.has_mq135_raw &&
               std::abs(current.mq135_raw - previous.mq135_raw) >= kMq135JumpThreshold) {
        std::snprintf(reason, sizeof(reason), "MQ135 changed %d to %d",
                      previous.mq135_raw, current.mq135_raw);
    }

    if (reason[0] != '\0') {
        SetAlarm(reason);
    }
}

void SmartHomeController::ApplyEnvironmentSample(const EnvironmentSample& sample) {
    EnvironmentSample decorated = BuildDecoratedSample(
        sample, state_.manual_environment_mode ? "manual" : sample.environment_source);
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
    cJSON* json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "purifier_level", state_.purifier_level);
    cJSON_AddStringToObject(json, "purifier", LevelText(state_.purifier_level));
    cJSON_AddNumberToObject(json, "fresh_air_level", state_.fresh_air_level);
    cJSON_AddStringToObject(json, "fresh_air", LevelText(state_.fresh_air_level));
    cJSON_AddNumberToObject(json, "humidifier_level", state_.humidifier_level);
    cJSON_AddStringToObject(json, "humidifier", LevelText(state_.humidifier_level));
    cJSON_AddBoolToObject(json, "auto_mode", state_.auto_mode);
    cJSON_AddBoolToObject(json, "eco_mode", state_.eco_mode);
    cJSON_AddBoolToObject(json, "manual_environment_mode", state_.manual_environment_mode);
    cJSON_AddBoolToObject(json, "occupancy_known", state_.occupancy_known);
    cJSON_AddBoolToObject(json, "occupied", state_.occupied);
    cJSON_AddBoolToObject(json, "has_ambient_light", state_.has_ambient_light);
    cJSON_AddNumberToObject(json, "ambient_light_percent", state_.ambient_light_percent);
    cJSON_AddBoolToObject(json, "light_on", state_.light_on);
    cJSON_AddBoolToObject(json, "has_radar_data", state_.has_radar_data);
    cJSON_AddNumberToObject(json, "radar_target_count", state_.radar_target_count);
    cJSON_AddBoolToObject(json, "alarm_active", state_.alarm_active);
    cJSON_AddStringToObject(json, "alarm_reason", alarm_reason_.c_str());
    cJSON_AddStringToObject(json, "environment_source", last_sample_.environment_source);
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
    return json;
}

cJSON* SmartHomeController::BuildHistoryJson() const {
    cJSON* json = cJSON_CreateObject();
    cJSON* samples = cJSON_CreateArray();
    const size_t oldest_index = (history_write_index_ + kHistorySize - history_count_) % kHistorySize;

    for (size_t i = 0; i < history_count_; ++i) {
        const size_t index = (oldest_index + i) % kHistorySize;
        const EnvironmentSample& sample = history_[index];
        cJSON* item = cJSON_CreateObject();
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
            char briefing[256] = {};
            std::snprintf(briefing, sizeof(briefing),
                          "temperature %.1f C, humidity %.1f percent, air score %d, comfort %s, advice %s",
                          last_sample_.temperature_c, last_sample_.humidity_percent,
                          last_sample_.air_score, last_sample_.comfort, last_sample_.advice);
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

    while (servo_task_running_) {
        const int level = ClampLevel(state_.fresh_air_level);
        if (level == 0) {
            angle = 0;
            direction = 1;
            SetServoAngle(0);
            vTaskDelay(pdMS_TO_TICKS(300));
            continue;
        }

        SetServoAngle(angle);
        angle += direction * servo_step_degrees_;
        if (angle >= servo_target_max_angle_) {
            angle = servo_target_max_angle_;
            direction = -1;
        } else if (angle <= servo_target_min_angle_) {
            angle = servo_target_min_angle_;
            direction = 1;
        }
        vTaskDelay(pdMS_TO_TICKS(servo_step_delay_ms_));
    }

    servo_task_handle_ = nullptr;
    vTaskDelete(nullptr);
}
