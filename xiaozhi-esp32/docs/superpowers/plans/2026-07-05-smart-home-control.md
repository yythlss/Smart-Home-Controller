# Smart Home Control Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add smart-home actuator control for purifier, fresh-air servo fan, humidifier, automatic mode, software eco mode, MCP voice tools, and recent environment history for later HMI curves.

**Architecture:** Add a focused `SmartHomeController` owned by `CompactWifiBoard`. Screen events, MCP tools, and automatic sensor decisions all call this controller so device state has one source of truth. Use LEDC PWM for LEDs and SG90 servo output; keep HMI curve writes disabled until numeric curve object IDs are confirmed from the HMI editor.

**Tech Stack:** ESP-IDF C++17, FreeRTOS tasks, LEDC PWM (`driver/ledc.h`), existing `McpServer`, existing TJC serial HMI protocol.

---

## File Structure

Create:

- `main/boards/bread-compact-wifi/smart_home_controller.h`  
  Defines device state, environment input, control methods, MCP registration, and servo task lifecycle.

- `main/boards/bread-compact-wifi/smart_home_controller.cc`  
  Implements LEDC setup, PWM level mapping, SG90 angle PWM mapping, screen-event command handling, automatic mode, software eco mode, and MCP tools.

Modify:

- `main/boards/bread-compact-wifi/config.h`  
  Add smart-home GPIO and LEDC channel/timer constants. Remove or supersede old `TOUCH_BUTTON_GPIO`/`BUILTIN_LED_GPIO` assumptions only inside this board configuration.

- `main/boards/bread-compact-wifi/compact_wifi_board.cc`  
  Own `SmartHomeController`; initialize it; forward screen `BTN,DEVICE` / `BTN,MODE` events; forward sensor readings each 5s.

- `main/boards/bread-compact-wifi/serial_hmi_widgets.json`  
  Add documented HMI event `BTN,MODE,ECO,TOGGLE` and optional curve/status widgets.

- `../文档/串口屏手动事件配置手册.md`  
  Add manual HMI instructions for the eco button and curve numeric IDs.

- `docs/phase-handoff-2026-07-05-smart-home-control.md`  
  New Chinese handoff after implementation.

Test:

- `tests/test_bread_compact_wifi_regressions.py`  
  Add source-level regression tests that enforce GPIO allocation, controller boundaries, MCP tool registration, eco/auto behavior, and curve-ID gating.

---

## Task 1: Add Regression Tests First

**Files:**

- Modify: `tests/test_bread_compact_wifi_regressions.py`
- Read: `docs/superpowers/specs/2026-07-05-smart-home-control-design.md`

- [ ] **Step 1: Add failing tests for controller boundaries and GPIO mapping**

Append these tests to `BreadCompactWifiRegressionTest`:

```python
    def test_smart_home_controller_owns_actuators_and_gpio_mapping(self):
        config = (BOARD_DIR / "config.h").read_text(encoding="utf-8")
        header = (BOARD_DIR / "smart_home_controller.h").read_text(encoding="utf-8")
        source = (BOARD_DIR / "smart_home_controller.cc").read_text(encoding="utf-8")

        self.assertIn("#define SMART_HOME_PURIFIER_LED_GPIO GPIO_NUM_13", config)
        self.assertIn("#define SMART_HOME_HUMIDIFIER_LED_GPIO GPIO_NUM_14", config)
        self.assertIn("#define SMART_HOME_FRESH_AIR_SERVO_GPIO GPIO_NUM_21", config)
        self.assertIn("class SmartHomeController", header)
        self.assertIn("ApplyPurifier", source)
        self.assertIn("ApplyHumidifier", source)
        self.assertIn("SetServoAngle", source)
        self.assertIn("LEDC_TIMER_13_BIT", source)
        self.assertIn("LEDC_TIMER_50_HZ", source)
```

- [ ] **Step 2: Add failing tests for screen event forwarding and MCP tools**

Append:

