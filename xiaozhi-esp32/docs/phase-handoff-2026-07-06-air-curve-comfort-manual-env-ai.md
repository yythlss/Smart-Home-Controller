# 2026-07-06 空气曲线、舒适度、手动环境和 AI 控制阶段交付

本文档记录本阶段在 `E:/espwork/xiaozhi-esp32/xiaozhi-esp32` 的 `bread-compact-wifi` 板型上完成的改动，供队友接续开发和实机验证使用。

## 1. 本阶段目标

用户反馈：

- 串口屏 `c_air` 空气质量曲线一直没有显示。
- `t_comfort` 环境舒适度没有真正用上。
- 当前空气传感器只有空气质量评分，没有 PM2.5、CO2、TVOC 浓度，应按现有传感器能力调整。
- 自动模式需要可测试的手动输入数据模式，小程序和串口屏都要能操作。
- AI 语音输入控制能力需要完善，建议、空调、加湿器、净化器等语义要能通过工具承接。
- 图片中已有的 `t_advice`、`t_ai_state`、`t_air_score`、`t_air_state`、`t_air_raw`、`t_comfort`、`t_temp_d`、`t_humi_d`、`c_air` 都要有固件侧代码支持。

## 2. 当前 HMI 文件

本阶段确认用户提供的 HMI 文件为：

```text
D:/QQ/serial_warm_home  (1).HMI
```

只读扫描结论：

```text
文件大小：8406019 字节
最后修改时间：2026-07-06 21:00:17
已确认存在控件名：t_ai_state、t_temp、t_humi、t_air_state、t_advice
已确认 page1 相关控件名：t_air_score、t_air_state、t_air_raw、t_comfort、t_temp_d、t_humi_d、c_air
c_air 数字 ID：12
```

本阶段没有直接修改 `.HMI` 二进制文件。所有 HMI 页面、热区和事件仍需要在 USART HMI 编辑器中人工保存、编译、下载。

## 3. 代码改动

### 3.1 空气质量曲线

改动文件：

```text
main/boards/bread-compact-wifi/serial_hmi.h
main/boards/bread-compact-wifi/serial_hmi.cc
main/boards/bread-compact-wifi/compact_wifi_board.cc
```

当前逻辑：

1. `SerialHmi` 内部缓存最近 30 条空气评分。
2. 每次有新传感器/手动环境数据时，先写入缓存。
3. 只有当前页面是 `page1` 空气详情页时，才对 `c_air` 发送曲线命令。
4. 进入或刷新 `page1` 时，先清空曲线，再回放历史。

实际发送命令：

```text
cle 12,0
add 12,0,<air_score>
```

这样可以避免传感器在首页或其他页面刷新时，`add 12,0,val` 被 HMI 忽略，导致切到 page1 后曲线为空。

### 3.2 舒适度和建议规则

改动文件：

```text
main/boards/bread-compact-wifi/smart_home_controller.h
main/boards/bread-compact-wifi/smart_home_controller.cc
main/boards/bread-compact-wifi/serial_hmi.cc
main/boards/bread-compact-wifi/compact_wifi_board.cc
```

当前舒适度规则在 `SmartHomeController::ComfortDescription()` 中统一计算：

```text
舒适：温度 22-28 C，湿度 40-65%，空气评分 >= 80
空气较差：有 MQ135/手动空气数据，air_score < 40
偏冷：temperature_c < 18
偏热：temperature_c > 30
偏干：humidity_percent < 35
偏湿：humidity_percent > 75
空气一般：air_score < 65
基本舒适：未触发以上异常，但未达到舒适标准
```

当前建议规则在 `AdviceForEnvironment()` 中统一计算：

```text
空气差：开净化器和新风
高温：开空调降温
低温：开空调升温
干燥：开加湿器
湿度高：开新风除湿
空气一般：保持通风
舒适：环境舒适
```

串口屏和小程序不再各自计算舒适度，只显示控制器输出的 `comfort` 和 `advice`。

### 3.3 手动环境模式

改动文件：

```text
main/boards/bread-compact-wifi/smart_home_controller.h
main/boards/bread-compact-wifi/smart_home_controller.cc
main/boards/bread-compact-wifi/smart_home_http_server.h
main/boards/bread-compact-wifi/smart_home_http_server.cc
main/boards/bread-compact-wifi/serial_hmi.h
main/boards/bread-compact-wifi/serial_hmi.cc
main/boards/bread-compact-wifi/compact_wifi_board.cc
```

新增能力：

- `SetManualEnvironmentMode(bool enabled)`
- `SetManualEnvironment(float temperature_c, float humidity_percent, int air_score, int mq135_raw = -1)`
- `SetEnvironmentPreset(const char* preset)`
- `HandleEnvironmentAction(const char* target, const char* action)`

预设场景：

```text
GOOD      26 C / 55% / 88 分
HOT       33 C / 58% / 72 分
DRY       25 C / 28% / 76 分
POLLUTED  27 C / 60% / 28 分 / MQ135 raw 2400
```

