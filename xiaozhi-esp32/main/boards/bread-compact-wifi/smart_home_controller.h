#ifndef SMART_HOME_CONTROLLER_H
#define SMART_HOME_CONTROLLER_H

#include "config.h"
#include "mcp_server.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <cstddef>
#include <cstdint>
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
    uint64_t sample_time_ms = 0;
    bool cached_temperature_humidity = false;
    bool manual_environment_mode = false;
    const char* environment_source = "sensor";
    const char* comfort = "待计算";
    const char* advice = "等待传感器";
};

enum class RadarZone {
    Unknown,
    None,
    Left,
    Center,
    Right,
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
    bool has_radar_position = false;
    int radar_nearest_x_mm = 0;
    int radar_nearest_y_mm = 0;
    int radar_nearest_speed_mm_per_s = 0;
    RadarZone radar_zone = RadarZone::Unknown;
    bool alarm_active = false;
};

struct SmartHomeHealth {
    bool network_connected = false;
    bool hmi_initialized = false;
    bool dht_current_ok = false;
    bool mq135_current_ok = false;
    bool ambient_light_current_ok = false;
    uint64_t dht_last_success_ms = 0;
    uint64_t mq135_last_success_ms = 0;
    uint64_t ambient_light_last_success_ms = 0;
    uint64_t radar_last_frame_ms = 0;
    uint32_t dht_consecutive_failures = 0;
    uint32_t mq135_consecutive_failures = 0;
    uint32_t ambient_light_consecutive_failures = 0;
    uint32_t radar_received_bytes = 0;
    uint32_t radar_valid_frames = 0;
    uint32_t radar_rejected_frames = 0;
};

struct AutomationRuleConfig {
    bool enabled = false;
    int air_score_below = 60;
    int humidity_below = 35;
    int temperature_above = 30;
    int purifier_level = 3;
    int fresh_air_level = 2;
    int humidifier_level = 2;
};

struct SmartHomeEvent {
    uint64_t timestamp_ms = 0;
    char type[24] = {};
    char source[16] = {};
    char message[96] = {};
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
    void UpdateRadarObservation(int target_count, bool has_position,
                                int nearest_x_mm, int nearest_y_mm,
                                int nearest_speed_mm_per_s);
    void UpdateAmbientLight(float ambient_light_percent);
    void UpdateSensorHealth(bool dht_ok, bool mq135_ok, bool ambient_light_ok);
    void UpdateRadarHealth(uint32_t received_bytes, uint32_t valid_frames,
                           uint32_t rejected_frames, bool received_frame);
    void UpdateNetworkHealth(bool connected);
    void UpdateHmiHealth(bool initialized);
    void AcknowledgeAlarm();
    void SetManualEnvironmentMode(bool enabled);
    void SetManualEnvironment(float temperature_c, float humidity_percent, int air_score, int mq135_raw = -1);
    bool SetEnvironmentPreset(const char* preset);
    void SetAutomationRule(const AutomationRuleConfig& config);
    bool ApplyScene(const char* scene);
    void SetPresenceDetectedCallback(PresenceDetectedCallback callback);
    void SetLightOutputCallback(BinaryOutputCallback callback);
    void SetAlarmOutputCallback(AlarmOutputCallback callback);

    SmartHomeState GetState() const;
    SmartHomeHealth GetHealth() const;
    EnvironmentSample GetLastSample() const;
    EnvironmentSample GetLastSensorSample() const;
    cJSON* BuildStateJson() const;
    cJSON* BuildHistoryJson() const;
    cJSON* BuildHealthJson() const;
    cJSON* BuildEventsJson() const;
    cJSON* BuildSummaryJson() const;

private:
    static constexpr size_t kHistorySize = 30;
    static constexpr size_t kEventHistorySize = 32;
    static constexpr float kLightOnThresholdPercent = 25.0f;
    static constexpr float kLightOffThresholdPercent = 35.0f;
    static constexpr float kDarkThresholdPercent = kLightOnThresholdPercent;
    static constexpr float kTemperatureJumpThresholdC = 5.0f;
    static constexpr float kHumidityJumpThresholdPercent = 20.0f;
    static constexpr int kAirScoreJumpThreshold = 30;
    static constexpr int kMq135JumpThreshold = 1200;
    static constexpr int kAlarmConfirmationSamples = 2;
    static constexpr uint64_t kAlarmCooldownMs = 60000;
    static constexpr uint64_t kManualOverrideDurationMs = 30 * 60 * 1000;
    static constexpr uint64_t kRadarVacancyTimeoutMs = 2 * 60 * 1000;
    static constexpr uint64_t kSensorStaleAfterMs = 30 * 1000;

