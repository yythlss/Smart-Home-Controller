# LD2450 与光敏模块接入 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 `bread-compact-wifi` 板型中接入 GL5528 AO 光敏模块和 HLK-LD2450 雷达，使亮度数据驱动现有智能灯光规则，雷达数据可在串口、HTTP 状态和 AI 逻辑中安全观察。

**Architecture:** 将与 ESP-IDF 无关的亮度换算和 LD2450 目标帧解析放入纯 C++ 模块，以便在电脑端做真实单元测试。ADC 与 UART 驱动分别封装为板级传感器类；板级任务负责采样和日志，控制器只接收规范化亮度和“目标存在”观察值。雷达无目标绝不直接触发 `UpdatePresence(false)`，避免门口视场暂时为空时关闭全屋设备。

**Tech Stack:** ESP-IDF 5.5，C++17，ESP-IDF UART/ADC oneshot，FreeRTOS，Python `unittest`，本机 C++ 主机测试。

**Execution status (2026-07-18):** 已在当前工作区内联执行。由于本机没有可运行的主机 C++ 编译器，计划中的主机二进制测试未执行；已用现有 Python 回归测试和 ESP-IDF 真实构建替代验证。雷达方向/人数估计因尚无真实门口坐标日志保持未启用。

---

## 文件结构

| 文件 | 职责 |
| --- | --- |
| `main/boards/bread-compact-wifi/ambient_light_filter.h/.cc` | 纯 C++ 的原始 ADC 值滤波和 0-100% 亮度换算 |
| `main/boards/bread-compact-wifi/ambient_light_sensor.h/.cc` | 复用 MQ135 的 ADC1 句柄，配置并读取光敏模块 AO |
| `main/boards/bread-compact-wifi/ld2450_protocol.h/.cc` | 校验和解析 LD2450 的 35 字节目标数据帧 |
| `main/boards/bread-compact-wifi/ld2450_sensor.h/.cc` | UART1 接收缓存、帧同步和目标快照输出 |
| `main/boards/bread-compact-wifi/config.h` | GPIO2/ADC1_CH1、UART1 GPIO11/12 与采样参数 |
| `main/boards/bread-compact-wifi/mq135_sensor.h` | 暴露只读 ADC oneshot 句柄，供光敏模块复用 |
| `main/boards/bread-compact-wifi/smart_home_controller.h/.cc` | 雷达观测状态、亮度回差和 JSON 状态输出 |
| `main/boards/bread-compact-wifi/compact_wifi_board.cc` | 创建传感器、启动雷达任务、写入串口诊断 |
| `tests/ld2450_ambient_light_host_test.cc` | 主机端真实 C++ 单元测试 |
| `tests/test_bread_compact_wifi_regressions.py` | 工程集成契约测试 |
| `../文档/LD2450雷达与光敏模块接线验证.md` | 最终硬件连接和功能验证手册 |
| `docs/phase-handoff-2026-07-18-ld2450-ambient-light-integration.md` | 本阶段交付和接续说明 |

## Task 1: 写入并验证纯逻辑的失败测试

**Files:**
- Create: `tests/ld2450_ambient_light_host_test.cc`
- Modify: `tests/test_bread_compact_wifi_regressions.py`

- [ ] **Step 1: 写入失败的 C++ 测试**

```cpp
#include "ambient_light_filter.h"
#include "ld2450_protocol.h"

int main() {
    AmbientLightFilter filter(300, 3300);
    assert(filter.Normalize(300) == 0.0f);
    assert(filter.Normalize(3300) == 100.0f);

    uint8_t frame[Ld2450Protocol::kTargetFrameSize] = {};
    // AA FF 03 00 ... 55 CC，目标 1 位于 X=300mm、Y=900mm。
    assert(Ld2450Protocol::DecodeTargetFrame(frame, sizeof(frame), snapshot));
}
```

- [ ] **Step 2: 运行测试，确认它因缺少生产文件而失败**

Run:

```powershell
g++ -std=c++17 -I main/boards/bread-compact-wifi tests/ld2450_ambient_light_host_test.cc -o tmp/ld2450_ambient_light_host_test.exe
```

Expected: 编译失败，提示 `ambient_light_filter.h` 或 `ld2450_protocol.h` 不存在。

- [ ] **Step 3: 添加 Python 集成契约测试**

