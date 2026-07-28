# bread-compact-wifi 智能家居外设接线与验证步骤

> 版本提示：本文主体记录 2026-07-05 至 2026-07-06 的连续旋转舵机阶段。当前风扇源码已恢复为 180°角度舵机扇叶；涉及风扇脉宽、停止点和连续旋转的段落仅作历史参考。最新自动规则、新硬件和 AI 接入说明以 `docs/phase-handoff-2026-07-14-smart-sensing-ai-automation.md` 为准。

本文档给当前 `bread-compact-wifi` 板型的智能家居演示硬件使用，重点覆盖 2026-07-05 新增的整套功能：智能家居执行器、串口屏事件、自动/节能模式、空气质量曲线、MCP 工具、局域网 HTTP API 和微信小程序演示工程。文档也包含净化 LED、加湿 LED、360°连续旋转舵机风扇、TJC 串口屏、DHT11 和 MQ135 的线路连接、上电检查、固件烧录、串口监视和实机验证步骤。

适用工程：

```text
E:/espwork/xiaozhi-esp32/xiaozhi-esp32
```

适用板型目录：

```text
main/boards/bread-compact-wifi
```

当前固件控制逻辑：

- `GPIO13`：空气净化器红色 LED，PWM 亮度表示档位。
- `GPIO14`：加湿器蓝色 LED，PWM 亮度表示档位。
- `GPIO21`：360°连续旋转舵机风扇，50Hz PWM 脉宽表示停止和转速。
- `GPIO41/GPIO42`：TJC/USART HMI 串口屏 UART2。
- `GPIO18`：DHT11 温湿度。
- `GPIO1 / ADC1_CH0`：MQ135 模拟空气质量输入。
- `SmartHomeController`：统一管理净化、新风/风扇、加湿、自动、节能、MCP 工具、环境历史和空气质量曲线。
- `SmartHomeHttpServer`：在局域网内提供 `8080` 端口 HTTP API，供小程序读取状态和控制设备。

## 0. 本文覆盖的 2026-07-05 新增功能

| 功能 | 验证入口 | 验证现象 |
| --- | --- | --- |
| 净化器控制 | 串口屏 / MCP / HTTP / 小程序 | `GPIO13` 红 LED 按 `关 -> 低 -> 中 -> 高 -> 关` 循环 |
| 新风/风扇控制 | 串口屏 / MCP / HTTP / 小程序 | `GPIO21` 连续旋转舵机风扇按 `关 -> 低 -> 中 -> 高 -> 关` 循环 |
| 加湿器控制 | 串口屏 / MCP / HTTP / 小程序 | `GPIO14` 蓝 LED 按 `关 -> 低 -> 中 -> 高 -> 关` 循环 |
| 自动模式 | 串口屏 / MCP / HTTP / 小程序 | 根据 DHT11/MQ135 读数自动调整净化、风扇、加湿 |
| 节能模式 | 串口屏 / MCP / HTTP / 小程序 | 关闭自动模式，并关闭三类执行器 |
| 空气质量曲线 | TJC 串口屏 `c_air` | 固件缓存最近 30 条评分，进入 page1 后 `cle 12,0` 并回放 `add 12,0,val` |
| 环境历史数据 | HTTP `/api/history` / 小程序 | 返回最近最多 30 条环境采样 |
| 当前状态查询 | MCP `self.home.get_state` / HTTP `/api/state` / 小程序 | 返回设备档位、模式、温湿度、MQ135 和空气评分 |
| 手动模拟环境 | 串口屏 / MCP / HTTP `/api/environment` / 小程序 | 模拟舒适、高温、干燥、污染，用于验证自动模式、舒适度和建议 |
| 小程序方案 A | 微信开发者工具 | 局域网读取状态、历史数据并远程控制设备 |

推荐整体验证顺序：

1. 先做接线和上电前检查。
2. 构建固件并烧录 ESP32S3。
3. 打开 monitor，确认启动日志和传感器日志。
4. 验证 TJC 串口屏触摸事件是否能进入 ESP32。
5. 验证 `GPIO13/GPIO14/GPIO21` 三类执行器。
6. 验证自动模式和节能模式。
7. 验证空气质量曲线和环境历史。
8. 验证手动模拟环境和自动模式联动。
9. 验证 HTTP API。
10. 验证小程序演示工程。
11. 如有条件，再验证 MCP 语音/工具控制。

## 1. 开发和验证前必须先确认

1. 当前任务不需要修改 `.vscode/**`、`sdkconfig*`、`CMakePresets.json`、ESP-IDF 安装目录或全局环境变量。
2. 接线前先断开 ESP32S3、串口屏、舵机风扇和传感器电源。
3. 舵机风扇和串口屏优先使用稳定外部 5V 电源，不建议从 ESP32S3 的 GPIO 或 3.3V 引脚取电。
4. 所有外设如果使用独立电源，必须与 ESP32S3 `GND` 共地，否则 UART、PWM 和 ADC 信号会不稳定。
5. MQ135 模块的 `AO` 输出必须确认不超过 `3.3V`，超过会损坏 ESP32S3 ADC 引脚。
6. LED 必须串联限流电阻，建议 `220Ω-1kΩ`，不要让 GPIO 直接短接 LED 到 GND。

## 2. 总接线表

| 模块/外设 | 外设引脚 | ESP32S3 连接 | 电源建议 | 说明 |
| --- | --- | --- | --- | --- |
| TJC 串口屏 | RX | `GPIO41 / ESP32 TX` | 按屏幕规格接 5V 或指定电压 | ESP32 发命令到屏幕 |
| TJC 串口屏 | TX | `GPIO42 / ESP32 RX` | 同上 | 屏幕事件回传 ESP32 |
| TJC 串口屏 | GND | `GND` | 与 ESP32 共地 | 必须连接 |
| DHT11 | DATA | `GPIO18` | `3.3V` | 建议外接上拉电阻 |
| DHT11 | VCC | `3.3V` | `3.3V` | 模块版可按模块标注接线 |
| DHT11 | GND | `GND` | 共地 | 必须连接 |
| MQ135 | AO | `GPIO1 / ADC1_CH0` | 按模块规格 | AO 必须小于等于 3.3V |
| MQ135 | VCC | 模块电源正极 | 常见为 5V | 以手头模块标注为准 |
| MQ135 | GND | `GND` | 共地 | 必须连接 |
| 净化 LED | 正极 | `GPIO13` 串联电阻后接 LED | ESP32 GPIO | 红色 LED，PWM 亮度表示档位 |
| 净化 LED | 负极 | `GND` | 共地 | 低功耗指示用 |
| 加湿 LED | 正极 | `GPIO14` 串联电阻后接 LED | ESP32 GPIO | 蓝色 LED，PWM 亮度表示档位 |
| 加湿 LED | 负极 | `GND` | 共地 | 低功耗指示用 |
| 360°连续旋转舵机风扇 | 信号线 | `GPIO21` | 信号由 ESP32 输出 | 常见橙色/黄色线 |
| 360°连续旋转舵机风扇 | 正极 | 外部稳定 `5V` | 不建议由 GPIO/3.3V 供电 | 常见红线 |
| 360°连续旋转舵机风扇 | 负极 | `GND` | 与 ESP32 共地 | 常见棕/黑线 |

