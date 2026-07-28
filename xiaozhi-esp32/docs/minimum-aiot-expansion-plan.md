# 小智 AIoT 最小可实行扩展方案

日期：2026-06-09

适用工程：

```text
E:/espwork/xiaozhi-esp32/xiaozhi-esp32
```

适用板型：

```text
main/boards/bread-compact-wifi
```

当前 HMI 工程：

```text
D:/QQ/serial_warm_home .HMI
```

## 1. 当前工程状态

当前板子已经部署小智 AI 框架，工程使用 `CONFIG_BOARD_TYPE_BREAD_COMPACT_WIFI=y`，目标芯片为 `esp32s3`。

当前已经接入或预留的能力如下：

| 模块 | 当前状态 | 关键文件 |
| --- | --- | --- |
| 小智对话 | 已保留原框架的按键、音频、联网和对话流程 | `main/application.*`、`main/boards/bread-compact-wifi/compact_wifi_board.cc` |
| 语音输入输出 | `config.h` 已配置 I2S 麦克风和功放引脚 | `main/boards/bread-compact-wifi/config.h` |
| 串口屏 | UART2 已接入，ESP32 只更新控件值和切页 | `serial_hmi.*`、`串口屏手动事件配置手册.md` |
| DHT11 | GPIO18 读取温湿度 | `dht11_sensor.*` |
| MQ135 | GPIO1 / ADC1_CH0 读取空气质量模拟值 | `mq135_sensor.*` |
| 智能家居事件 | 串口屏事件已能解析，但目前只打日志 | `CompactWifiBoard::ScreenEventTask()` |
| AI 控制 MCU | 工程已有 MCP 机制，但当前板型还没有自定义工具 | `main/mcp_server.*`、板级 `InitializeTools()` |

最近构建记录显示当前工程可编译：

```text
idf.py -B build_codex_check reconfigure
idf.py -B build_codex_check build
Project build complete.
```

## 2. 最小可实行目标

本阶段不要一上来追求完整智能家居系统。推荐先完成一个闭环：

1. 板子联网后自动同步时间，并在串口屏显示日期、时间。
2. 板子定时获取天气，并在串口屏显示当前天气和室外温度。
3. 用户可以通过语音询问当前室内温湿度、空气质量、时间、天气。
4. 用户可以通过语音或串口屏按钮控制“空气净化器、风扇、加湿器、自动模式”的软件状态。
5. 真实外设先不接高压负载，先用日志、屏幕状态、低压 LED 或低压小风扇验证。

这套方案的好处是：硬件风险小，队友只要拿板子就能逐步验证；后面要接继电器、MQTT、更多传感器时，可以直接在这个基础上扩展。

## 3. 现在推荐加入的功能

### 3.1 联网时间同步

优先级：必须加。

最小方案：

- 使用 ESP-IDF 的 SNTP 能力，在 Wi-Fi 连接成功后同步系统时间。
- 设置中国时区为 `CST-8`。
- 时间同步成功后，每 1 秒或 5 秒刷新一次屏幕时间控件。
- 如果未同步成功，屏幕显示 `--:--`，不要阻塞小智对话和传感器任务。

建议新增控件名，均不超过 14 个字符：

| 控件名 | 页面 | 内容示例 |
| --- | --- | --- |
| `t_time` | `page0` 或 `page3` | `20:35` |
| `t_date` | `page3` | `06-09 Tue` |
| `t_sync` | `page3` | `TIME OK` 或 `NO TIME` |

实现建议：

- 新增 `time_weather_service.h/.cc` 或 `time_service.h/.cc`，不要把网络时间逻辑塞进 `SerialHmi`。
- `SerialHmi` 只负责 `SetText("t_time", "...")` 这种控件更新。
- 同步任务由 `CompactWifiBoard` 创建，等待网络可用后启动 SNTP。

参考依据：

- ESP-IDF 官方文档说明 SNTP 服务可通过 `esp_netif_sntp_init()` 初始化，并且该初始化通常只应执行一次，重复初始化前需要先 deinit。

### 3.2 天气获取与显示

优先级：必须加，但先做固定城市。

最小方案：