```python
def test_ld2450_and_ambient_light_hardware_contract(self):
    config = (BOARD_DIR / "config.h").read_text(encoding="utf-8")
    board = (BOARD_DIR / "compact_wifi_board.cc").read_text(encoding="utf-8")
    controller = (BOARD_DIR / "smart_home_controller.cc").read_text(encoding="utf-8")

    self.assertIn("LD2450_UART_PORT", config)
    self.assertIn("AMBIENT_LIGHT_ADC_CHANNEL", config)
    self.assertIn("Ld2450Sensor", board)
    self.assertIn("AmbientLightSensor", board)
    self.assertIn("UpdateRadarObservation", controller)
```

- [ ] **Step 4: 运行 Python 测试，确认新契约尚未满足**

Run:

```powershell
python -m unittest tests.test_bread_compact_wifi_regressions.BreadCompactWifiRegressionTest.test_ld2450_and_ambient_light_hardware_contract -v
```

Expected: FAIL，缺少 `LD2450_UART_PORT`、`Ld2450Sensor` 和 `UpdateRadarObservation`。

## Task 2: 实现可测试的光敏换算与 LD2450 帧解析

**Files:**
- Create: `main/boards/bread-compact-wifi/ambient_light_filter.h`
- Create: `main/boards/bread-compact-wifi/ambient_light_filter.cc`
- Create: `main/boards/bread-compact-wifi/ld2450_protocol.h`
- Create: `main/boards/bread-compact-wifi/ld2450_protocol.cc`
- Modify: `tests/ld2450_ambient_light_host_test.cc`

- [ ] **Step 1: 最小实现光敏换算器**

```cpp
class AmbientLightFilter {
public:
    AmbientLightFilter(int dark_raw, int bright_raw);
    float Normalize(int raw) const;
    float PushSample(int raw);
private:
    int dark_raw_;
    int bright_raw_;
    bool has_filtered_value_ = false;
    float filtered_percent_ = 0.0f;
};
```

`Normalize()` 必须允许 `dark_raw > bright_raw`，以适配接线方向不同的 AO 模块；输出必须限制在 0-100。

- [ ] **Step 2: 最小实现 LD2450 固定目标帧解码器**

```cpp
struct Ld2450Target {
    bool active = false;
    int16_t x_mm = 0;
    int16_t y_mm = 0;
    int16_t speed_mm_per_s = 0;
    uint16_t distance_mm = 0;
};

struct Ld2450Snapshot {
    bool valid = false;
    uint8_t active_target_count = 0;
    Ld2450Target targets[3] = {};
};
```

解码器必须验证 `AA FF 03 00` 头、`55 CC` 尾、固定 35 字节长度和 `0x02` 目标数据类型。目标记录从字节 7 开始，每个记录为 8 字节的小端序 `x/y/speed/distance`。`distance_mm == 0` 的目标必须标记为非活动目标。

- [ ] **Step 3: 编译并运行主机测试**

Run:

```powershell
g++ -std=c++17 -I main/boards/bread-compact-wifi tests/ld2450_ambient_light_host_test.cc main/boards/bread-compact-wifi/ambient_light_filter.cc main/boards/bread-compact-wifi/ld2450_protocol.cc -o tmp/ld2450_ambient_light_host_test.exe
& .\tmp\ld2450_ambient_light_host_test.exe
```

Expected: 退出码 0。

## Task 3: 实现 ESP32 的 ADC 与 UART 适配器

**Files:**
- Create: `main/boards/bread-compact-wifi/ambient_light_sensor.h`
- Create: `main/boards/bread-compact-wifi/ambient_light_sensor.cc`
- Create: `main/boards/bread-compact-wifi/ld2450_sensor.h`
- Create: `main/boards/bread-compact-wifi/ld2450_sensor.cc`
- Modify: `main/boards/bread-compact-wifi/mq135_sensor.h`
- Modify: `main/boards/bread-compact-wifi/config.h`

- [ ] **Step 1: 将候选硬件参数写入板型配置**

```cpp
#define AMBIENT_LIGHT_ADC_UNIT        ADC_UNIT_1
#define AMBIENT_LIGHT_ADC_CHANNEL     ADC_CHANNEL_1 // GPIO2
#define AMBIENT_LIGHT_DARK_RAW        300
#define AMBIENT_LIGHT_BRIGHT_RAW      3300

#define LD2450_UART_PORT              UART_NUM_1
#define LD2450_UART_RX_PIN            GPIO_NUM_11
#define LD2450_UART_TX_PIN            GPIO_NUM_12
#define LD2450_UART_BAUD_RATE         256000
```

- [ ] **Step 2: 复用 MQ135 的 ADC 句柄，避免重复创建 ADC1 单元**

```cpp
adc_oneshot_unit_handle_t GetAdcHandle() const { return adc_handle_; }
```