## 3. 推荐接线顺序

按以下顺序接线，便于排查问题。

### 3.1 先接公共地

1. ESP32S3 `GND` 接面包板地线。
2. 串口屏 `GND` 接同一条地线。
3. DHT11 `GND` 接同一条地线。
4. MQ135 `GND` 接同一条地线。
5. 舵机风扇外部 5V 电源负极接同一条地线。

检查点：

- 所有模块的 GND 最终必须导通。
- 如果舵机风扇用独立 5V 电源，只接 5V 和信号、不接共地，会导致风扇不受控或乱转。

### 3.2 再接串口屏

| ESP32S3 | 串口屏 |
| --- | --- |
| `GPIO41` | 屏幕 `RX` |
| `GPIO42` | 屏幕 `TX` |
| `GND` | 屏幕 `GND` |
| 外部电源正极 | 屏幕电源正极 |

检查点：

- TX/RX 必须交叉连接。
- 当前固件和 HMI 工程波特率都应为 `9600`。
- 如果屏幕 TX 是 5V TTL，建议给 `GPIO42` 加分压或电平转换。

### 3.3 接 DHT11

| DHT11 | ESP32S3 |
| --- | --- |
| `VCC` | `3.3V` |
| `DATA` | `GPIO18` |
| `GND` | `GND` |

建议：

- `DATA` 到 `3.3V` 加 `4.7kΩ-10kΩ` 上拉电阻。
- DHT11 读数慢，当前固件 5 秒采样一次，不要按刷新速度判断传感器是否损坏。

### 3.4 接 MQ135

| MQ135 | ESP32S3 |
| --- | --- |
| `VCC` | 按模块规格接 `5V` 或 `3.3V` |
| `AO` | `GPIO1 / ADC1_CH0` |
| `GND` | `GND` |

必须检查：

- 用万用表量 `AO` 到 `GND`，确认最大不超过 `3.3V`。
- 如果模块 AO 会输出 5V，需要先分压再接 ESP32S3。
- 当前 MQ135 只用于演示级空气评分，不是准确 ppm 测量。

### 3.5 接净化和加湿 LED

净化红色 LED：

```text
GPIO13 -> 限流电阻 -> LED 正极
LED 负极 -> GND
```

加湿蓝色 LED：

```text
GPIO14 -> 限流电阻 -> LED 正极
LED 负极 -> GND
```

建议：

- 限流电阻建议 `220Ω-1kΩ`。
- 如果 LED 不亮，先确认 LED 极性，再确认是否处于对应设备非 0 档。
- 如果 LED 一直很暗，换较小电阻或检查 GPIO 是否接错。

### 3.6 接 360°连续旋转舵机风扇

常见线色：

| 舵机风扇线色 | 连接 |
| --- | --- |
| 橙色/黄色 | `GPIO21` |
| 红色 | 外部稳定 `5V` |
| 棕色/黑色 | `GND`，并与 ESP32S3 共地 |

当前固件脉宽表：

| 档位 | 语义 | PWM 脉宽 |
| --- | --- | --- |
| `0` | 停止 | `1500us` |
| `1` | 低速 | `1600us` |
| `2` | 中速 | `1750us` |
| `3` | 高速 | `1900us` |

注意：

- 360°连续旋转舵机不是角度舵机，不能再按 `0°-180°` 去验证。
- `1500us` 是常见停止中位，但不同舵机可能有偏差。
- 如果 0 档仍缓慢转动，需要在 `SmartHomeController::FreshAirPulseUsForLevel()` 中微调 0 档脉宽。
- 如果方向相反，可以把 1/2/3 档改为低于中位的脉宽，例如 `1400us/1250us/1100us`，但要先确认扇叶安装方向和实际风向需求。

## 4. 上电前检查清单

接线完成后，不要急着上电，先逐项检查。

```text
[ ] ESP32S3、串口屏、舵机风扇、传感器全部共地
[ ] TJC 屏幕 TX/RX 已交叉连接：GPIO41->屏幕 RX，GPIO42<-屏幕 TX
[ ] TJC 屏幕供电符合屏幕规格
[ ] DHT11 DATA 接 GPIO18，并有上拉或模块自带上拉
[ ] MQ135 AO 接 GPIO1，且电压不超过 3.3V
[ ] GPIO13 红 LED 已串联限流电阻
[ ] GPIO14 蓝 LED 已串联限流电阻
[ ] 舵机风扇信号线接 GPIO21
[ ] 舵机风扇红线接稳定 5V，不接 GPIO
[ ] 舵机风扇 GND 与 ESP32S3 GND 共地
[ ] 没有把 5V 接到 ESP32S3 的 GPIO/ADC 输入
[ ] 面包板正负电源轨没有接反
```

## 5. HMI 屏幕事件准备

如果要通过串口屏验证设备控制，HMI 中 `page2` 的触摸热区需要能发送以下事件。

净化器：

```text
prints "BTN,DEVICE,AIR_PURIFIER,TOGGLE",0
printh 0a
```

风扇/新风：

```text
prints "BTN,DEVICE,FAN,TOGGLE",0
printh 0a
```

加湿器：

```text
prints "BTN,DEVICE,HUMIDIFIER,TOGGLE",0
printh 0a
```

自动模式：

```text
prints "BTN,MODE,AUTO,TOGGLE",0
printh 0a
```

节能模式：

```text
prints "BTN,MODE,ECO,TOGGLE",0
printh 0a
```

填写位置：

- 在 USART HMI 编辑器中选中触摸热区。
- 填到右侧 `事件 -> 弹起事件(0)`。
- 不要填到左侧 `输出`。
- 不要勾选 `发送键值`。

### 5.1 HMI 工程下载前检查

在把 HMI 工程下载到串口屏前，至少检查以下项目：

