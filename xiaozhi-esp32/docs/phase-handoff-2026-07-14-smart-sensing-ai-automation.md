# 2026-07-14 智能感知、AI 联网与自动控制增强交付

## 1. 本阶段目标

本阶段围绕以下需求扩展现有 `bread-compact-wifi` 工程：

- 为蜂鸣器、雷达传感器、独立灯光和光敏传感器预留统一软件接口。
- 重做节能模式，按环境使用较低档位，不再直接关闭全部设备。
- 保留固件规则控制与 AI/MCP 判断控制两条路径。
- 增加无人关机、有人且暗时开灯、环境突变报警。
- 扩展小智远端 MCP 桥接，使 AI 可以查天气、读 RSS 新闻并给出室内外综合建议。
- 核对唤醒词、雷达开麦、定时播报和 ESP32-S3 蓝牙的可行性。

本阶段没有猜测新硬件 GPIO，也没有修改 `.vscode/**`、`sdkconfig*`、`CMakePresets.json`、ESP-IDF 工具链或系统环境变量。

## 2. 开发前确认的当前状态

- 工程根目录：`E:/espwork/xiaozhi-esp32/xiaozhi-esp32`。
- 当前板型：`main/boards/bread-compact-wifi`。
- 当前芯片：ESP32-S3。
- 现有输入：DHT11 温湿度、MQ135 模拟空气质量。
- 现有输出：GPIO13 净化 LED、GPIO14 加湿 LED、GPIO21 180°角度舵机扇叶。
- 当前小程序 HTTP API 可以正常连接和控制。
- 当前小智 MCP 桥接位于 `tools/xiaozhi_mcp_bridge/`，真实 token 未写入工程。
- 当前受保护的 `sdkconfig` 已显示 `CONFIG_USE_AFE_WAKE_WORD=y`，说明上游 AFE 唤醒词路径已经启用；本阶段只读确认，没有改配置。

## 3. 本阶段已经实现的代码能力

### 3.1 新状态

`SmartHomeState` 新增：

```text
occupancy_known
occupied
has_ambient_light
ambient_light_percent
light_on
alarm_active
```

`GET /api/state` 和 `self.home.get_state` 会返回这些字段，并额外返回 `alarm_reason`。

### 3.2 新硬件接入钩子

`SmartHomeController` 新增：

```text
UpdatePresence(bool occupied)
UpdateAmbientLight(float ambient_light_percent)
SetLight(bool power)
AcknowledgeAlarm()
SetPresenceDetectedCallback(...)
SetLightOutputCallback(...)
SetAlarmOutputCallback(...)
```

这些接口是后续真实驱动的稳定边界：

- 雷达驱动只需把有人/无人结果交给 `UpdatePresence()`。
- 光敏驱动只需把换算后的 0-100 亮度百分比交给 `UpdateAmbientLight()`。
- 灯光驱动注册 `SetLightOutputCallback()`，控制 GPIO、MOSFET 或继电器。
- 蜂鸣器驱动注册 `SetAlarmOutputCallback()`，按报警状态启停蜂鸣器。

当前还没有注册真实灯光和蜂鸣器输出回调，因此 `light_on`、`alarm_active` 已能联调，但不会驱动尚未确认的 GPIO。

### 3.3 节能模式重做

节能模式与普通自动模式互斥，但不再“全部关闭”。当前节能规则：

| 环境条件 | 节能动作 |
| --- | --- |
| MQ135 raw `>= 2000` 或空气评分 `< 40` | 净化 2 档，新风 1 档 |
| MQ135 raw `>= 1000` 或空气评分 `< 65` | 净化 1 档 |
| 湿度 `< 35%` | 加湿 1 档 |
| 温度 `> 30°C` | 新风至少 1 档 |
| 环境正常 | 对应设备关闭 |
| 雷达明确判定无人 | 净化、新风、加湿、灯全部关闭 |

普通自动模式仍保留原有较积极的 2-3 档控制。AI 也可以直接调用设备工具；直接开启净化、新风或加湿时，会退出节能模式，因此“代码规则判断 + AI 判断”两条路径同时保留。

### 3.4 智能灯光

当前规则：

```text
雷达状态已知
且 occupied=true
且光敏数据有效
且 ambient_light_percent <= 25
=> 开灯
```

无人、环境变亮或占用状态变为 false 时关灯。该规则避免“环境暗但没人也开灯”。

### 3.5 无人关机

当 `UpdatePresence(false)` 明确收到无人状态时，控制器立即执行：

```text
purifier_level=0
fresh_air_level=0
humidifier_level=0
light_on=false
```

注意：单个门口雷达通常不能判断进门/出门方向，也不能可靠统计屋内人数。精确无人判断建议使用以下任一方案：

1. 室内毫米波存在传感器，直接检测房间是否持续有人。
2. 门口两个检测区按先后顺序判断进入/离开。
3. 门磁负责门状态，雷达负责门内存在状态。

### 3.6 环境突变报警

相邻两次环境样本满足任一条件时，报警状态锁存：