    class StateGuard {
    public:
        explicit StateGuard(const SmartHomeController& owner);
        ~StateGuard();

    private:
        const SmartHomeController& owner_;
    };

    void ConfigureLedc();
    void RegisterMcpTools();
    void ApplyAll();
    void ApplyPurifier();
    void ApplyHumidifier();
    void ApplyFreshAir();
    void ApplyLight();
    void ApplyTargetLevels(int purifier_level, int fresh_air_level, int humidifier_level);
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
    EnvironmentSample SanitizeEnvironmentSample(EnvironmentSample sample) const;
    EnvironmentSample BuildDecoratedSample(EnvironmentSample sample, const char* source) const;
    EnvironmentSample DefaultManualSample() const;
    void RecordEnvironmentSample(const EnvironmentSample& sample);
    int ClampScore(int score) const;
    int EstimateMq135RawFromScore(int score) const;
    int NormalizeLevel(bool power, int level) const;
    int NextToggleLevel(int level) const;
    int ClampLevel(int level) const;
    const char* LevelText(int level) const;
    const char* RadarZoneText(RadarZone zone) const;
    uint64_t NowMs() const;
    uint64_t RemainingOverrideMs(uint64_t deadline_ms) const;
    bool OverrideActive(uint64_t deadline_ms) const;
    void ClearManualOverrides();
    void LoadPersistentSettings();
    void PersistModes() const;
    void PersistAutomationRule() const;
    cJSON* BuildMcpResponse(bool ok, const char* action, const char* message,
                            cJSON* data = nullptr) const;
    void RecordEvent(const char* type, const char* source, const char* message);
    void LockState() const;
    void UnlockState() const;

    static void ServoTaskEntry(void* arg);
    void ServoTaskLoop();

    SerialHmi* serial_hmi_ = nullptr;
    SmartHomeState state_ = {};
    SmartHomeHealth health_ = {};
    EnvironmentSample last_sample_ = {};
    EnvironmentSample last_sensor_sample_ = {};
    bool has_last_sensor_sample_ = false;
    EnvironmentSample manual_sample_ = {};
    EnvironmentSample history_[30] = {};
    size_t history_write_index_ = 0;
    size_t history_count_ = 0;
    AutomationRuleConfig automation_rule_ = {};
    bool automation_rule_active_ = false;
    SmartHomeEvent events_[kEventHistorySize] = {};
    size_t event_write_index_ = 0;
    size_t event_count_ = 0;
    bool initialized_ = false;
    bool has_alarm_baseline_ = false;
    EnvironmentSample alarm_candidate_baseline_ = {};
    std::string alarm_candidate_type_;
    int alarm_candidate_count_ = 0;
    uint64_t last_alarm_ms_ = 0;
    std::string alarm_reason_;
    std::string active_scene_ = "custom";
    PresenceDetectedCallback presence_detected_callback_;
    BinaryOutputCallback light_output_callback_;
    AlarmOutputCallback alarm_output_callback_;
    int servo_target_min_angle_ = 0;
    int servo_target_max_angle_ = 0;
    int servo_step_degrees_ = 2;
    int servo_step_delay_ms_ = 80;
    uint64_t purifier_override_until_ms_ = 0;
    uint64_t fresh_air_override_until_ms_ = 0;
    uint64_t humidifier_override_until_ms_ = 0;
    uint64_t light_override_until_ms_ = 0;
    uint64_t radar_clear_since_ms_ = 0;
    bool servo_task_running_ = false;
    TaskHandle_t servo_task_handle_ = nullptr;
    mutable SemaphoreHandle_t state_mutex_ = nullptr;
};

#endif // SMART_HOME_CONTROLLER_H