```text
[ ] HMI 工程波特率为 9600
[ ] page2 上有净化、风扇/新风、加湿、自动、节能触摸热区
[ ] hs_purifier 弹起事件为 BTN,DEVICE,AIR_PURIFIER,TOGGLE
[ ] hs_fan 弹起事件为 BTN,DEVICE,FAN,TOGGLE
[ ] hs_humid 弹起事件为 BTN,DEVICE,HUMIDIFIER,TOGGLE
[ ] hs_auto 弹起事件为 BTN,MODE,AUTO,TOGGLE
[ ] hs_eco 弹起事件为 BTN,MODE,ECO,TOGGLE
[ ] page3 上有 hs_env_m / hs_env_good / hs_env_hot / hs_env_dry / hs_env_bad 触摸热区
[ ] hs_env_m 弹起事件为 BTN,ENV,MANUAL,TOGGLE
[ ] hs_env_good 弹起事件为 BTN,ENV,SCENE,GOOD
[ ] hs_env_hot 弹起事件为 BTN,ENV,SCENE,HOT
[ ] hs_env_dry 弹起事件为 BTN,ENV,SCENE,DRY
[ ] hs_env_bad 弹起事件为 BTN,ENV,SCENE,POLLUTED
[ ] page1 或空气详情页存在 c_air 曲线控件
[ ] c_air 的数字 ID 仍为 12
```

特别注意：

- 2026-07-05 扫描结果显示，`c_air` 已确认存在，数字 ID 为 `12`。
- 当前固件只启用空气质量曲线：`add 12,0,val`。
- `c_temp`、`c_humi` 的数字 ID 尚未确认，固件暂时不会写温度/湿度曲线。
- 之前发现 `hs_eco` 可能仍发送 `BTN,MODE,AUTO,TOGGLE`，下载屏幕前必须改成 `BTN,MODE,ECO,TOGGLE`。

### 5.2 HMI 事件下载后验证

HMI 工程下载到屏幕后，打开 ESP32 monitor，逐个点击 page2 的热区。每点击一次，monitor 应出现类似日志。

净化：

```text
Screen event: raw=BTN,DEVICE,AIR_PURIFIER,TOGGLE target=AIR_PURIFIER action=TOGGLE
```

风扇/新风：

```text
Screen event: raw=BTN,DEVICE,FAN,TOGGLE target=FAN action=TOGGLE
```

加湿：

```text
Screen event: raw=BTN,DEVICE,HUMIDIFIER,TOGGLE target=HUMIDIFIER action=TOGGLE
```

自动：

```text
Screen event: raw=BTN,MODE,AUTO,TOGGLE target=AUTO action=TOGGLE
```

节能：

```text
Screen event: raw=BTN,MODE,ECO,TOGGLE target=ECO action=TOGGLE
```

如果屏幕有显示但 monitor 没有事件日志，优先检查 HMI 事件填写位置和屏幕工程是否重新下载成功。

## 6. 构建固件

先运行源码级回归测试：

```powershell
cd E:\espwork\xiaozhi-esp32\xiaozhi-esp32
python -m unittest discover -s tests -v
```

通过标准：

```text
Ran 10 tests
OK
```

在工程根目录执行：

```powershell
cd E:\espwork\xiaozhi-esp32\xiaozhi-esp32
powershell -ExecutionPolicy Bypass -File scripts/build_codex_check.ps1
```

构建成功时应看到类似输出：

```text
Successfully created esp32s3 image.
Generated E:/espwork/xiaozhi-esp32/xiaozhi-esp32/build_codex_check/xiaozhi.bin
```

最近一次验证结果：

```text
xiaozhi.bin binary size 0x243a40 bytes.
Smallest app partition is 0x3f0000 bytes.
0x1ac5c0 bytes (42%) free.
```