- 第一版固定一个城市或经纬度，不做定位。
- 使用 HTTP/HTTPS GET 请求天气 API。
- 每 30 分钟更新一次天气，失败时保留上一次成功结果。
- 屏幕只显示最少字段：天气、室外温度、更新时间。

推荐第一版 API：

- 演示阶段优先 Open-Meteo。
- 原因：无需 API Key，直接返回 JSON，适合先跑通链路。
- 后续如果要中文天气现象和国内城市编码，可以切到高德天气、和风天气等带 Key 的服务。

Open-Meteo 示例：

```text
https://api.open-meteo.com/v1/forecast?latitude=30.67&longitude=104.06&current=temperature_2m,weather_code&timezone=Asia%2FShanghai
```

建议新增控件名：

| 控件名 | 页面 | 内容示例 |
| --- | --- | --- |
| `t_weather` | `page0` 或 `page3` | `Sunny` |
| `t_out_temp` | `page0` 或 `page3` | `29 C` |
| `t_w_update` | `page3` | `20:30` |

注意：

- 串口屏中文要经过现有 UTF-8 到 GBK 转换。第一版天气文案建议短一些，例如 `晴`、`多云`、`小雨`，或直接用英文 `Sunny`、`Cloudy`，避免字库缺字。
- API 请求失败不能影响传感器采集和语音对话。
- 天气 API 的城市、经纬度、Key 不建议写死在多个文件里。第一版可以先写在板型配置常量里，后续迁移到 NVS `Settings`。

参考依据：

- Open-Meteo 官方文档说明 `/v1/forecast` 使用经纬度请求天气数据，返回 JSON；非商业基础使用不需要 API Key。

### 3.3 语音交互

优先级：当前已有，先验证，不急着重写。

当前 `compact_wifi_board.cc` 已经配置：

| 操作 | 当前逻辑 |
| --- | --- |
| BOOT 单击 | 启动阶段进入配网，正常阶段切换聊天状态 |
| GPIO47 按下 | `Application::StartListening()` |
| GPIO47 松开 | `Application::StopListening()` |
| 音量加减 | 调整 `AudioCodec` 输出音量 |

最小验证目标：

1. 队友先确认麦克风和功放硬件能完成小智正常对话。
2. 确认 GPIO47 按住说话、松开停止是否正常。
3. 如果需要串口屏上也有“按住说话”，后续再给 HMI 增加触摸热区事件。

不建议第一版就做：

- 不建议先改唤醒词模型。
- 不建议先做复杂连续对话状态 UI。
- 不建议把语音开始/停止逻辑直接写进串口屏驱动。

### 3.4 AI 控制 MCU

优先级：必须加，这是让“现有 AI 控制单片机”的关键。

推荐方案：使用小智工程现有 MCP 工具机制。

当前工程事实：

- `main/mcp_server.cc` 已经有通用工具，例如 `self.get_device_status`、`self.audio_speaker.set_volume`。
- `mcp_server.cc` 里明确提示：不要把自定义工具加到 `AddCommonTools()`，自定义工具必须加在板级 `InitializeTools()` 里。
- 其他板型已有示例，例如 `main/boards/common/lamp_controller.h` 用 `McpServer::AddTool()` 注册 `self.lamp.turn_on`、`self.lamp.turn_off`，回调里执行 GPIO 控制。

本项目建议增加 4 个最小工具：

| 工具名 | 用途 | 第一版动作 |
| --- | --- | --- |
| `self.env.get_status` | 让 AI 查询室内传感器、天气、时间、设备状态 | 返回 JSON 字符串 |
| `self.home.set_device` | 让 AI 控制净化器、风扇、加湿器 | 先修改软件状态并刷新屏幕 |
| `self.home.set_auto` | 让 AI 开关自动模式 | 先修改软件状态 |
| `self.screen.show_page` | 让 AI 控制串口屏跳转页面 | 调用 `SerialHmi::ShowNamedPage()` |

建议 `self.env.get_status` 返回内容：

```json
{
  "temperature_c": 26.5,
  "humidity_percent": 58,
  "mq135_raw": 820,
  "air_score": 75,
  "air_level": "good",
  "time": "20:35",
  "weather": "Sunny",
  "outdoor_temperature_c": 29,
  "devices": {
    "purifier": false,
    "fan": false,
    "humidifier": false,
    "auto": true
  }
}
```

