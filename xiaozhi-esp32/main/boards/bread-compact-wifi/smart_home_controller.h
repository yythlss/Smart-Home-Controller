#ifndef SMART_HOME_CONTROLLER_H
#define SMART_HOME_CONTROLLER_H

#include "config.h"
#include "mcp_server.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <cstddef>
#include <functional>
#include <string>

struct EnvironmentSample {
    bool has_temperature = false;
    float temperature_c = 0.0f;
    bool has_humidity = false;
    float humidity_percent = 0.0f;
    bool has_mq135_raw = false;
    int mq135_raw = 0;
    int air_score = 0;
    bool manual_environment_mode = false;
    const char* environment_source = "sensor";
    const char* comfort = "待计算";
    const char* advice = "等待传感器";
};

struct SmartHomeState {
    int purifier_level = 0;
    int fresh_air_level = 0;
    int humidifier_level = 0;
    bool auto_mode = false;
    bool eco_mode = false;
    bool manual_environment_mode = false;
    bool occupancy_known = false;
    bool occupied = false;
    bool has_ambient_light = false;
    float ambient_light_percent = 100.0f;
    bool light_on = false;
    bool has_radar_data = false;
    int radar_target_count = 0;
    bool alarm_active = false;
};

class SerialHmi;

class SmartHomeController {
public:
    using PresenceDetectedCallback = std::function<void()>;
    using BinaryOutputCallback = std::function<void(bool)>;
    using AlarmOutputCallback = std::function<void(bool, const char*)>;

    explicit SmartHomeController(SerialHmi* serial_hmi = nullptr);
    ~SmartHomeController();

    void Initialize();
    bool HandleDeviceAction(const char* target, const char* action);
    bool HandleModeAction(const char* target, const char* action);
    bool HandleEnvironmentAction(const char* target, const char* action);
    void UpdateEnvironment(const EnvironmentSample& sample);

    void SetPurifier(bool power, int level);
    void SetFreshAir(bool power, int level);
    void SetHumidifier(bool power, int level);
    void SetAutoMode(bool enabled);
    void SetEcoMode(bool enabled);
    void SetLight(bool power);
    void UpdatePresence(bool occupied);
    void UpdateRadarObservation(int target_count);
    void UpdateAmbientLight(float ambient_light_percent);
    void AcknowledgeAlarm();
    void SetManualEnvironmentMode(bool enabled);
    void SetManualEnvironment(float temperature_c, float humidity_percent, int air_score, int mq135_raw = -1);
    bool SetEnvironmentPreset(const char* preset);
    void SetPresenceDetectedCallback(PresenceDetectedCallback callback);
    void SetLightOutputCallback(BinaryOutputCallback callback);
    void SetAlarmOutputCallback(AlarmOutputCallback callback);

    SmartHomeState GetState() const;
    EnvironmentSample GetLastSample() const;
    cJSON* BuildStateJson() const;
    cJSON* BuildHistoryJson() const;

private:
    static constexpr size_t kHistorySize = 30;
    static constexpr int kCurveIdUnavailable = -1;
    static constexpr float kLightOnThresholdPercent = 25.0f;
    static constexpr float kLightOffThresholdPercent = 35.0f;
    static constexpr float kDarkThresholdPercent = kLightOnThresholdPercent;
    static constexpr float kTemperatureJumpThresholdC = 5.0f;
    static constexpr float kHumidityJumpThresholdPercent = 20.0f;
    static constexpr int kAirScoreJumpThreshold = 30;
    static constexpr int kMq135JumpThreshold = 1200;

    void ConfigureLedc();
    void RegisterMcpTools();
    void ApplyAll();
    void ApplyPurifier();
    void ApplyHumidifier();
    void ApplyFreshAir();
    void ApplyLight();
    void SetLedDuty(ledc_channel_t channel, int percent);
    void SetServoAngle(int angle);
    void SetServoProfileForLevel(int level);
    void EvaluateAutoMode(const EnvironmentSample& sample);
    void EvaluateEcoMode(const EnvironmentSample& sample);
    void EvaluateLighting();
    void EvaluateEnvironmentAlarm(const EnvironmentSample& previous, const EnvironmentSample& current);
    void ShutdownForNoOccupancy();
    void SetAlarm(const char* reason);
    void ApplyEnvironmentSample(const EnvironmentSample& sample);
    EnvironmentSample BuildDecoratedSample(EnvironmentSample sample, const char* source) const;
    EnvironmentSample DefaultManualSample() const;
    void RecordEnvironmentSample(const EnvironmentSample& sample);
    void MaybeSendCurvePoint(int curve_id, int channel, int value);
    int ClampScore(int score) const;
    int EstimateMq135RawFromScore(int score) const;
    int NormalizeLevel(bool power, int level) const;
    int NextToggleLevel(int level) const;
    int ClampLevel(int level) const;
    const char* LevelText(int level) const;

    static void ServoTaskEntry(void* arg);
    void ServoTaskLoop();

    SerialHmi* serial_hmi_ = nullptr;
    SmartHomeState state_ = {};
    EnvironmentSample last_sample_ = {};
    EnvironmentSample manual_sample_ = {};
    EnvironmentSample history_[30] = {};
    size_t history_write_index_ = 0;
    size_t history_count_ = 0;
    bool initialized_ = false;
    bool has_alarm_baseline_ = false;
    std::string alarm_reason_;
    PresenceDetectedCallback presence_detected_callback_;
    BinaryOutputCallback light_output_callback_;
    AlarmOutputCallback alarm_output_callback_;
    int servo_target_min_angle_ = 0;
    int servo_target_max_angle_ = 0;
    int servo_step_degrees_ = 2;
    int servo_step_delay_ms_ = 80;
    bool servo_task_running_ = false;
    TaskHandle_t servo_task_handle_ = nullptr;
};

#endif // SMART_HOME_CONTROLLER_H