如果新增或删除过源文件后构建异常，可以先运行一次重新配置：

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build_codex_check.ps1 -Reconfigure
```

## 7. 烧录固件

优先使用 VSCode ESP-IDF 插件中当前已经配置好的烧录入口，避免手动改写 `sdkconfig` 或芯片目标。

推荐流程：

1. 用 VSCode 打开工程目录：

```text
E:/espwork/xiaozhi-esp32/xiaozhi-esp32
```

2. 确认底部 ESP-IDF 状态栏仍是当前工程和 `esp32s3`。
3. 选择正确串口。
4. 点击 ESP-IDF 的 Build/Flash/Monitor，或先 Flash 再 Monitor。
5. 不要随意运行 `idf.py set-target` 或 `menuconfig`。

如果必须用命令行烧录，应先确认端口号，例如 `COMx`，再使用当前 ESP-IDF 环境。不要为了烧录改动受保护配置文件。

## 8. 打开串口监视器后的启动检查

烧录后打开 monitor，先看启动日志。注意：HTTP API 已改为等 WiFi/lwIP 网络栈就绪后再启动，因此 `Mini program HTTP API started on port 8080` 不会在刚开机时立刻出现，必须等 WiFi 连接成功后才会出现。

刚开机阶段重点确认：

```text
Smart home controller initialized
```

WiFi 连接成功后再确认：

```text
Network connected, starting mini program HTTP API
Mini program HTTP API started on port 8080
```

如果有串口屏事件，还应看到类似：

```text
Screen event: raw=BTN,DEVICE,FAN,TOGGLE target=FAN action=TOGGLE
SmartHome: Apply fresh air fan output: gpio=21 level=1 pulse=1600us
SmartHome: Device action applied: target=FAN level=1
```

如果没有这些日志：

1. 确认固件确实烧录成功。
2. 确认当前板型是 `bread-compact-wifi`。
3. 确认串口监视器连接的是 ESP32S3 的 USB 串口，不是屏幕串口。
4. 确认串口屏事件写在 HMI 的弹起事件中，并且已下载到屏幕。

如果只有 `Screen event`，没有 `SmartHome: Apply ... output`，说明事件没有进入有效控制分支，需要继续查事件类型、`target` 和 `action`。

如果两类日志都有，但外设仍不动作，说明固件已经写 GPIO/PWM，优先检查外设接线、供电、共地、LED 极性、限流电阻和模块触发方式。

如果出现 `E BOD: Brownout detector was triggered`，说明供电电压跌落导致 ESP32 复位，应先按 16.4 处理供电，再继续功能验证。

## 9. 基础外设验证顺序

建议按以下顺序验证，便于定位问题。

### 9.1 只验证 ESP32 固件启动

不急着操作屏幕，先观察 monitor 是否稳定启动。

通过标准：

- 没有反复重启。
- 没有 brownout 或电压不足日志。
- 智能家居控制器初始化日志出现。

如果反复重启：

- 先拔掉舵机风扇和屏幕，只保留 ESP32S3。
- 如果拔掉后正常，优先检查 5V 供电和共地。

### 9.2 验证传感器刷新

等待至少 10 秒，因为当前传感器周期约 5 秒。

通过标准：

- monitor 中能看到 DHT11 或 MQ135 相关读数。
- 屏幕温湿度不一直是 `--`。
- 空气评分能随 MQ135 原始值变化。

如果 DHT11 偶尔失败：

- 这是单总线传感器常见情况，先看是否下一周期恢复。
- 检查 DATA 上拉。
- 缩短线长。

如果 MQ135 原始值一直为 0 或满量程：

- 检查 AO 是否接到 `GPIO1`。
- 检查 AO 电压范围。
- 检查 MQ135 模块是否需要预热。

### 9.3 验证红色净化 LED

通过屏幕 `hs_purifier` 或其他入口触发：

```text
BTN,DEVICE,AIR_PURIFIER,TOGGLE
```

预期循环：

```text
关闭 -> 低档 -> 中档 -> 高档 -> 关闭
```

现象：

- 低档：红 LED 较暗。
- 中档：红 LED 中等亮度。
- 高档：红 LED 最亮。
- 关闭：红 LED 熄灭。

异常处理：

- 完全不亮：检查 LED 极性、限流电阻、GPIO13 接线。
- 一直亮：检查是否误接到 3.3V/5V，或事件是否一直触发。
- 亮度无变化：检查是否接到 `GPIO13`，不要接错到普通电源脚。

### 9.4 验证蓝色加湿 LED

触发：

```text
BTN,DEVICE,HUMIDIFIER,TOGGLE
```

预期循环：

```text
关闭 -> 低档 -> 中档 -> 高档 -> 关闭
```

现象：

- 低档：蓝 LED 较暗。
- 中档：蓝 LED 中等亮度。
- 高档：蓝 LED 最亮。
- 关闭：蓝 LED 熄灭。

异常处理同净化 LED，但检查 `GPIO14`。

### 9.5 验证 360°连续旋转舵机风扇

触发：

```text
BTN,DEVICE,FAN,TOGGLE
```

预期循环：

```text
关闭 -> 低速 -> 中速 -> 高速 -> 关闭
```

当前脉宽：

| 状态 | 脉宽 | 预期 |
| --- | --- | --- |
| 关闭 | `1500us` | 风扇停止 |
| 低速 | `1600us` | 慢速连续转动 |
| 中速 | `1750us` | 中速连续转动 |
| 高速 | `1900us` | 快速连续转动 |

注意：

- 不能再期待它转到某个角度后停住。
- 不能再验证 `0°/90°/180°`。
- 0 档应该停止，但具体停止点可能需要校准。

如果 0 档仍转：

1. 记录当前现象，例如“0 档顺时针慢转”。
2. 修改 `SmartHomeController::FreshAirPulseUsForLevel()` 中 default 返回值。
3. 常见尝试顺序：

```text
1500us -> 1490us -> 1510us -> 1480us -> 1520us
```

4. 每次只改一个值，重新构建、烧录、验证。
5. 找到完全停止或最接近停止的脉宽后，再记录到交付文档。

如果低档不转：

1. 保持停止脉宽不变。
2. 适当提高 1 档，例如：

```text
1600us -> 1650us
```

3. 如果仍不转，确认舵机风扇供电是否足够。

如果高档太快或振动大：

1. 降低 3 档，例如：

```text
1900us -> 1850us -> 1800us
```

2. 检查扇叶是否松动或偏心。

如果方向相反：

1. 先确认扇叶安装方向是否正确。
2. 如果确实需要反向控制，把 1/2/3 档改到低于中位：

```text
1档 1400us
2档 1250us
3档 1100us
```

3. 改完后重新验证停止点，因为反向档位不应影响 0 档停止。

## 10. 自动模式和节能模式验证

### 10.1 自动模式

触发：

```text
BTN,MODE,AUTO,TOGGLE
```

预期：

- 自动模式开启。
- 如果湿度低于阈值，加湿档位会自动提高。
- 如果 MQ135 原始值较高，净化档位会提高。
- 如果温度较高或空气质量差，新风/风扇档位会提高。

注意：

- 自动模式依赖当前传感器读数，不一定每次触发都立即改变所有外设。
- 当前 MQ135 是演示级阈值，不代表精确浓度。

### 10.2 节能模式

触发：

```text
BTN,MODE,ECO,TOGGLE
```

预期：

- 自动模式关闭。
- 净化 LED 关闭。
- 加湿 LED 关闭。
- 舵机风扇回到停止脉宽。
- ESP32 不进入 deep sleep，语音、屏幕、HTTP API 和传感器仍保持工作。

## 11. MCP 语音/工具验证

当前注册工具：

```text
self.home.get_state
self.home.set_purifier
self.home.set_fresh_air
self.home.set_humidifier
self.home.set_auto
self.home.set_eco
```

建议验证顺序：

1. 先调用 `self.home.get_state`，确认能返回 JSON 状态。
2. 调用 `self.home.set_purifier`，参数 `power=true, level=1/2/3/0`，看红 LED。
3. 调用 `self.home.set_humidifier`，参数 `power=true, level=1/2/3/0`，看蓝 LED。
4. 调用 `self.home.set_fresh_air`，参数 `power=true, level=1/2/3/0`，看风扇。
5. 调用 `self.home.set_eco`，参数 `power=true`，确认全部执行器关闭。

如果 MCP 控制有效但屏幕控制无效，说明固件控制器正常，问题大概率在 HMI 事件没有写对或没有下载到屏幕。

## 12. HTTP API 验证

ESP32 联网后，monitor 中应出现：

```text
Mini program HTTP API started on port 8080: /api/state /api/history /api/device /api/mode
```

先找到 ESP32 的局域网 IP，然后浏览器访问：

```text
http://<ESP32_IP>:8080/api/state
```

能返回 JSON，说明 HTTP 服务正常。

### 12.1 查询当前状态 `/api/state`

浏览器或 PowerShell 均可验证。

浏览器：

```text
http://<ESP32_IP>:8080/api/state
```

PowerShell：

```powershell
Invoke-RestMethod -Method Get -Uri "http://<ESP32_IP>:8080/api/state"
```

返回中重点看这些字段：

```text
purifier_level
fresh_air_level
humidifier_level
auto_mode
eco_mode
has_temperature
temperature_c
has_humidity
humidity_percent
has_mq135_raw
mq135_raw
air_score
```

通过标准：

- 能返回 JSON。
- `purifier_level/fresh_air_level/humidifier_level` 都在 `0-3`。
- `auto_mode/eco_mode` 是布尔值。
- 接上传感器后，温湿度、MQ135 和空气评分字段能随采样刷新。

### 12.2 查询历史数据 `/api/history`

PowerShell：

```powershell
Invoke-RestMethod -Method Get -Uri "http://<ESP32_IP>:8080/api/history"
```

通过标准：

- 返回 JSON 中有 `count`、`capacity`、`samples`。
- `capacity` 应为 `30`。
- 等待 5 秒以上后再次请求，`samples` 中应有新采样。
- `samples` 顺序为旧到新，适合小程序画近期趋势。

如果 `count` 一直为 0：

1. 确认 sensor task 正常运行。
2. monitor 中应周期性出现 `---- Sensor Read ----`。
3. 至少等待一个 5 秒采样周期。

### 12.3 控制设备 `/api/device`

接口：

```text
POST http://<ESP32_IP>:8080/api/device
```

PowerShell 控制净化器二档：

```powershell
Invoke-RestMethod -Method Post -Uri "http://<ESP32_IP>:8080/api/device" -ContentType "application/json" -Body '{"device":"purifier","power":true,"level":2}'
```

预期：红 LED 变为中等亮度，`/api/state` 中 `purifier_level` 为 `2`。

PowerShell 控制加湿器三档：

```powershell
Invoke-RestMethod -Method Post -Uri "http://<ESP32_IP>:8080/api/device" -ContentType "application/json" -Body '{"device":"humidifier","power":true,"level":3}'
```

预期：蓝 LED 最亮，`/api/state` 中 `humidifier_level` 为 `3`。

PowerShell 控制风扇二档：

```powershell
Invoke-RestMethod -Method Post -Uri "http://<ESP32_IP>:8080/api/device" -ContentType "application/json" -Body '{"device":"fan","power":true,"level":2}'
```

JSON 内容等价于：

```json
{
  "device": "fan",
  "power": true,
  "level": 2
}
```

预期：舵机风扇中速旋转，`/api/state` 中 `fresh_air_level` 为 `2`。

PowerShell 关闭三个设备：

```powershell
Invoke-RestMethod -Method Post -Uri "http://<ESP32_IP>:8080/api/device" -ContentType "application/json" -Body '{"device":"purifier","power":false,"level":0}'
Invoke-RestMethod -Method Post -Uri "http://<ESP32_IP>:8080/api/device" -ContentType "application/json" -Body '{"device":"humidifier","power":false,"level":0}'
Invoke-RestMethod -Method Post -Uri "http://<ESP32_IP>:8080/api/device" -ContentType "application/json" -Body '{"device":"fan","power":false,"level":0}'
```

风扇关闭 JSON 示例：

```json
{
  "device": "fan",
  "power": false,
  "level": 0
}
```

通过标准：

- 控制净化时，红 LED 响应。
- 控制加湿时，蓝 LED 响应。
- 控制 `fan` 或 `fresh_air` 时，`GPIO21` 舵机风扇响应。
- 每次控制后再访问 `/api/state`，档位字段应一致。

支持的设备名：

```text
purifier
air_purifier
fresh_air
fan
humidifier
```

### 12.4 控制模式 `/api/mode`

接口：

```text
POST http://<ESP32_IP>:8080/api/mode
```

开启自动模式：

```powershell
Invoke-RestMethod -Method Post -Uri "http://<ESP32_IP>:8080/api/mode" -ContentType "application/json" -Body '{"mode":"auto","power":true}'
```

预期：

- `/api/state` 中 `auto_mode` 为 `true`。
- `eco_mode` 为 `false`。
- 后续传感器采样时，自动策略可能调整三类执行器。

开启节能模式：

```powershell
Invoke-RestMethod -Method Post -Uri "http://<ESP32_IP>:8080/api/mode" -ContentType "application/json" -Body '{"mode":"eco","power":true}'
```

JSON 内容等价于：

```json
{
  "mode": "eco",
  "power": true
}
```

预期：

- `/api/state` 中 `eco_mode` 为 `true`。
- `auto_mode` 为 `false`。
- `purifier_level/fresh_air_level/humidifier_level` 都变为 `0`。
- 红 LED、蓝 LED 熄灭，风扇停止。

关闭节能模式：

```powershell
Invoke-RestMethod -Method Post -Uri "http://<ESP32_IP>:8080/api/mode" -ContentType "application/json" -Body '{"mode":"eco","power":false}'
```

支持的模式名：

```text
auto
eco
```

### 12.5 HTTP API 常见问题

如果访问不到 `/api/state`：

1. 确认 ESP32 已连接 WiFi。
2. 确认 monitor 中出现 `Mini program HTTP API started on port 8080`。
3. 确认电脑和 ESP32 在同一个局域网。
4. 确认访问地址带端口 `8080`。
5. 确认没有把 ESP32 的配网页面端口 `80` 当成小程序 API 端口。

如果 `/api/state` 能访问但 `/api/device` 无效：

1. 确认 POST 请求 `Content-Type` 是 `application/json`。
2. 确认 JSON 里的 `device`、`power`、`level` 字段存在。
3. 控制后再次访问 `/api/state`，确认状态是否变化。
4. 如果状态变化但外设不动，再排查 GPIO 接线。

## 13. 微信小程序演示工程验证

小程序目录：

```text
docs/mini_program_demo
```

详细说明优先看：

```text
docs/mini_program_demo/README.md
```

本节给整机验证时使用，要求先确认 HTTP API，再验证小程序。不要一上来就用手机扫码，否则无法判断问题是在固件、网络、微信开发者工具，还是手机网络环境。

### 13.1 小程序验证前置条件

```text
[ ] ESP32S3 已烧录当前 bread-compact-wifi 固件
[ ] 串口 monitor 没有反复重启
[ ] 串口 monitor 没有持续出现 Brownout detector was triggered
[ ] ESP32S3 已连接 WiFi
[ ] monitor 出现 Network connected, starting mini program HTTP API
[ ] monitor 出现 Mini program HTTP API started on port 8080
[ ] 电脑和 ESP32S3 在同一个局域网
[ ] 微信开发者工具已安装
```

如需手机真机预览，再额外确认：

```text
[ ] 手机和 ESP32S3 在同一个 WiFi
[ ] 手机没有使用移动数据/VPN/代理绕开当前局域网
[ ] 路由器没有开启 AP 隔离、访客网络隔离或无线客户端隔离
```

### 13.2 记录 ESP32S3 地址

从串口日志、路由器后台或手机热点已连接设备列表中找到 ESP32S3 的 IP。记录格式建议如下：

```text
ESP32S3 IP：192.168.1.23
小程序填写地址：192.168.1.23:8080
电脑 IP：192.168.1.xx
手机 IP：192.168.1.xx
```

注意：

- 小程序输入框填 `<ESP32_IP>:8080`，例如 `192.168.1.23:8080`。
- 不要只填 `192.168.1.23`。
- 不要填配网页面的 `80` 端口。
- 不要把 `/api/state` 也填进小程序输入框。

### 13.3 小程序前先做 HTTP API 基础验证

电脑浏览器访问：

```text
http://<ESP32_IP>:8080/api/state
```

通过标准：

```text
[ ] 能打开页面
[ ] 返回 JSON
[ ] JSON 中包含 purifier_level
[ ] JSON 中包含 fresh_air_level
[ ] JSON 中包含 humidifier_level
[ ] JSON 中包含 auto_mode
[ ] JSON 中包含 eco_mode
[ ] JSON 中包含 air_score
```

PowerShell 验证：

```powershell
Invoke-RestMethod -Method Get -Uri "http://<ESP32_IP>:8080/api/state"
Invoke-RestMethod -Method Get -Uri "http://<ESP32_IP>:8080/api/history"
```

如果这两条都失败，先停在 HTTP API 排查，不要继续判断小程序。

### 13.4 HTTP API 控制预验证

净化器二档：

```powershell
Invoke-RestMethod -Method Post -Uri "http://<ESP32_IP>:8080/api/device" -ContentType "application/json" -Body '{"device":"purifier","power":true,"level":2}'
```

预期：

```text
[ ] 返回 JSON
[ ] purifier_level 变为 2
[ ] GPIO13 红色 LED 亮度变化
```

加湿器三档：

```powershell
Invoke-RestMethod -Method Post -Uri "http://<ESP32_IP>:8080/api/device" -ContentType "application/json" -Body '{"device":"humidifier","power":true,"level":3}'
```

预期：

```text
[ ] 返回 JSON
[ ] humidifier_level 变为 3
[ ] GPIO14 蓝色 LED 亮度变化
```

新风/风扇二档：

```powershell
Invoke-RestMethod -Method Post -Uri "http://<ESP32_IP>:8080/api/device" -ContentType "application/json" -Body '{"device":"fan","power":true,"level":2}'
```

预期：

```text
[ ] 返回 JSON
[ ] fresh_air_level 变为 2
[ ] GPIO21 连续旋转舵机风扇中速旋转
```

关闭三类设备：

```powershell
Invoke-RestMethod -Method Post -Uri "http://<ESP32_IP>:8080/api/device" -ContentType "application/json" -Body '{"device":"purifier","power":false,"level":0}'
Invoke-RestMethod -Method Post -Uri "http://<ESP32_IP>:8080/api/device" -ContentType "application/json" -Body '{"device":"humidifier","power":false,"level":0}'
Invoke-RestMethod -Method Post -Uri "http://<ESP32_IP>:8080/api/device" -ContentType "application/json" -Body '{"device":"fan","power":false,"level":0}'
```

开启自动模式：

```powershell
Invoke-RestMethod -Method Post -Uri "http://<ESP32_IP>:8080/api/mode" -ContentType "application/json" -Body '{"mode":"auto","power":true}'
```

开启节能模式：

```powershell
Invoke-RestMethod -Method Post -Uri "http://<ESP32_IP>:8080/api/mode" -ContentType "application/json" -Body '{"mode":"eco","power":true}'
```

节能模式预期：

```text
[ ] eco_mode 变为 true
[ ] auto_mode 变为 false
[ ] purifier_level / fresh_air_level / humidifier_level 都变为 0
[ ] 外设全部关闭
```

### 13.5 导入微信开发者工具

1. 打开微信开发者工具。
2. 选择“导入项目”。
3. 项目目录选择：

```text
E:/espwork/xiaozhi-esp32/xiaozhi-esp32/docs/mini_program_demo
```

4. AppID 选择测试号、游客模式或自己的测试 AppID。
5. 项目名称可填 `空气管家局域网演示`。
6. 导入后确认左侧文件包含：

```text
app.json
pages/index/index.js
pages/index/index.wxml
pages/index/index.wxss
```

7. 打开右上角“详情”或“本地设置”。
8. 勾选“不校验合法域名、web-view 域名、TLS 版本以及 HTTPS 证书”。

不同版本微信开发者工具入口名称可能略有差异，目标就是关闭合法域名校验。否则本地 `http://192.168.x.x:8080` 请求会被拦截。