建议 `self.home.set_device` 参数：

| 参数 | 类型 | 建议值 |
| --- | --- | --- |
| `target` | string | `purifier`、`fan`、`humidifier` |
| `action` | string | `on`、`off`、`toggle` |

第一版一定要先做“软件状态控制”：

- AI 调用工具后，更新内存里的设备状态。
- 刷新串口屏 page2 上对应状态。
- monitor 打印日志，例如 `AI control: fan=on`。
- 暂不接真实高压负载。

真实 GPIO 控制放到第二步：

- 软件状态验证稳定后，再给每个设备绑定一个 GPIO。
- GPIO 先驱动 LED 或低压继电器输入端。
- 最后再接低压风扇、灯带等安全负载。

## 4. 现有硬件与必须新增硬件

### 4.1 现有硬件需要确认

当前工程引脚占用如下，队友调试前必须确认硬件实物是否一致：

| 功能 | 引脚 |
| --- | --- |
| TJC 屏幕 TX/RX | GPIO41 / GPIO42 |
| DHT11 DATA | GPIO18 |
| MQ135 AO | GPIO1 / ADC1_CH0 |
| INMP441 麦克风 | GPIO4、GPIO5、GPIO6 |
| MAX98357A 功放 | GPIO7、GPIO15、GPIO16 |
| BOOT 按键 | GPIO0 |
| 触摸/说话按键 | GPIO47 |
| 音量加减 | GPIO40、GPIO39 |
| 板载 LED | GPIO48 |

如果当前小智对话已经能听和说，说明麦克风、功放、扬声器基础链路可用，不需要为“语音交互”额外加硬件。

### 4.2 不需要新增硬件的功能

以下功能理论上不需要新增硬件：

| 功能 | 说明 |
| --- | --- |
| 联网同步时间 | 使用已有 Wi-Fi |
| 天气获取 | 使用已有 Wi-Fi |
| 语音询问时间天气 | 使用已有小智对话能力 |
| AI 查询温湿度/空气质量 | 使用已有 DHT11、MQ135 |
| AI 控制虚拟设备状态 | 只更新状态、屏幕和日志 |

### 4.3 如果要真实控制外设，必须增加的硬件

如果比赛演示需要“AI 打开风扇/净化器/加湿器”有真实动作，至少需要增加：

| 硬件 | 数量 | 用途 | 注意 |
| --- | ---: | --- | --- |
| 低压 LED 模块或低压小风扇 | 1-3 个 | 第一阶段替代真实家电演示 | 推荐 5V 或 12V 低压负载 |
| MOSFET 驱动模块或继电器模块 | 1-3 路 | ESP32 GPIO 控制负载电源 | ESP32 不能直接带电机或继电器线圈 |
| 外部电源 | 1 个 | 给风扇、灯带、屏幕或继电器供电 | 必须与 ESP32 共地 |
| 杜邦线/端子/面包板 | 若干 | 调试连接 | 固定牢靠，避免短路 |
| 续流二极管或带保护的驱动模块 | 视负载而定 | 保护 GPIO 和驱动管 | 电机、继电器等感性负载必须考虑 |

不建议第一版直接控制 220V 交流电器。比赛演示阶段可以用低压风扇、LED 灯带、USB 小加湿器替代。

### 4.4 可选硬件

以下不是最小方案必须项：

| 硬件 | 是否推荐当前加入 | 原因 |
| --- | --- | --- |
| DS3231 RTC | 暂不推荐 | 有 Wi-Fi 时 SNTP 足够，RTC 只适合离线保持时间 |
| PMS5003 PM2.5 | 后续可加 | 当前 MQ135 只能做演示级空气质量，PM2.5 更直观 |
| SCD40/SCD41 CO2 | 后续可加 | 可提升环境监测专业度，但需要 I2C 引脚规划 |
| 光照/人体传感器 | 后续可加 | 可扩展自动模式，但不是当前闭环关键 |
| 红外发射模块 | 后续可加 | 可控制空调等家电，但协议适配工作量较大 |