| 参数 | 当前阈值 |
| --- | ---: |
| 温度变化 | `>= 5°C` |
| 湿度变化 | `>= 20%` |
| 空气评分变化 | `>= 30` |
| MQ135 raw 变化 | `>= 1200` |

报警后：

- `alarm_active=true`。
- `alarm_reason` 记录变化原因。
- 板级回调调用 `Application::Alert()`，在现有扬声器播放提示音并显示异常信息。
- `AcknowledgeAlarm()`、HTTP `/api/alarm/ack` 或 MCP 报警确认工具可清除报警。

真实蜂鸣器输出尚未接入，等待蜂鸣器类型和 GPIO 确认。

## 4. 新增 HTTP API

### 4.1 更新雷达和环境亮度

```http
POST /api/context
Content-Type: application/json

{
  "occupied": true,
  "ambient_light_percent": 18
}
```

### 4.2 确认报警

```http
POST /api/alarm/ack
```

### 4.3 控制灯光

复用 `/api/device`：

```http
POST /api/device
Content-Type: application/json

{
  "device": "light",
  "power": true,
  "level": 1
}
```

## 5. AI/MCP 能力

### 5.1 ESP32 设备端工具

新增：

```text
self.home.set_light
self.home.update_context
self.home.acknowledge_alarm
self.home.get_environment_briefing
```

原有净化、新风、加湿、自动、节能、手动环境、环境预设和建议工具继续保留。

### 5.2 电脑端小智 MCP 桥接工具

新增：

```text
home_set_light
home_update_context
home_acknowledge_alarm
home_get_weather
home_get_news
home_get_combined_advice
```

天气使用 Open-Meteo，不需要 API 密钥。新闻使用用户指定的可信 RSS：

```powershell
$env:NEWS_RSS_URL = "https://你的可信新闻源/rss.xml"
```

AI 综合建议会同时参考：

- 室内温度、湿度、空气评分、是否有人。
- 室外温度、天气现象和最高降雨概率。

示例语音：

```text
查询杭州天气
读三条新闻
结合杭州天气和家里的环境给我建议
打开灯
进入节能模式
确认环境报警
```

## 6. 唤醒方案结论

### 6.1 提示词不能唤醒麦克风

提示词由 AI 模型处理，前提是设备已经采集语音并送到服务端。因此提示词不能替代本地唤醒词，也不能在麦克风关闭时自行打开麦克风。

### 6.2 当前工程已有本地唤醒词链路

只读检查显示：

```text
CONFIG_USE_AFE_WAKE_WORD=y
CONFIG_SEND_WAKE_WORD_DATA=y
```

上游代码已有 `EspWakeWord/AfeWakeWord` 和 `Application::WakeWordInvoke()`。如果实机仍只能按键，应优先检查：

1. 唤醒词模型资源是否实际打包。
2. 麦克风 I2S 接线和采样是否正常。
3. 启动日志是否显示 AFE/唤醒词初始化成功。
4. 环境噪声、麦克风增益和唤醒阈值。

本阶段没有修改受保护的 `sdkconfig`。

### 6.3 雷达触发开麦

当前板级入口已经注册回调：占用状态从未知/无人变为有人时，调用线程安全的 `Application::StartListening()`。

该功能要真正工作，还需要雷达驱动调用 `UpdatePresence()`。建议加入防抖和冷却时间，避免门口连续移动导致重复开麦。

## 7. 定时播报与异常语音提醒

已完成：

- `self.home.get_environment_briefing` 可返回适合 AI 播报的环境摘要。
- 环境异常会主动播放现有提示音并显示报警原因。

尚未完成：

- 固件端任意中文文本的主动 TTS。
- 固定时间由设备主动请求云端播报。

当前 `Application` 对板级代码公开的是内置音频播放和 `Alert()`，没有稳定的“提交任意文本并要求云端 TTS”接口。后续可选方案：

1. 小智平台提供定时任务/主动消息能力时，由云端定时调用环境摘要工具并播报。
2. 扩展设备协议，定义明确的主动 TTS 请求消息，再由服务端支持。
3. 预先录制少量固定播报音频，只适合固定告警，不适合动态温湿度数值。

## 8. 新硬件选型和接入前必须确认

| 硬件 | 必须确认 | 当前建议 |
| --- | --- | --- |
| 蜂鸣器 | 有源/无源、工作电压、电流、是否需三极管 | 简单报警优先有源蜂鸣器；电流较大时用三极管/MOSFET，不直接由 GPIO 供电 |
| 雷达 | 型号、数字 OUT/UART、供电、检测距离 | 只做有人/无人可选带数字 OUT 的毫米波存在模块；判断进出方向需双区域或门磁配合 |
| LED 灯 | 指示灯还是照明灯、工作电压和电流 | 小 LED 串限流电阻；照明灯必须用 MOSFET/继电器驱动 |
| 光敏 | LDR 模拟量还是 BH1750 等数字传感器 | 要稳定亮度数值优先数字光照传感器；使用 LDR 时需确认 ADC 输入不超过 3.3V |

当前已占用的主要 GPIO：