### 13.6 开发者工具模拟器验证

先用模拟器验证。步骤：

1. 确认 HTTP `/api/state` 已经能在电脑浏览器打开。
2. 在小程序首页输入框填写：

```text
<ESP32_IP>:8080
```

示例：

```text
192.168.1.23:8080
```

3. 点击“连接”。
4. 观察页面提示和各功能区。

模拟器通过标准：

```text
[ ] 页面提示“数据已刷新”或类似成功信息
[ ] 环境状态区域显示温度、湿度、空气评分
[ ] 模式区域显示“自动”“节能”按钮
[ ] 设备控制区域显示“净化”“新风”“加湿”
[ ] 近期空气质量区域显示评分条，或在刚启动时显示“暂无历史数据”
```

### 13.7 小程序功能逐项验证

状态读取：

```text
[ ] 点击“刷新数据”后页面不报错
[ ] 温度显示数值，或没有 DHT11 有效值时显示 --
[ ] 湿度显示数值，或没有 DHT11 有效值时显示 --
[ ] 空气评分显示 0-100 范围内数值
```

历史数据：

```text
[ ] ESP32S3 运行 15-30 秒后点击“刷新数据”
[ ] “近期空气质量”出现一条或多条评分条
[ ] 等待 5 秒再次刷新，评分条数量或最后一条内容更新
```