## 5. 推荐软件结构

为了后续可维护，建议按模块拆开：

```text
main/boards/bread-compact-wifi/
  compact_wifi_board.cc          板级组装、任务创建、MCP 工具注册
  config.h                       引脚和基础参数
  serial_hmi.h/.cc               串口屏通信与控件刷新
  dht11_sensor.h/.cc             DHT11
  mq135_sensor.h/.cc             MQ135
  time_weather_service.h/.cc     新增：SNTP、天气 HTTP、缓存
  smart_home_control.h/.cc       新增：设备状态、可选 GPIO 输出
```

建议不要做的事：

- 不要把天气 HTTP 请求写进 `SerialHmi`。
- 不要在 `mcp_server.cc` 里添加本项目工具。
- 不要让 AI 直接控制任意 GPIO 编号。
- 不要把屏幕按钮控制、AI 控制、自动模式写成三套互不相通的逻辑。

推荐统一入口：

```text
串口屏按钮事件 -> SmartHomeControl::SetDevice(...)
AI MCP 工具 -> SmartHomeControl::SetDevice(...)
自动模式 -> SmartHomeControl::SetDevice(...)
```

这样同一个设备状态只维护一份，后续接真实 GPIO 时也只需要改 `SmartHomeControl`。

## 6. 队友调试步骤

### 6.1 基线验证

先不加新代码，确认当前固件和硬件基础正常：

1. 用当前工程构建。
2. 烧录到板子。
3. 打开 monitor。
4. 确认小智能正常联网和对话。
5. 确认串口屏启动后显示 page0。
6. 点击 HMI 图标，确认能切换 page1、page2、page3。
7. 确认 DHT11 温湿度刷新。
8. 确认 MQ135 原始值和空气等级刷新。
9. 点击 page2 的设备按钮，monitor 应出现 `Control event reserved for later integration`。

推荐命令：

```powershell
& 'D:\esp\Espressif\frameworks\esp-idf-v5.5.2\export.ps1'
idf.py -B build_codex_check build
idf.py -B build_codex_check -p COMx flash monitor
```

不要运行 `idf.py set-target` 或 `menuconfig`，除非团队明确决定要改受保护配置。

### 6.2 时间同步验证

加时间功能后验证：

1. monitor 打印 Wi-Fi connected。
2. SNTP 初始化只执行一次。
3. 时间同步前屏幕显示 `--:--`。
4. 时间同步成功后屏幕显示当前北京时间。
5. 断网时不崩溃，保留上一次时间或显示 `NO TIME`。

建议日志：

```text
[TIME] SNTP start
[TIME] sync ok: 2026-06-09 20:35:10
[TJC] t_time.txt="20:35"
```

### 6.3 天气验证

加天气功能后验证：

1. 先在电脑浏览器打开天气 API URL，确认能返回 JSON。
2. 板子上电后等待 Wi-Fi 连接。
3. monitor 打印天气请求 URL、HTTP 状态码、解析结果。
4. 屏幕显示天气和室外温度。
5. 故意断网，确认不会影响小智对话以外的本地任务。

建议日志：

```text
[WEATHER] GET forecast
[WEATHER] http=200 temp=29 weather_code=1
[TJC] t_weather.txt="Sunny"
[TJC] t_out_temp.txt="29 C"
```

### 6.4 MCP 工具验证

加 MCP 工具后验证顺序：

1. 先只加 `self.env.get_status`，不要先接 GPIO。
2. monitor 中打印工具注册日志。
3. 问小智：“当前室内温湿度是多少？”
4. 如果 AI 调用了工具，monitor 打印 `MCP env.get_status called`。
5. 小智回答中应包含 DHT11 或 MQ135 的实际数据。
6. 再加 `self.home.set_device`。
7. 问小智：“打开风扇。”
8. monitor 打印 `AI control: fan=on`，屏幕 page2 状态变化。

如果 AI 没有调用工具：

- 检查工具名和描述是否清楚。
- 检查后端是否完成 MCP `tools/list`。
- 检查小智服务端配置是否支持设备 MCP。
- 先用日志确认工具注册成功，再判断自然语言调用问题。

### 6.5 真实外设验证