手动模式开启后，真实传感器采样不会覆盖当前手动样本；关闭后，下一个 5 秒采样周期恢复真实传感器数据。

### 3.4 HTTP API

改动文件：

```text
main/boards/bread-compact-wifi/smart_home_http_server.h
main/boards/bread-compact-wifi/smart_home_http_server.cc
```

新增接口：

```text
POST /api/environment
```

请求示例：

```json
{"enabled":true,"temperature_c":30,"humidity_percent":55,"air_score":30}
```

```json
{"enabled":true,"preset":"POLLUTED"}
```

```json
{"enabled":false}
```

返回仍然是完整状态 JSON，新增字段包括：

```text
manual_environment_mode
environment_source
air_state
comfort
advice
```

### 3.5 小程序

改动文件：

```text
docs/mini_program_demo/pages/index/index.js
docs/mini_program_demo/pages/index/index.wxml
docs/mini_program_demo/pages/index/index.wxss
docs/mini_program_demo/README.md
```

新增界面：

- 环境状态中显示空气等级、舒适度、环境建议、数据来源。
- 新增“手动输入数据”区域。
- 可输入温度、湿度、空气评分。
- 可点击舒适、高温、干燥、污染预设。
- 可点击“恢复传感器”退出手动模拟。

新增小程序方法：

```text
onManualInput()
setManualEnvironment()
setEnvironmentPreset()
disableManualEnvironment()
```

### 3.6 串口屏事件

改动文件：

```text
main/boards/bread-compact-wifi/serial_hmi_widgets.json
../文档/串口屏手动事件配置手册.md
```

新增事件：

```text
BTN,ENV,MANUAL,TOGGLE
BTN,ENV,SCENE,GOOD
BTN,ENV,SCENE,HOT
BTN,ENV,SCENE,DRY
BTN,ENV,SCENE,POLLUTED
```

推荐放在 `page3` AI 与设置页：

| 热区名 | 事件 | 作用 |
| --- | --- | --- |
| `hs_env_m` | `BTN,ENV,MANUAL,TOGGLE` | 手动环境开关 |
| `hs_env_good` | `BTN,ENV,SCENE,GOOD` | 舒适场景 |
| `hs_env_hot` | `BTN,ENV,SCENE,HOT` | 高温场景 |
| `hs_env_dry` | `BTN,ENV,SCENE,DRY` | 干燥场景 |
| `hs_env_bad` | `BTN,ENV,SCENE,POLLUTED` | 污染场景 |

所有热区名均不超过 14 个字符。

### 3.7 AI/MCP 工具

改动文件：

```text
main/boards/bread-compact-wifi/smart_home_controller.cc
```

新增或强化工具：

```text
self.home.get_state
self.home.set_manual_environment
self.home.set_environment_preset
self.home.get_advice
```

语义覆盖：

- “空气太差/空气不好” -> 可触发净化、新风或查询建议。
- “模拟高温/太热了” -> 可设置 HOT 预设，建议开空调降温。
- “空气太干” -> 可设置 DRY 预设，建议开加湿器。
- “现在环境怎么样/给我建议” -> 可调用 `self.home.get_advice`。

## 4. 测试和验证结果

本阶段已完成以下本地验证：

```powershell
python -m unittest discover -s tests -v
```

结果：

```text
Ran 15 tests
OK
```

```powershell
python -m json.tool main/boards/bread-compact-wifi/serial_hmi_widgets.json
```

结果：JSON 解析通过。

```powershell
node --check docs/mini_program_demo/pages/index/index.js
```

结果：无语法错误。

```powershell
git status --short -- .vscode sdkconfig sdkconfig.defaults sdkconfig.defaults.esp32 sdkconfig.defaults.esp32s3 CMakePresets.json
```