净化器：

```text
[ ] 依次点击“净化”行的 1、2、3、0
[ ] 小程序高亮档位随点击切换
[ ] /api/state 中 purifier_level 依次为 1、2、3、0
[ ] GPIO13 红色 LED 亮度随档位变化，0 档关闭
```

新风/风扇：

```text
[ ] 依次点击“新风”行的 1、2、3、0
[ ] 小程序高亮档位随点击切换
[ ] /api/state 中 fresh_air_level 依次为 1、2、3、0
[ ] GPIO21 连续旋转舵机风扇按低速、中速、高速、停止变化
```

加湿器：

```text
[ ] 依次点击“加湿”行的 1、2、3、0
[ ] 小程序高亮档位随点击切换
[ ] /api/state 中 humidifier_level 依次为 1、2、3、0
[ ] GPIO14 蓝色 LED 亮度随档位变化，0 档关闭
```

自动模式：

```text
[ ] 点击“自动”后 auto_mode 变为 true
[ ] “自动”按钮显示激活状态
[ ] eco_mode 不是 true
[ ] 等待至少一个 5 秒采样周期后，设备档位可能根据传感器读数自动变化
[ ] 再次点击“自动”后 auto_mode 变为 false
```

节能模式：

```text
[ ] 先把任意设备设置到非 0 档
[ ] 点击“节能”后 eco_mode 变为 true
[ ] auto_mode 变为 false
[ ] purifier_level / fresh_air_level / humidifier_level 都变为 0
[ ] 外设全部关闭
[ ] 再次点击“节能”后 eco_mode 变为 false
```