```text
GPIO0  启动按键
GPIO1  MQ135 ADC
GPIO4/5/6/7/15/16 音频 I2S
GPIO13 净化 LED
GPIO14 加湿 LED
GPIO18 DHT11
GPIO21 180°舵机扇叶
GPIO39/40/47 按键
GPIO41/42 TJC UART2
GPIO48 板载状态 LED
```

不要在没有原理图和模块型号的情况下从剩余 GPIO 中随意挑选。

## 9. ESP32-S3 蓝牙待研究方向

ESP32-S3 支持 Bluetooth LE，不支持经典蓝牙。建议后续优先评估：

1. BLE 配网或首次绑定，减少手动输入 WiFi。
2. 手机靠近检测，作为雷达占用的辅助证据。
3. BLE 传感器网关，接入低功耗温湿度或门磁。
4. 本地维护通道，读取设备状态或修改非敏感参数。

不建议把 BLE 手机信号强度单独作为“家里是否有人”的唯一依据，因为手机可能没带、蓝牙可能关闭，RSSI 也会受墙体和人体遮挡影响。

## 10. 无新硬件时的联调步骤

### 10.1 模拟有人且环境暗

```powershell
$base = "http://<ESP32_IP>:8080"
$body = @{ occupied = $true; ambient_light_percent = 18 } | ConvertTo-Json
Invoke-RestMethod "$base/api/context" -Method Post -ContentType "application/json" -Body $body
```

预期：

```text
occupied=true
light_on=true
monitor 出现 Presence detected, opening AI microphone
```

### 10.2 模拟无人

```powershell
$body = @{ occupied = $false; ambient_light_percent = 18 } | ConvertTo-Json
Invoke-RestMethod "$base/api/context" -Method Post -ContentType "application/json" -Body $body
```

预期所有档位为 0，`light_on=false`。

### 10.3 验证节能模式不是全部关闭

先模拟污染环境，再开启节能：

```powershell
$body = @{ enabled = $true; preset = "POLLUTED" } | ConvertTo-Json
Invoke-RestMethod "$base/api/environment" -Method Post -ContentType "application/json" -Body $body
$body = @{ mode = "eco"; power = $true } | ConvertTo-Json
Invoke-RestMethod "$base/api/mode" -Method Post -ContentType "application/json" -Body $body
```

预期净化为 2 档、新风为 1 档，而不是全部关闭。

### 10.4 验证环境突变报警

连续提交差异较大的手动环境样本：

```powershell
$body = @{ enabled = $true; temperature_c = 25; humidity_percent = 55; air_score = 85 } | ConvertTo-Json
Invoke-RestMethod "$base/api/environment" -Method Post -ContentType "application/json" -Body $body
$body = @{ enabled = $true; temperature_c = 33; humidity_percent = 25; air_score = 35 } | ConvertTo-Json
Invoke-RestMethod "$base/api/environment" -Method Post -ContentType "application/json" -Body $body
```

预期 `alarm_active=true`，并有 `alarm_reason`。确认报警：

```powershell
Invoke-RestMethod "$base/api/alarm/ack" -Method Post
```

## 11. 本阶段改动文件

```text
main/boards/bread-compact-wifi/smart_home_controller.h
main/boards/bread-compact-wifi/smart_home_controller.cc
main/boards/bread-compact-wifi/smart_home_http_server.h
main/boards/bread-compact-wifi/smart_home_http_server.cc
main/boards/bread-compact-wifi/compact_wifi_board.cc
tools/xiaozhi_mcp_bridge/smart_home_bridge.py
tools/xiaozhi_mcp_bridge/README.md
tests/test_bread_compact_wifi_regressions.py
tests/test_xiaozhi_mcp_bridge.py
docs/current-project-handoff.md
docs/continuation-notes.md
../文档/串口屏与环境监测项目交付说明.md
```

## 12. 验证结果

源码和桥接测试：

```text
python -m unittest discover -s tests -v
Ran 26 tests
OK
```

Python 语法检查：

```text
python -m py_compile tools/xiaozhi_mcp_bridge/smart_home_bridge.py tests/test_xiaozhi_mcp_bridge.py
通过
```

ESP-IDF 构建：

```text
powershell -ExecutionPolicy Bypass -File scripts/build_codex_check.ps1
Successfully created esp32s3 image.
xiaozhi.bin binary size 0x247910 bytes.
Smallest app partition is 0x3f0000 bytes.
0x1a86f0 bytes (42%) free.
```

构建中只有上游 `esp_video` 与 lwIP 的 `_IO/_IOR/_IOW` 重定义警告，没有编译错误。

## 13. 下一阶段必须确认

1. 蜂鸣器型号、有源/无源、工作电压和最大电流。
2. 雷达型号、数字 OUT 或 UART 接口、检测距离和供电。
3. LED 是小指示灯还是实际照明负载，电压和电流是多少。
4. 光敏使用 LDR 模拟量还是数字光照传感器。
5. 四个新硬件最终 GPIO 分配和电源方案。
6. 门口雷达只做“靠近触发开麦”，还是需要可靠判断进出方向/人数。
7. 定时语音播报采用云端定时任务、协议扩展还是固定录音。

硬件信息确认后，下一阶段再新增真实驱动、接线表、去抖/超时参数和实机验证步骤。