`AmbientLightSensor` 必须只调用 `adc_oneshot_config_channel()` 和 `adc_oneshot_read()`，不得调用第二次 `adc_oneshot_new_unit(ADC_UNIT_1)`。

- [ ] **Step 3: 实现 UART1 接收缓存**

`Ld2450Sensor::Poll()` 必须使用 `uart_read_bytes()` 追加字节，查找数据帧头，保留不完整帧，验证尾部后交给 `Ld2450Protocol::DecodeTargetFrame()`。遇到垃圾字节时仅丢弃到下一个帧头，不能重装 UART 驱动。

- [ ] **Step 4: 重新运行主机测试与 Python 契约测试**

Run:

```powershell
& .\tmp\ld2450_ambient_light_host_test.exe
python -m unittest tests.test_bread_compact_wifi_regressions.BreadCompactWifiRegressionTest.test_ld2450_and_ambient_light_hardware_contract -v
```

Expected: 两条命令均 PASS。

## Task 4: 接入板级任务和智能家居状态

**Files:**
- Modify: `main/boards/bread-compact-wifi/compact_wifi_board.cc`
- Modify: `main/boards/bread-compact-wifi/smart_home_controller.h`
- Modify: `main/boards/bread-compact-wifi/smart_home_controller.cc`
- Modify: `tests/test_bread_compact_wifi_regressions.py`

- [ ] **Step 1: 为控制器写入失败测试**

```python
for name in ["has_radar_data", "radar_target_count", "UpdateRadarObservation"]:
    self.assertIn(name, controller_header + controller_source)
self.assertIn("kLightOffThresholdPercent", controller_source)
self.assertIn("state_.ambient_light_percent >= kLightOffThresholdPercent", controller_source)
```

- [ ] **Step 2: 确认失败原因正确**

Run:

```powershell
python -m unittest tests.test_bread_compact_wifi_regressions.BreadCompactWifiRegressionTest.test_radar_status_and_light_hysteresis_contract -v
```

Expected: FAIL，缺少雷达状态字段和关灯回差阈值。

- [ ] **Step 3: 最小实现控制器联动**

```cpp
void SmartHomeController::UpdateRadarObservation(int target_count) {
    state_.has_radar_data = true;
    state_.radar_target_count = std::max(0, std::min(3, target_count));
    if (state_.radar_target_count > 0) {
        UpdatePresence(true);
    }
}
```

`UpdateRadarObservation(0)` 只更新雷达状态，绝不调用 `UpdatePresence(false)`。亮度规则应使用 25%开灯、35%关灯的回差，位于中间区间时保持已有灯状态。

- [ ] **Step 4: 创建并启动雷达任务**

在 `CompactWifiBoard` 中构造 `AmbientLightSensor` 与 `Ld2450Sensor`。在既有 `SensorTask()` 中读取光敏值；新增 `RadarTask()`，每 100ms 轮询 UART1，输出目标数量和每个活动目标的 X/Y/速度/距离。调用 `UpdateRadarObservation(snapshot.active_target_count)`。

- [ ] **Step 5: 运行所有回归测试**

Run:

```powershell
python -m unittest discover -s tests -v
```

Expected: PASS。

## Task 5: 编译和硬件交付

**Files:**
- Create: `../文档/LD2450雷达与光敏模块接线验证.md`
- Create: `docs/phase-handoff-2026-07-18-ld2450-ambient-light-integration.md`
- Modify: `docs/current-project-handoff.md`
- Modify: `docs/continuation-notes.md`

- [ ] **Step 1: 运行板型静态构建**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build_codex_check.ps1 -Reconfigure
```

Expected: `Successfully created esp32s3 image.`

- [ ] **Step 2: 写入详细硬件文档**

文档必须包含：供电、共地、AO 与 UART 交叉连接、禁止将 5V UART 直接接 ESP32、安装位置、串口日志预期、光敏校准、雷达首帧确认、AI 唤醒验证、自动灯光验证、异常排查和未启用的“无人自动关闭”保护。

- [ ] **Step 3: 写入阶段交付**

阶段交付必须记录改动文件、实际测试命令、构建结果、尚未在实物验证的项目，以及需要用户手动执行的接线和校准步骤。

## 自检结论

- 计划覆盖了光敏采样、LD2450 数据接收、智能灯光、AI 触发、HTTP 状态、主机测试、ESP-IDF 构建和硬件验证文档。
- 雷达的方向/人数估计不在本次首轮固件中直接启用，因为其安装方向和门口实际坐标尚未由实物数据校准；首轮先保证它不会误关设备。
- 本计划不修改 `.vscode`、`sdkconfig`、`sdkconfig.defaults`、CMake 工具链或全局环境变量。