### 13.8 手机真机预览验证

模拟器通过后再做真机：

1. 手机连接和 ESP32S3 同一个 WiFi。
2. 微信开发者工具点击“预览”生成二维码。
3. 手机微信扫码打开小程序。
4. 手机小程序填写 `<ESP32_IP>:8080`。
5. 点击“连接”。
6. 重复状态读取、历史数据、三类设备控制、自动模式、节能模式验证。

真机通过标准：

```text
[ ] 手机小程序能刷新状态
[ ] 手机小程序能显示近期空气评分条
[ ] 手机小程序能控制净化、新风/风扇、加湿
[ ] 手机小程序能开启/关闭自动模式
[ ] 手机小程序能开启/关闭节能模式
```

如果模拟器能用但真机不能用：

1. 确认手机与 ESP32S3 是同一 WiFi。
2. 确认路由器没有 AP 隔离、访客网络隔离或无线客户端隔离。
3. 确认手机没有使用移动数据/VPN/代理。
4. 答辩演示可优先使用开发者工具模拟器。

### 13.9 小程序验证记录模板

完成后建议把下面记录发给队友或写入交付文档：

```text
验证日期：
固件版本/提交：
ESP32S3 IP：
小程序填写地址：
电脑是否同网段：
手机是否同网段：

HTTP API：
[ ] /api/state 通过
[ ] /api/history 通过
[ ] /api/device 通过
[ ] /api/mode 通过

微信开发者工具模拟器：
[ ] 状态读取通过
[ ] 历史数据通过
[ ] 净化控制通过
[ ] 新风/风扇控制通过
[ ] 加湿控制通过
[ ] 自动模式通过
[ ] 节能模式通过

手机真机预览：
[ ] 状态读取通过
[ ] 历史数据通过
[ ] 净化控制通过
[ ] 新风/风扇控制通过
[ ] 加湿控制通过
[ ] 自动模式通过
[ ] 节能模式通过
[ ] 未验证，原因：

异常现象：
处理结论：
下一步：
```

### 13.10 小程序常见问题

页面提示连接失败：

1. 确认填写的是 `<ESP32_IP>:8080`。
2. 确认 monitor 出现 `Mini program HTTP API started on port 8080`。
3. 确认电脑浏览器能打开 `http://<ESP32_IP>:8080/api/state`。
4. 确认微信开发者工具关闭合法域名校验。
5. 确认电脑和 ESP32S3 在同一个局域网。

API 能打开，小程序不能打开：

1. 打开开发者工具调试器。
2. 在 Console 看 JS 报错。
3. 在 Network 看请求 URL、状态码和失败原因。
4. 确认没有把 `/api/state` 填进小程序首页输入框。

小程序状态变化但外设不动作：

1. 访问 `/api/state` 确认档位是否变化。
2. 看 monitor 是否有 `SmartHome: Apply ... output` 日志。
3. 检查 GPIO 接线、LED 极性、限流电阻。
4. 检查舵机风扇 5V 独立供电和 ESP32 共地。
5. 如果出现 `Brownout detector was triggered`，先处理供电。

## 14. 空气质量曲线和历史数据验证

昨天新增的空气质量曲线和历史数据由同一份传感器采样驱动。

### 14.1 串口屏空气曲线

当前只确认 `c_air` 数字 ID 为 `12`。固件会在 page1 当前页清空并回放曲线：

```text
cle 12,0
add 12,0,<air_score>
```

monitor 中会打印类似：

```text
[TJC] add 12,0,85
```

验证步骤：

1. 确认 HMI 工程中有曲线控件 `c_air`。
2. 确认 `c_air` 数字 ID 是 `12`。
3. 烧录屏幕工程。
4. 烧录 ESP32 固件并打开 monitor。
5. 等待至少两个 5 秒采样周期。
6. 查看 monitor 是否出现 `[TJC] add 12,0,...`。
7. 查看屏幕曲线是否增加点。

如果 monitor 有 `[TJC] cle 12,0` 和 `[TJC] add 12,0,...` 但屏幕曲线不动：

1. 检查 HMI 中 `c_air` 的数字 ID 是否真为 `12`。
2. 检查当前是否在能看到曲线的页面。
3. 检查曲线控件通道是否为 `0`。
4. 检查 HMI 工程是否重新下载。

### 14.2 HTTP 历史数据

历史数据最多保留 `30` 条，约等于最近 `2.5` 分钟窗口。

验证：

```powershell
Invoke-RestMethod -Method Get -Uri "http://<ESP32_IP>:8080/api/history"
```

通过标准：

- `count` 大于 0。
- `capacity` 为 30。
- `samples` 中有 `air_score`。
- 如果 DHT11 正常，`samples` 中有温度和湿度。
- 如果 MQ135 正常，`samples` 中有 `mq135_raw`。

## 15. 最终验收表