真实控制必须按以下顺序：

1. 只接 LED 或万用表，不接负载。
2. AI 控制时确认 GPIO 电平变化。
3. 接 MOSFET/继电器输入端，不接负载。
4. 确认继电器或驱动模块动作。
5. 接低压负载，例如 5V 小风扇或 LED 灯带。
6. 观察供电是否稳定，确认 ESP32 不重启。
7. 最后再考虑更复杂负载。

禁止事项：

- 禁止 ESP32 GPIO 直接驱动电机、继电器线圈、大电流灯带。
- 禁止第一版直接控制 220V 交流负载。
- 禁止在没有共地和电平确认的情况下接外部模块。

## 7. 第一阶段任务拆分

建议按下面顺序推进：

| 阶段 | 内容 | 通过标准 |
| --- | --- | --- |
| A | 基线实机验证 | 语音、屏幕、DHT11、MQ135 都正常 |
| B | 增加 SNTP 时间服务 | 屏幕显示北京时间，断网不崩溃 |
| C | 增加天气服务 | 屏幕显示天气和室外温度 |
| D | 增加 `self.env.get_status` | 小智能回答当前传感器数据 |
| E | 增加软件设备状态控制 | AI 和屏幕按钮都能改同一份状态 |
| F | 增加低压真实输出 | LED 或低压风扇能被 AI 控制 |

完成 A-E 就已经可以作为最小 AIoT 演示闭环。F 是真实控制演示增强项。

## 8. 后续优化路线

在最小闭环稳定后，再按优先级优化：

1. 把城市、天气 API Key、更新间隔放入 NVS 配置。
2. 在串口屏 page3 增加时间、天气、网络状态、AI 状态。
3. 在 page2 增加每个设备的开关状态显示。
4. 增加自动模式规则，例如空气评分低于 60 自动打开净化器。
5. 增加 PM2.5 或 CO2 传感器，减少 MQ135 演示级估算的不确定性。
6. 增加 Home Assistant、MQTT 或云端同步。
7. 增加保护策略，例如设备最短开关间隔、上电默认关闭、异常断网保持本地控制。

## 9. 当前风险

| 风险 | 影响 | 处理建议 |
| --- | --- | --- |
| 板子不在当前开发者手上 | 无法直接实机验证 | 文档给队友按阶段验证，并要求保存 monitor 日志 |
| 天气 API 网络或证书失败 | 天气显示失败 | 失败时显示 `--`，不影响其他功能 |
| AI 不主动调用 MCP 工具 | 语音控制不生效 | 检查工具注册、工具描述、后端 MCP 支持 |
| MQ135 不准确 | 空气质量结果只能演示 | 文档中明确“演示级”，后续换 PM2.5/CO2 |
| 串口屏控件缺失 | 时间天气无法显示 | 先在 HMI 里补 `t_time`、`t_weather` 等控件 |
| GPIO 选错或冲突 | 外设不动作或影响音频/屏幕 | 新增控制引脚前先核对 `config.h` 和实物接线 |

## 10. 交付给队友时需要一起发的材料

必须发：

```text
E:/espwork/xiaozhi-esp32/xiaozhi-esp32/docs/minimum-aiot-expansion-plan.md
D:/Project/VSCode-project/xiaozhi-esp32/文档/硬件连接与软件验证步骤.md
D:/Project/VSCode-project/xiaozhi-esp32/文档/串口屏手动事件配置手册.md
D:/Project/VSCode-project/xiaozhi-esp32/文档/串口屏与环境监测项目交付说明.md
D:/QQ/serial_warm_home .HMI
```

建议同时发：

```text
E:/espwork/xiaozhi-esp32/xiaozhi-esp32/docs/current-project-handoff.md
E:/espwork/xiaozhi-esp32/xiaozhi-esp32/docs/project-file-map.md
E:/espwork/xiaozhi-esp32/xiaozhi-esp32/main/boards/bread-compact-wifi/serial_hmi_widgets.json
```

## 11. 外部参考

- ESP-IDF ESP-NETIF/SNTP 文档：`https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/network/esp_netif_programming.html`
- Open-Meteo Forecast API 文档：`https://open-meteo.com/en/docs`