```python
    def test_smart_home_screen_events_and_mcp_tools_are_registered(self):
        board = (BOARD_DIR / "compact_wifi_board.cc").read_text(encoding="utf-8")
        source = (BOARD_DIR / "smart_home_controller.cc").read_text(encoding="utf-8")

        self.assertIn("SmartHomeController smart_home_", board)
        self.assertIn("smart_home_.HandleDeviceAction", board)
        self.assertIn("smart_home_.HandleModeAction", board)
        self.assertIn("smart_home_.UpdateEnvironment", board)

        for tool in [
            "self.home.get_state",
            "self.home.set_purifier",
            "self.home.set_fresh_air",
            "self.home.set_humidifier",
            "self.home.set_auto",
            "self.home.set_eco",
        ]:
            self.assertIn(tool, source)
```

- [ ] **Step 3: Add failing tests for auto/eco semantics and non-continuous servo**

Append:

```python
    def test_smart_home_auto_eco_and_sg90_reciprocating_servo_rules(self):
        source = (BOARD_DIR / "smart_home_controller.cc").read_text(encoding="utf-8")
        design = (ROOT / "docs" / "superpowers" / "specs" / "2026-07-05-smart-home-control-design.md").read_text(encoding="utf-8")

        self.assertIn("SetEcoMode", source)
        self.assertIn("SetAutoMode", source)
        self.assertIn("auto_mode_ = false", source)
        self.assertIn("eco_mode_ = false", source)
        self.assertIn("EvaluateAutoMode", source)
        self.assertIn("humidity_percent < 40.0f", source)
        self.assertIn("mq135_raw >= 2000", source)
        self.assertIn("servo_target_min_angle_", source)
        self.assertIn("servo_target_max_angle_", source)
        self.assertIn("0°-180°", design)
        self.assertIn("往复摆动", design)
```

- [ ] **Step 4: Add failing tests for curve history with ID gating**

Append:

```python
    def test_environment_history_is_kept_but_curve_writes_are_gated_by_numeric_ids(self):
        header = (BOARD_DIR / "smart_home_controller.h").read_text(encoding="utf-8")
        source = (BOARD_DIR / "smart_home_controller.cc").read_text(encoding="utf-8")

        self.assertIn("EnvironmentSample history_[30]", header)
        self.assertIn("history_write_index_", header)
        self.assertIn("RecordEnvironmentSample", source)
        self.assertIn("kCurveIdUnavailable", source)
        self.assertIn("MaybeSendCurvePoint", source)
        self.assertIn("if (curve_id < 0)", source)
        self.assertIn("add %d,%d,%d", source)
```

- [ ] **Step 5: Run tests and verify they fail for missing implementation**

Run:

```powershell
python -m unittest discover -s tests -v
```

Expected: FAIL with missing `smart_home_controller.h` or missing expected strings.

---

## Task 2: Add Smart-Home GPIO and PWM Configuration

**Files:**

- Modify: `main/boards/bread-compact-wifi/config.h`

- [ ] **Step 1: Add GPIO constants**

Add below the sensor definitions:

```cpp
// ===== 智能家居执行器引脚定义 =====
#define SMART_HOME_PURIFIER_LED_GPIO    GPIO_NUM_13
#define SMART_HOME_HUMIDIFIER_LED_GPIO  GPIO_NUM_14
#define SMART_HOME_FRESH_AIR_SERVO_GPIO GPIO_NUM_21

// GPIO47/GPIO48/GPIO45 当前预留，不在本阶段使用。
```

- [ ] **Step 2: Add LEDC constants**

Add below those GPIO constants:

```cpp
// ===== 智能家居 PWM 定义 =====
#define SMART_HOME_LEDC_SPEED_MODE      LEDC_LOW_SPEED_MODE
#define SMART_HOME_LED_TIMER            LEDC_TIMER_1
#define SMART_HOME_SERVO_TIMER          LEDC_TIMER_2
#define SMART_HOME_PURIFIER_LED_CHANNEL LEDC_CHANNEL_0
#define SMART_HOME_HUMIDIFIER_CHANNEL   LEDC_CHANNEL_1
#define SMART_HOME_SERVO_CHANNEL        LEDC_CHANNEL_2
#define SMART_HOME_LED_PWM_HZ           5000
#define SMART_HOME_SERVO_PWM_HZ         50
```