结果：无输出，受保护配置未被本阶段修改。

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build_codex_check.ps1
```

结果：

```text
Generated E:/espwork/xiaozhi-esp32/xiaozhi-esp32/build_codex_check/xiaozhi.bin
xiaozhi.bin binary size 0x245af0 bytes
Smallest app partition is 0x3f0000 bytes
0x1aa510 bytes (42%) free
```

## 5. 仍需人工完成

### 5.1 HMI 编辑器

在 USART HMI 编辑器中打开：

```text
D:/QQ/serial_warm_home  (1).HMI
```

逐项确认：

```text
[ ] page1 存在 c_air 曲线控件
[ ] c_air 数字 ID 是 12
[ ] c_air 通道为 0 或单通道
[ ] page1 存在 t_air_score / t_air_state / t_air_raw / t_comfort / t_temp_d / t_humi_d
[ ] page0 存在 t_advice / t_ai_state
[ ] page3 增加 hs_env_m / hs_env_good / hs_env_hot / hs_env_dry / hs_env_bad
[ ] 每个 hs_env_* 的弹起事件写在“事件 -> 弹起事件(0)”
[ ] 每条事件后都有 printh 0a
[ ] 保存 HMI 工程
[ ] 编译 HMI 工程
[ ] 下载到串口屏
```

### 5.2 串口屏事件脚本

手动环境开关：

```text
prints "BTN,ENV,MANUAL,TOGGLE",0
printh 0a
```

舒适：

```text
prints "BTN,ENV,SCENE,GOOD",0
printh 0a
```

高温：

```text
prints "BTN,ENV,SCENE,HOT",0
printh 0a
```

干燥：

```text
prints "BTN,ENV,SCENE,DRY",0
printh 0a
```

污染：

```text
prints "BTN,ENV,SCENE,POLLUTED",0
printh 0a
```

### 5.3 实机验证

烧录当前固件后验证：

```text
[ ] page1 打开后 monitor 出现 [TJC] cle 12,0
[ ] page1 打开后 monitor 出现 [TJC] add 12,0,<score>
[ ] page1 曲线有点或趋势线变化
[ ] 串口屏点击污染预设后 monitor 出现 Screen event: raw=BTN,ENV,SCENE,POLLUTED
[ ] t_comfort 显示空气较差/偏热/偏干等描述
[ ] t_advice 显示开净化器和新风/开空调降温/开加湿器等建议
[ ] 小程序能 POST /api/environment 并显示手动模拟
[ ] 自动模式开启时，POLLUTED 预设会提高净化和新风档位，DRY 预设会提高加湿档位
[ ] MCP/语音侧能调用 self.home.get_advice 或环境预设工具
```

## 6. 遗留风险

- `.HMI` 文件仍需要人工确认和下载；代码侧已经支持事件，但如果 HMI 热区事件没写对，ESP32 收不到 `BTN,ENV,...`。
- `c_air` 依赖数字 ID 12。如果编辑器里重新创建控件导致 ID 改变，必须同步修改 `serial_hmi.cc` 的 `kAirCurveId`。
- MQ135 当前仍是演示级空气评分，不是 PM2.5、CO2、TVOC 浓度检测。
- 自动模式阈值是当前演示规则，后续接入更完整传感器后需要重新标定。
- 语音控制依赖小智框架能正确选择 MCP 工具，实机语音识别效果仍需烧录后验证。

## 7. 下一阶段建议

1. 先按 `串口屏手动事件配置手册.md` 完成 HMI 热区和事件下载。
2. 烧录 ESP32 固件，验证 `c_air` 曲线、`t_comfort`、`t_advice`、`BTN,ENV,...`。
3. 用 `docs/mini_program_demo/README.md` 的第 3.6、6.8-6.11 节验证小程序手动环境和自动模式联动。
4. 实机测试 AI 语音控制是否能调用新增 MCP 工具。
5. 如果曲线仍不显示，先记录 HMI 中 `c_air` 的真实数字 ID，再决定改 HMI 还是改固件常量。

## 8. 2026-07-06 暂停记录

用户已决定今天先记录，明天继续。当前暂停点是：串口屏 HMI 还没手动改完，下一次优先在 USART HMI 编辑器里处理 `page3` 手动环境触摸热区和背景提示。

明天继续时先做这几步：

1. 打开 `D:/QQ/serial_warm_home  (1).HMI`，建议先另存副本。
2. 进入 `page3`。
3. 检查 `page3` 上方已有 `t_ai_state` 和 `t_link_state`，不要覆盖这两个文本控件。
4. 在 `page3` 底部增加 5 个触摸热区：

| 热区名 | 建议坐标 | 事件 |
| --- | --- | --- |
| `hs_env_m` | `x=36,y=184,w=84,h=42` | `BTN,ENV,MANUAL,TOGGLE` |
| `hs_env_good` | `x=132,y=184,w=70,h=42` | `BTN,ENV,SCENE,GOOD` |
| `hs_env_hot` | `x=214,y=184,w=70,h=42` | `BTN,ENV,SCENE,HOT` |
| `hs_env_dry` | `x=296,y=184,w=70,h=42` | `BTN,ENV,SCENE,DRY` |
| `hs_env_bad` | `x=378,y=184,w=70,h=42` | `BTN,ENV,SCENE,POLLUTED` |

返回首页热区继续保持：

```text
hs_back: x=10,y=8,w=120,h=42
```

背景图片处理结论：

- 2026-07-07 已补充新版背景 `page3_ai_settings_hmi_manual_env.png`，底部已画入 `手动/舒适/高温/干燥/污染` 五个按钮。
- 仍需在 USART HMI 编辑器里创建透明触摸热区覆盖这些按钮；背景图只负责显示，事件必须在热区的 `事件 -> 弹起事件(0)` 中填写。
- 如果不导入新版背景，透明热区仍能触发功能，但用户和队友看不出点哪里；正式交付建议使用新版背景。

明天验证时 monitor 至少应看到：

```text
Screen event: raw=BTN,ENV,MANUAL,TOGGLE
Screen event: raw=BTN,ENV,SCENE,GOOD
Screen event: raw=BTN,ENV,SCENE,HOT
Screen event: raw=BTN,ENV,SCENE,DRY
Screen event: raw=BTN,ENV,SCENE,POLLUTED
```
