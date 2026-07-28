# 2026-07-06 智能家居外设输出诊断与 Brownout 排查交付

## 问题现象

用户烧录后按下串口屏设备热区，monitor 已经能看到事件进入 ESP32：

```text
I (...) TestBoard: Screen event: raw=BTN,DEVICE,HUMIDIFIER,TOGGLE target=HUMIDIFIER action=TOGGLE
I (...) TestBoard: Screen event: raw=BTN,DEVICE,FAN,TOGGLE target=FAN action=TOGGLE
```

但外设没有按预期动作。用户随后提供的新日志里还出现：

```text
E BOD: Brownout detector was triggered
--- Error: ClearCommError failed ...
--- Waiting for the device to reconnect.
```

## 根因判断

本次日志分成两类问题：

1. `Screen event` 说明 HMI 事件、TJC 串口回传、`SerialHmi::PollEvent()` 和 `ScreenEventTask()` 已经通了。
2. `Brownout detector was triggered` 是 ESP32 检测到供电电压跌落后复位。它是当前最关键的硬件错误；`ClearCommError` 是复位后 USB 串口断开导致的 PC 端连带报错，不是根因。

`Unknown screen event: .J`、`Unknown screen event: W]}` 这类短乱码，更像 brownout/复位瞬间 UART 线上读到的无效字节。只要后续不再 brownout，它通常会一起消失；如果没有 brownout 仍持续出现，再单独检查屏幕 TX 电平、共地和串口线。

## 本阶段代码改动

- `main/boards/bread-compact-wifi/smart_home_controller.h`
  - 新增 `NextToggleLevel(int level) const`。

- `main/boards/bread-compact-wifi/smart_home_controller.cc`
  - 修复设备触摸切换无法真正回到 0 档的问题。
  - 旧逻辑用 `(level + 1) % 4` 得到 0 后仍调用 `SetX(true, 0)`，会被 `NormalizeLevel(true, 0)` 强制改回 1。
  - 新逻辑为 `next_level == 0` 时调用 `SetX(false, 0)`，确保循环是 `关 -> 低 -> 中 -> 高 -> 关`。
  - 新增实际输出诊断日志：

```text
Apply purifier output: gpio=13 level=1 duty=33%
Apply humidifier output: gpio=14 level=1 duty=33%
Apply fresh air fan output: gpio=21 level=1 pulse=1600us
Device action applied: target=FAN level=1
```

  - 给 `ledc_set_duty()` 和 `ledc_update_duty()` 增加返回值检查；如果 PWM 写入失败，会输出错误日志。

## 重新烧录后的预期日志

按一次加湿器热区，应看到：

```text
Screen event: raw=BTN,DEVICE,HUMIDIFIER,TOGGLE target=HUMIDIFIER action=TOGGLE
SmartHome: Apply humidifier output: gpio=14 level=1 duty=33%
SmartHome: Device action applied: target=HUMIDIFIER level=1
```

连续按风扇热区 4 次，应看到：

```text
SmartHome: Apply fresh air fan output: gpio=21 level=1 pulse=1600us
SmartHome: Apply fresh air fan output: gpio=21 level=2 pulse=1750us
SmartHome: Apply fresh air fan output: gpio=21 level=3 pulse=1900us
SmartHome: Apply fresh air fan output: gpio=21 level=0 pulse=1500us
```

判断规则：

- 只有 `Screen event`，没有 `Apply ... output`：事件没有进入有效控制分支。
- 同时有 `Screen event` 和 `Apply ... output`：固件已经写 GPIO/PWM；外设仍不动时优先查接线、供电、共地、LED 极性和模块触发方式。
- 出现 `Brownout detector was triggered`：先处理供电，不要继续纠结串口屏事件或代码逻辑。

## Brownout 优先排查

`Brownout detector was triggered` 表示 ESP32 供电瞬间跌到安全阈值以下。当前工程外设里，最容易触发 brownout 的是：

- 舵机风扇启动电流。
- 串口屏背光电流。
- WiFi 扫描/连接时 ESP32 瞬时电流。
- 面包板电源轨接触不良或线太细。
- 用开发板 5V/USB 同时带屏幕和舵机。

建议复测顺序：

1. 先只接 ESP32S3 USB，不接屏幕、不接舵机风扇、不接 LED，确认没有 brownout。
2. 接屏幕电源和 TX/RX，确认没有 brownout。
3. 接 DHT11、MQ135，确认没有 brownout。
4. 接 LED，确认没有 brownout。
5. 最后接舵机风扇。舵机风扇红线用独立稳定 5V，棕/黑线必须与 ESP32S3 GND 共地。

如果一接舵机风扇就 brownout：

- 不要从 ESP32S3 的 3.3V 或 GPIO 给舵机供电。
- 优先使用独立 5V 电源，电流能力建议至少 1A，多个外设同时供电建议 2A 或以上。
- 舵机电源 GND 和 ESP32S3 GND 必须共地。
- 缩短舵机供电线，避免面包板松动。
- 可在舵机 5V 和 GND 旁并联较大电容作缓冲，例如 470uF-1000uF，注意极性。

## 验证结果

已先运行新增回归测试并观察到失败，失败点为 `NextToggleLevel` 不存在，证明测试覆盖了当前缺口。

修改后源码级回归测试通过：

```text
python -m unittest discover -s tests -v
Ran 10 tests in 0.005s
OK
```

HMI 控件契约 JSON 解析通过：

```text
python -m json.tool main/boards/bread-compact-wifi/serial_hmi_widgets.json
```

受保护配置文件检查无输出，本阶段未修改 `.vscode/**`、`sdkconfig*`、`CMakePresets.json`：

```text
git status --short -- .vscode sdkconfig sdkconfig.defaults sdkconfig.defaults.esp32 sdkconfig.defaults.esp32s3 CMakePresets.json
```

ESP-IDF 构建通过：

```text
powershell -ExecutionPolicy Bypass -File scripts/build_codex_check.ps1
Successfully created esp32s3 image.
Generated E:/espwork/xiaozhi-esp32/xiaozhi-esp32/build_codex_check/xiaozhi.bin
xiaozhi.bin binary size 0x244120 bytes.
Smallest app partition is 0x3f0000 bytes.
0x1abee0 bytes (42%) free.
```

## 用户和队友手动复测步骤

1. 重新烧录当前 ESP32 固件。
2. 打开 USB monitor。
3. 先不接舵机风扇，确认没有 `Brownout detector was triggered`。
4. 按 `HUMIDIFIER` 热区，确认同时出现 `Screen event`、`Apply humidifier output` 和 `Device action applied`。
5. 连续按 `FAN` 热区 4 次，确认 `level` 依次为 `1,2,3,0`。
6. 接舵机风扇时使用独立 5V 供电并共地。
7. 如果 `Apply ... output` 日志正常但外设不动作，按硬件接线和供电继续排查。
8. 如果仍出现 brownout，先处理电源，不要继续做功能验证。

## 遗留风险

- 本阶段无法替代万用表或示波器对真实 GPIO/PWM 和 5V 电压跌落的测量。
- 如果使用继电器模块、MOS 驱动板或成品风扇控制板，当前 PWM/电平策略可能需要按模块规格调整。
- 如果 brownout 发生在 WiFi 扫描阶段，说明整体供电裕量不足，即使外设不动作也需要先加强 ESP32S3 和屏幕供电。

## 下一阶段目标

1. 用户烧录后回传新的按键 monitor 日志。
2. 如果没有 brownout 且有 `Apply ... output`，继续按硬件链路排查外设。
3. 如果仍有 brownout，优先调整供电方案和接线顺序。