- [ ] **Step 3: Include LEDC header**

Ensure the header includes:

```cpp
#include <driver/ledc.h>
```

- [ ] **Step 4: Run regression tests**

Run:

```powershell
python -m unittest discover -s tests -v
```

Expected: still FAIL because controller files do not exist yet.

---

## Task 3: Implement `SmartHomeController`

**Files:**

- Create: `main/boards/bread-compact-wifi/smart_home_controller.h`
- Create: `main/boards/bread-compact-wifi/smart_home_controller.cc`

- [ ] **Step 1: Create the controller header**

Create `smart_home_controller.h`:

```cpp
#ifndef SMART_HOME_CONTROLLER_H
#define SMART_HOME_CONTROLLER_H

#include "config.h"
#include "mcp_server.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstddef>
#include <cstdint>

struct EnvironmentSample {
    bool has_temperature = false;
    float temperature_c = 0.0f;
    bool has_humidity = false;
    float humidity_percent = 0.0f;
    bool has_mq135_raw = false;
    int mq135_raw = 0;
    int air_score = 0;
};

struct SmartHomeState {
    int purifier_level = 0;
    int fresh_air_level = 0;
    int humidifier_level = 0;
    bool auto_mode = false;
    bool eco_mode = false;
};

class SmartHomeController {
public:
    SmartHomeController();
    ~SmartHomeController();

    void Initialize();
    bool HandleDeviceAction(const char* target, const char* action);
    bool HandleModeAction(const char* target, const char* action);
    void UpdateEnvironment(const EnvironmentSample& sample);

    void SetPurifier(bool power, int level);
    void SetFreshAir(bool power, int level);
    void SetHumidifier(bool power, int level);
    void SetAutoMode(bool enabled);
    void SetEcoMode(bool enabled);

    SmartHomeState GetState() const;

private:
    static constexpr size_t kHistorySize = 30;
    static constexpr int kCurveIdUnavailable = -1;

    void ConfigureLedc();
    void RegisterMcpTools();
    void ApplyAll();
    void ApplyPurifier();
    void ApplyHumidifier();
    void ApplyFreshAir();
    void SetLedDuty(ledc_channel_t channel, int percent);
    void SetServoAngle(int angle);
    void SetServoProfileForLevel(int level);
    void EvaluateAutoMode(const EnvironmentSample& sample);
    void RecordEnvironmentSample(const EnvironmentSample& sample);
    void MaybeSendCurvePoint(int curve_id, int channel, int value);
    int NormalizeLevel(bool power, int level) const;
    int ClampLevel(int level) const;
    const char* LevelText(int level) const;
    cJSON* BuildStateJson() const;

    static void ServoTaskEntry(void* arg);
    void ServoTaskLoop();

    SmartHomeState state_ = {};
    EnvironmentSample last_sample_ = {};
    EnvironmentSample history_[30] = {};
    size_t history_write_index_ = 0;
    size_t history_count_ = 0;
    bool initialized_ = false;

    int servo_target_min_angle_ = 0;
    int servo_target_max_angle_ = 0;
    int servo_step_degrees_ = 2;
    int servo_step_delay_ms_ = 60;
    bool servo_task_running_ = false;
    TaskHandle_t servo_task_handle_ = nullptr;
};

#endif // SMART_HOME_CONTROLLER_H
```

- [ ] **Step 2: Create the controller source skeleton**

Create `smart_home_controller.cc` with includes and helpers:

```cpp
#include "smart_home_controller.h"
#include "serial_hmi.h"

#include <esp_log.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

#define TAG "SmartHome"

namespace {
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

int AirScoreFromRaw(int raw) {
    return SerialHmi::EstimateAirScoreFromMq135Raw(raw);
}
} // namespace
```

- [ ] **Step 3: Implement initialization and LEDC setup**

Add:

```cpp
SmartHomeController::SmartHomeController() = default;

SmartHomeController::~SmartHomeController() {
    servo_task_running_ = false;
}

void SmartHomeController::Initialize() {
    if (initialized_) {
        return;
    }
    ConfigureLedc();
    RegisterMcpTools();
    ApplyAll();
    servo_task_running_ = true;
    xTaskCreate(ServoTaskEntry, "fresh_air_servo", 3072, this, 4, &servo_task_handle_);
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
```