```text
[ ] 上电后 ESP32S3 不反复重启
[ ] 串口 monitor 出现 Smart home controller initialized
[ ] WiFi 连接成功后串口 monitor 出现 Mini program HTTP API started on port 8080
[ ] TJC 屏幕能显示页面
[ ] HMI 工程中 hs_eco 已改为 BTN,MODE,ECO,TOGGLE
[ ] TJC 屏幕触摸风扇热区后 monitor 出现 BTN,DEVICE,FAN,TOGGLE
[ ] TJC 屏幕触摸净化热区后 monitor 出现 BTN,DEVICE,AIR_PURIFIER,TOGGLE
[ ] TJC 屏幕触摸加湿热区后 monitor 出现 BTN,DEVICE,HUMIDIFIER,TOGGLE
[ ] TJC 屏幕触摸自动热区后 monitor 出现 BTN,MODE,AUTO,TOGGLE
[ ] TJC 屏幕触摸节能热区后 monitor 出现 BTN,MODE,ECO,TOGGLE
[ ] TJC 屏幕触摸手动环境热区后 monitor 出现 BTN,ENV,MANUAL,TOGGLE
[ ] TJC 屏幕触摸污染预设后 monitor 出现 BTN,ENV,SCENE,POLLUTED
[ ] DHT11 温湿度能刷新，或失败后能在下一周期恢复
[ ] MQ135 原始值能刷新，且 AO 电压不超过 3.3V
[ ] GPIO13 红 LED 按关/低/中/高循环
[ ] GPIO14 蓝 LED 按关/低/中/高循环
[ ] GPIO21 舵机风扇按关/低/中/高循环
[ ] 0 档风扇能停止，或已记录需要微调的停止脉宽
[ ] 自动模式能根据传感器读数调整执行器
[ ] 节能模式能关闭自动模式和所有执行器
[ ] monitor 中出现 [TJC] cle 12,0 和 [TJC] add 12,0,val 空气曲线回放
[ ] /api/state 能返回当前状态 JSON
[ ] /api/history 能返回最近历史 samples
[ ] /api/device 能控制净化、新风/风扇、加湿
[ ] /api/mode 能控制自动和节能
[ ] /api/environment 能设置手动环境并恢复真实传感器
[ ] 小程序能读取状态和历史
[ ] 小程序能控制净化、新风/风扇、加湿、自动、节能和手动环境
[ ] MCP 控制能读取状态、控制至少一个设备，并能调用 self.home.get_advice
[ ] 验证结果已写入阶段交付文档或 progress.md
```

## 16. 常见问题排查

### 16.1 屏幕没有反应

优先检查：

1. 屏幕是否已下载正确 HMI 工程。
2. 屏幕波特率是否为 `9600`。
3. `GPIO41` 是否接屏幕 RX。
4. `GPIO42` 是否接屏幕 TX。
5. 是否共地。
6. 屏幕电源是否足够。

### 16.2 屏幕能显示但按键无日志

优先检查：

1. 触摸热区事件是否写在 `弹起事件(0)`。
2. 是否写了 `prints "BTN,DEVICE,FAN,TOGGLE",0` 和 `printh 0a`。
3. 是否误写到左侧 `输出`。
4. 是否忘记编译并下载 HMI 工程到屏幕。

### 16.3 舵机风扇乱转或不受控

优先检查：

1. 舵机风扇 GND 是否与 ESP32S3 GND 共地。
2. 信号线是否接 `GPIO21`。
3. 舵机风扇是否有稳定 5V。
4. 0 档停止脉宽是否需要校准。
5. 是否把普通 180°舵机当成连续旋转舵机使用。

### 16.4 一开风扇就重启或屏幕闪

优先检查：

1. 舵机风扇是否从 ESP32S3 取电。
2. 5V 电源电流是否足够。
3. 舵机风扇启动瞬间是否造成电压下跌。
4. 是否需要给舵机风扇单独供电并共地。
5. 面包板电源线是否过细或接触不良。

如果 monitor 出现：

```text
E BOD: Brownout detector was triggered
--- Error: ClearCommError failed ...
--- Waiting for the device to reconnect.
```

含义是 ESP32 已经因为电压跌落复位，`ClearCommError` 是复位后 USB 串口断开造成的电脑端报错。此时先不要继续判断串口屏事件或外设逻辑，按以下顺序拆分：

1. 只保留 ESP32S3 USB 供电，确认不再 brownout。
2. 接串口屏但不接舵机风扇，确认不再 brownout。
3. 接传感器和 LED，确认不再 brownout。
4. 最后接舵机风扇；舵机红线接独立稳定 5V，棕/黑线接 GND，并与 ESP32S3 共地。
5. 舵机供电线尽量短，必要时在舵机 5V/GND 附近并联 470uF-1000uF 电容，注意极性。

### 16.5 LED 不亮

优先检查：

1. LED 正负极是否接反。
2. 是否串联限流电阻。
3. GPIO13/GPIO14 是否接错。
4. 当前档位是否为 0。
5. 是否进入节能模式导致全部执行器关闭。

### 16.6 HTTP API 能打开但小程序不能用

优先检查：

1. 小程序填写的是 `<ESP32_IP>:8080`，不是只有 IP。
2. 微信开发者工具关闭了合法域名校验。
3. ESP32、电脑、手机在同一个 WiFi 或同一个网段。
4. 先用浏览器或 PowerShell 验证 `/api/state`。
5. 真机预览失败时，优先用开发者工具模拟器演示。

### 16.7 自动模式看起来没反应

可能原因：

1. 当前传感器读数没有达到自动策略阈值。
2. 节能模式处于开启状态，自动模式被关闭。
3. DHT11 或 MQ135 没有有效读数。
4. 执行器接线有问题，但状态已经在 `/api/state` 中变化。

处理方法：

1. 先访问 `/api/state` 看 `auto_mode` 是否为 `true`。
2. 看 `has_temperature/has_humidity/has_mq135_raw` 是否为 `true`。
3. 等待至少一个 5 秒采样周期。
4. 如果需要强制验证执行器，先用 `/api/device` 直接控制。

## 17. 后续需要记录的数据

实机验证完成后，建议把以下结果补到交付文档：

| 项目 | 实测结果 |
| --- | --- |
| 舵机风扇 0 档停止脉宽 | 待填写 |
| 舵机风扇 1 档可启动脉宽 | 待填写 |
| 舵机风扇 2 档舒适脉宽 | 待填写 |
| 舵机风扇 3 档最大可接受脉宽 | 待填写 |
| 是否需要独立 5V 供电 | 待填写 |
| DHT11 是否稳定 | 待填写 |
| MQ135 AO 电压范围 | 待填写 |
| 屏幕触摸事件是否稳定 | 待填写 |
| HTTP API 是否可访问 | 待填写 |

这些数据会决定下一轮是否需要调整 `FreshAirPulseUsForLevel()`、传感器阈值、自动模式策略和供电方案。
