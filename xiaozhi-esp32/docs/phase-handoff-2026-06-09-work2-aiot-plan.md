# 阶段交付：小智 AIoT 最小扩展方案

日期：2026-06-09

## 1. 本阶段目标

根据当前 `bread-compact-wifi` 工程状态，整理一份最小可实行的 AIoT 扩展方案，说明：

- 现有小智 AI 如何控制 MCU。
- 当前推荐先加入哪些功能。
- 哪些功能不需要新增硬件。
- 如果要真实控制外设，必须新增哪些硬件。
- 队友拿到板子后应如何从硬件、软件、调试三方面验证。

## 2. 已完成

- 已按接续规则只读检查当前工程状态。
- 已确认当前工程根目录为 `E:/espwork/xiaozhi-esp32/xiaozhi-esp32`。
- 已确认当前板型为 `main/boards/bread-compact-wifi`。
- 已确认当前串口屏、DHT11、MQ135、按键和音频引脚配置。
- 已确认串口屏智能家居事件目前只解析和打日志，尚未控制真实外设。
- 已确认小智工程已有 MCP 工具机制，板级自定义工具应注册在板型的 `InitializeTools()` 中。
- 已新增详细方案文档：

```text
docs/minimum-aiot-expansion-plan.md
```

## 3. 改动文件

本阶段只新增文档，没有修改源码、构建配置或受保护环境文件。

```text
docs/minimum-aiot-expansion-plan.md
docs/phase-handoff-2026-06-09-work2-aiot-plan.md
```

## 4. 当前推荐结论

第一阶段最小闭环建议为：

1. Wi-Fi 连接后使用 SNTP 同步北京时间。
2. 使用固定城市或固定经纬度获取天气，优先用无需 Key 的 Open-Meteo 跑通演示链路。
3. 保留当前小智语音对话和 GPIO47 按住说话逻辑。
4. 新增 MCP 工具 `self.env.get_status`，让 AI 能读取 DHT11、MQ135、时间、天气和设备状态。
5. 新增 MCP 工具 `self.home.set_device`、`self.home.set_auto`、`self.screen.show_page`，让 AI 控制软件状态和串口屏页面。
6. 真实外设先用低压 LED 或低压风扇验证，不直接控制 220V 负载。

## 5. 验证结果

本阶段为方案和文档整理，没有运行 ESP-IDF 构建。

已完成的只读验证：

- `git status --short`：确认当前工作树已有前期代码和文档改动，未回滚或覆盖。
- 已读取当前板型目录、关键文档、`compact_wifi_board.cc`、`config.h`、MCP 相关代码和示例工具注册代码。

历史构建状态仍以 `docs/current-project-handoff.md` 记录为准：

```text
idf.py -B build_codex_check reconfigure
idf.py -B build_codex_check build
Project build complete.
```

## 6. 遗留风险

- 板子在队友手上，当前无法直接验证实机 Wi-Fi、音频、屏幕和传感器状态。
- 天气 API 需要实机网络验证，可能受 DNS、TLS 证书、网络环境影响。
- MCP 工具能否被自然语言稳定调用，需要小智后端和设备连接状态共同验证。
- 真实外设控制必须重新确认可用 GPIO，避免和音频、屏幕、ADC、按键冲突。
- MQ135 当前只适合演示级空气质量判断，不应作为准确 ppm 指标。

## 7. 下一阶段建议

队友拿到文档后建议按以下顺序做：

1. 先烧录当前工程，完成小智对话、串口屏、DHT11、MQ135 的基线验证。
2. 补 HMI 控件 `t_time`、`t_date`、`t_weather`、`t_out_temp`、`t_sync`。
3. 加 SNTP 时间服务并验证屏幕显示。
4. 加天气 HTTP 服务并验证屏幕显示。
5. 加 `self.env.get_status` MCP 工具，先验证 AI 能读取传感器数据。
6. 加软件设备状态控制，验证 AI 和屏幕按钮控制同一份状态。
7. 最后再接低压 LED 或低压风扇做真实控制演示。

## 8. 需要人工确认

- 实物是否已经接好 INMP441 麦克风、MAX98357A 功放和扬声器。
- 当前板子的可用空闲 GPIO。
- 演示城市或经纬度。
- 天气服务是否接受使用 Open-Meteo，还是必须使用国内带 Key 的天气服务。
- 比赛演示是否需要真实外设动作；如果需要，使用低压负载还是继电器模块。