- [ ] **Step 4: Implement actuator application methods**

Add:

```cpp
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

const char* SmartHomeController::LevelText(int level) const {
    switch (ClampLevel(level)) {
        case 1: return "low";
        case 2: return "medium";
        case 3: return "high";
        default: return "off";
    }
}

void SmartHomeController::SetLedDuty(ledc_channel_t channel, int percent) {
    percent = std::max(0, std::min(100, percent));
    const uint32_t max_duty = (1u << 13) - 1;
    const uint32_t duty = (max_duty * static_cast<uint32_t>(percent)) / 100u;
    ledc_set_duty(SMART_HOME_LEDC_SPEED_MODE, channel, duty);
    ledc_update_duty(SMART_HOME_LEDC_SPEED_MODE, channel);
}

void SmartHomeController::ApplyPurifier() {
    static const int kDutyByLevel[4] = {0, 33, 66, 100};
    SetLedDuty(SMART_HOME_PURIFIER_LED_CHANNEL, kDutyByLevel[ClampLevel(state_.purifier_level)]);
}

void SmartHomeController::ApplyHumidifier() {
    static const int kDutyByLevel[4] = {0, 33, 66, 100};
    SetLedDuty(SMART_HOME_HUMIDIFIER_CHANNEL, kDutyByLevel[ClampLevel(state_.humidifier_level)]);
}

void SmartHomeController::SetServoAngle(int angle) {
    angle = std::max(0, std::min(180, angle));
    const int min_us = 500;
    const int max_us = 2500;
    const int period_us = 20000;
    const int pulse_us = min_us + ((max_us - min_us) * angle) / 180;
    const uint32_t max_duty = (1u << 13) - 1;
    const uint32_t duty = (max_duty * static_cast<uint32_t>(pulse_us)) / period_us;
    ledc_set_duty(SMART_HOME_LEDC_SPEED_MODE, SMART_HOME_SERVO_CHANNEL, duty);
    ledc_update_duty(SMART_HOME_LEDC_SPEED_MODE, SMART_HOME_SERVO_CHANNEL);
}

void SmartHomeController::SetServoProfileForLevel(int level) {
    switch (ClampLevel(level)) {
        case 1:
            servo_target_min_angle_ = 20;
            servo_target_max_angle_ = 60;
            servo_step_degrees_ = 2;
            servo_step_delay_ms_ = 80;
            break;
        case 2:
            servo_target_min_angle_ = 15;
            servo_target_max_angle_ = 90;
            servo_step_degrees_ = 3;
            servo_step_delay_ms_ = 55;
            break;
        case 3:
            servo_target_min_angle_ = 0;
            servo_target_max_angle_ = 120;
            servo_step_degrees_ = 4;
            servo_step_delay_ms_ = 35;
            break;
        default:
            servo_target_min_angle_ = 0;
            servo_target_max_angle_ = 0;
            servo_step_degrees_ = 2;
            servo_step_delay_ms_ = 80;
            SetServoAngle(0);
            break;
    }
}

void SmartHomeController::ApplyFreshAir() {
    SetServoProfileForLevel(state_.fresh_air_level);
}

void SmartHomeController::ApplyAll() {
    ApplyPurifier();
    ApplyHumidifier();
    ApplyFreshAir();
}
```

- [ ] **Step 5: Implement public setters and screen-event handling**

Add:

```cpp
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
        state_.purifier_level = 0;
        state_.fresh_air_level = 0;
        state_.humidifier_level = 0;
        ApplyAll();
    }
}

SmartHomeState SmartHomeController::GetState() const {
    return state_;
}

bool SmartHomeController::HandleDeviceAction(const char* target, const char* action) {
    if (!TokenEquals(action, "TOGGLE")) {
        return false;
    }
    if (TokenEquals(target, "AIR_PURIFIER")) {
        SetPurifier(true, (state_.purifier_level + 1) % 4);
        return true;
    }
    if (TokenEquals(target, "FAN") || TokenEquals(target, "FRESH_AIR")) {
        SetFreshAir(true, (state_.fresh_air_level + 1) % 4);
        return true;
    }
    if (TokenEquals(target, "HUMIDIFIER")) {
        SetHumidifier(true, (state_.humidifier_level + 1) % 4);
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
```

- [ ] **Step 6: Implement automatic mode and history**

Add:

```cpp
void SmartHomeController::UpdateEnvironment(const EnvironmentSample& sample) {
    last_sample_ = sample;
    RecordEnvironmentSample(sample);
    if (state_.auto_mode && !state_.eco_mode) {
        EvaluateAutoMode(sample);
    }
}

void SmartHomeController::EvaluateAutoMode(const EnvironmentSample& sample) {
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

    ApplyAll();
}

void SmartHomeController::RecordEnvironmentSample(const EnvironmentSample& sample) {
    history_[history_write_index_] = sample;
    history_write_index_ = (history_write_index_ + 1) % kHistorySize;
    if (history_count_ < kHistorySize) {
        ++history_count_;
    }
    MaybeSendCurvePoint(kCurveIdUnavailable, 0, sample.air_score);
}

void SmartHomeController::MaybeSendCurvePoint(int curve_id, int channel, int value) {
    if (curve_id < 0) {
        return;
    }
    char command[32] = {};
    std::snprintf(command, sizeof(command), "add %d,%d,%d", curve_id, channel, value);
    ESP_LOGI(TAG, "Curve command pending HMI route: %s", command);
}
```

- [ ] **Step 7: Implement servo task loop**

Add:

```cpp
void SmartHomeController::ServoTaskEntry(void* arg) {
    static_cast<SmartHomeController*>(arg)->ServoTaskLoop();
}

void SmartHomeController::ServoTaskLoop() {
    int angle = 0;
    int direction = 1;
    while (servo_task_running_) {
        const int level = ClampLevel(state_.fresh_air_level);
        if (level == 0) {
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
    vTaskDelete(nullptr);
}
```

- [ ] **Step 8: Run tests**

Run:

```powershell
python -m unittest discover -s tests -v
```

Expected: FAIL only for missing MCP implementation and board integration.

---

## Task 4: Register MCP Tools

**Files:**

- Modify: `main/boards/bread-compact-wifi/smart_home_controller.cc`

- [ ] **Step 1: Add state JSON builder**

Add:

```cpp
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
    cJSON_AddBoolToObject(json, "has_temperature", last_sample_.has_temperature);
    cJSON_AddNumberToObject(json, "temperature_c", last_sample_.temperature_c);
    cJSON_AddBoolToObject(json, "has_humidity", last_sample_.has_humidity);
    cJSON_AddNumberToObject(json, "humidity_percent", last_sample_.humidity_percent);
    cJSON_AddBoolToObject(json, "has_mq135_raw", last_sample_.has_mq135_raw);
    cJSON_AddNumberToObject(json, "mq135_raw", last_sample_.mq135_raw);
    cJSON_AddNumberToObject(json, "air_score", last_sample_.air_score);
    return json;
}
```

- [ ] **Step 2: Add MCP registration method**

Add:

```cpp
void SmartHomeController::RegisterMcpTools() {
    auto& mcp_server = McpServer::GetInstance();

    mcp_server.AddTool("self.home.get_state",
        "Get purifier, fresh air, humidifier, automatic mode, eco mode, and latest environment state.",
        PropertyList(),
        [this](const PropertyList&) -> ReturnValue {
            return BuildStateJson();
        });

    mcp_server.AddTool("self.home.set_purifier",
        "Set purifier power and level. Level range is 0-3.",
        PropertyList({
            Property("power", kPropertyTypeBoolean),
            Property("level", kPropertyTypeInteger, 0, 3),
        }),
        [this](const PropertyList& properties) -> ReturnValue {
            SetPurifier(properties["power"].value<bool>(), properties["level"].value<int>());
            return true;
        });

    mcp_server.AddTool("self.home.set_fresh_air",
        "Set fresh air servo fan power and level. Level range is 0-3.",
        PropertyList({
            Property("power", kPropertyTypeBoolean),
            Property("level", kPropertyTypeInteger, 0, 3),
        }),
        [this](const PropertyList& properties) -> ReturnValue {
            SetFreshAir(properties["power"].value<bool>(), properties["level"].value<int>());
            return true;
        });

    mcp_server.AddTool("self.home.set_humidifier",
        "Set humidifier power and level. Level range is 0-3.",
        PropertyList({
            Property("power", kPropertyTypeBoolean),
            Property("level", kPropertyTypeInteger, 0, 3),
        }),
        [this](const PropertyList& properties) -> ReturnValue {
            SetHumidifier(properties["power"].value<bool>(), properties["level"].value<int>());
            return true;
        });

    mcp_server.AddTool("self.home.set_auto",
        "Enable or disable automatic environment control. Enabling auto exits eco mode.",
        PropertyList({
            Property("power", kPropertyTypeBoolean),
        }),
        [this](const PropertyList& properties) -> ReturnValue {
            SetAutoMode(properties["power"].value<bool>());
            return true;
        });

    mcp_server.AddTool("self.home.set_eco",
        "Enable or disable software eco mode. Enabling eco turns off auto mode and all actuators.",
        PropertyList({
            Property("power", kPropertyTypeBoolean),
        }),
        [this](const PropertyList& properties) -> ReturnValue {
            SetEcoMode(properties["power"].value<bool>());
            return true;
        });
}
```

- [ ] **Step 3: Run tests**

Run:

```powershell
python -m unittest discover -s tests -v
```

Expected: FAIL only for board integration and docs updates.

---

## Task 5: Integrate Controller Into `CompactWifiBoard`

**Files:**

- Modify: `main/boards/bread-compact-wifi/compact_wifi_board.cc`

- [ ] **Step 1: Include controller header**

Add:

```cpp
#include "smart_home_controller.h"
```

- [ ] **Step 2: Add member**

Below `SerialHmi serial_hmi_;`, add:

```cpp
    SmartHomeController smart_home_;
```

- [ ] **Step 3: Initialize controller in constructor**

After `InitializeButtons();`, add:

```cpp
        smart_home_.Initialize();
```

- [ ] **Step 4: Forward environment samples**

After `serial_hmi_.UpdateAirQuality(hmi_data);`, add:

```cpp
            EnvironmentSample environment = {};
            environment.has_temperature = display_dht_ok;
            environment.temperature_c = temp;
            environment.has_humidity = display_dht_ok;
            environment.humidity_percent = humi;
            environment.has_mq135_raw = mq_ok;
            environment.mq135_raw = air_raw;
            environment.air_score = hmi_data.air_score;
            smart_home_.UpdateEnvironment(environment);
```

- [ ] **Step 5: Forward device events**

Replace the reserved device/mode block with:

```cpp
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
```

- [ ] **Step 6: Run tests**

Run:

```powershell
python -m unittest discover -s tests -v
```

Expected: PASS for source-level tests after docs updates are complete.

---

## Task 6: Update HMI Contract and Manual Instructions

**Files:**

- Modify: `main/boards/bread-compact-wifi/serial_hmi_widgets.json`
- Modify: `../文档/串口屏手动事件配置手册.md`
- Modify: `../文档/串口屏设计说明.md`

- [ ] **Step 1: Add eco event to page2 contract**

In `serial_hmi_widgets.json`, under page2 widgets, add or update `hs_eco`:

```json
{
  "name": "hs_eco",
  "type": "touch_hotspot",
  "label": "节能模式",
  "event": "BTN,MODE,ECO,TOGGLE"
}
```

- [ ] **Step 2: Add optional curve widgets to page1 contract**

In page1 optional widgets, ensure:

```json
{
  "name": "c_air",
  "type": "curve",
  "label": "空气评分趋势曲线",
  "note": "可选。必须记录 HMI 编辑器分配的数字 id 后，固件才能发送 add objid,ch,val。"
}
```

If the HMI has room for temperature/humidity curves, add:

```json
{
  "name": "c_temp",
  "type": "curve",
  "label": "温度趋势曲线",
  "note": "可选。必须记录数字 id。"
}
```

```json
{
  "name": "c_humi",
  "type": "curve",
  "label": "湿度趋势曲线",
  "note": "可选。必须记录数字 id。"
}
```

- [ ] **Step 3: Update manual HMI event setup**

In `串口屏手动事件配置手册.md`, update page2 `hs_eco` instructions:

```text
节能模式：

prints "BTN,MODE,ECO,TOGGLE",0
printh 0a
```

Add curve ID note:

```text
曲线控件不能只记录控件名，还必须记录编辑器属性中的数字 ID。
请记录：
c_air 数字 ID：
c_temp 数字 ID：
c_humi 数字 ID：
未确认数字 ID 前，固件不会发送 add 命令。
```

- [ ] **Step 4: Run tests**

Run:

```powershell
python -m unittest discover -s tests -v
```

Expected: PASS.

---

## Task 7: Build and Write Handoff

**Files:**

- Create: `docs/phase-handoff-2026-07-05-smart-home-control.md`

- [ ] **Step 1: Run low-cost regression tests**

Run:

```powershell
python -m unittest discover -s tests -v
```

Expected:

```text
OK
```

- [ ] **Step 2: Build firmware**

Run from ESP-IDF terminal:

```powershell
& 'D:\esp\Espressif\frameworks\esp-idf-v5.5.2\export.ps1'
ninja -C build_codex_check -j 1
```

Expected:

```text
Successfully created esp32s3 image.
Generated E:/espwork/xiaozhi-esp32/xiaozhi-esp32/build_codex_check/xiaozhi.bin
```

- [ ] **Step 3: Write Chinese phase handoff**

Create `docs/phase-handoff-2026-07-05-smart-home-control.md`:

```markdown
# 2026-07-05 智能家居控制阶段交付

## 当前工程状态

- 工程目录：`E:/espwork/xiaozhi-esp32/xiaozhi-esp32`
- 当前板型：`bread-compact-wifi`
- 当前 HMI 文件：`D:/QQ/serial_warm_home  (1).HMI`

## 本阶段改动

- 新增 `SmartHomeController`，统一处理屏幕按键、MCP 语音控制和自动模式。
- `GPIO13` 控制净化红色 LED。
- `GPIO14` 控制加湿蓝色 LED。
- `GPIO21` 控制 SG90 舵机扇叶往复摆动。
- `GPIO47/GPIO48/GPIO45` 暂时保留。

## 验证结果

- 回归测试：记录 `python -m unittest discover -s tests -v` 的最终输出；通过时必须包含 `OK`，失败时记录首个失败测试名。
- 固件构建：记录 `ninja -C build_codex_check -j 1` 的最终输出；通过时必须包含生成的 `xiaozhi.bin` 路径，失败时记录首个编译错误。

## 实机验证重点

- 净化按钮：红色 LED 按 0-3 档循环。
- 新风按钮：SG90 扇叶按 0-3 档往复摆动。
- 加湿按钮：蓝色 LED 按 0-3 档循环。
- 自动模式：根据温湿度和 MQ135 raw 自动控制。
- 节能模式：关闭自动模式和所有执行器，但系统不进入 deep sleep。
- 语音 MCP：能调用 `self.home.*` 工具控制设备。

## 遗留风险

- 曲线控件数字 ID 仍需从 HMI 编辑器确认。
- SG90 舵机供电必须稳定，且与 ESP32 共地。
- 红色/蓝色 LED 需要串联限流电阻。
```

- [ ] **Step 4: Check git status**

Run:

```powershell
git status --short
```

Expected: only planned files and pre-existing dirty files are listed. Do not revert unrelated changes.

---

## Execution Notes

- Do not modify `.vscode/**`, `sdkconfig`, `sdkconfig.defaults*`, `CMakePresets.json`, ESP-IDF tools, or global environment.
- Do not run `idf.py set-target` or `menuconfig`.
- Do not commit unless the user explicitly asks; this worktree already contains many untracked project files.
- If HMI curve IDs become available, add a follow-up task that replaces `kCurveIdUnavailable` calls with confirmed numeric IDs and verifies `add objid,ch,val` on the real screen.
