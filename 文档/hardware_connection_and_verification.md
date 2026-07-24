# bread-compact-wifi 硬件连接与软件验证步骤

本文档说明当前 `bread-compact-wifi` 板型的硬件接线、串口屏准备、固件构建、烧录和验证流程。

适用工程目录：

```text
E:/espwork/xiaozhi-esp32/xiaozhi-esp32
```

适用板型目录：

```text
main/boards/bread-compact-wifi
```

当前功能目标：

- ESP32-S3 运行小智 AI 框架。
- 使用 TJC 4.3 寸串口屏显示室内环境状态。
- 使用 DHT11 读取温湿度。
- 使用 MQ135 读取空气质量模拟值。
- 保留按键、音频输入输出和板载 LED。

当前推荐使用的 HMI 工程文件：

```text
D:/QQ/serial_warm_home .HMI
```

说明：

- 该文件已通过官方 `USART HMI 1.67.6` 打开验证，可作为下载到串口屏的当前页面工程。
- 页面布局由 USART HMI 编辑器中的控件工程承载，ESP32 固件只负责切换到 `page 0` 并更新控件值。
- 旧的 ESP32 运行时 `cls/fill/draw/xstr` 绘制方案已不再作为默认方案，避免覆盖手动完成的 HMI 页面。
- 不要再直接修改 `.HMI` 二进制文件，后续如需做控件版界面，必须通过 USART HMI 编辑器保存。

## 1. 当前固件对应文件

核心文件：

```text
compact_wifi_board.cc
config.h
serial_hmi.h
serial_hmi.cc
dht11_sensor.h
dht11_sensor.cc
mq135_sensor.h
mq135_sensor.cc
serial_hmi_widgets.json
serial_screen_design.md
```

其中：

- `config.h` 定义所有关键引脚和串口参数。
- `serial_hmi.*` 负责 TJC 串口屏通信。
- `dht11_sensor.*` 负责 DHT11 温湿度读取。
- `mq135_sensor.*` 负责 MQ135 ADC 读取和空气等级估算。
- `serial_hmi_widgets.json` 是 TJC 屏幕工程控件命名表。
- `serial_screen_design.md` 是屏幕页面设计说明。

## 2. 引脚总表

当前 `config.h` 中的主要引脚如下。

| 功能 | ESP32-S3 引脚 | 外设连接 | 说明 |
| --- | ---: | --- | --- |
| TJC 串口屏 TX | GPIO41 | 接屏幕 RX | ESP32 发数据到屏幕 |
| TJC 串口屏 RX | GPIO42 | 接屏幕 TX | 屏幕触摸事件回传 ESP32 |
| TJC 串口屏 GND | GND | 接屏幕 GND | 必须共地 |
| DHT11 DATA | GPIO18 | 接 DHT11 DATA | 建议加 4.7k-10k 上拉 |
| MQ135 AO | GPIO1 / ADC1_CH0 | 接 MQ135 AO | 注意电压不能超过 3.3V |
| INMP441 WS | GPIO4 | 麦克风 WS/LRCK | I2S 输入 |
| INMP441 SCK | GPIO5 | 麦克风 SCK/BCLK | I2S 输入 |
| INMP441 SD | GPIO6 | 麦克风 SD/DOUT | I2S 输入 |
| MAX98357A DIN | GPIO7 | 功放 DIN | I2S 输出 |
| MAX98357A BCLK | GPIO15 | 功放 BCLK | I2S 输出 |
| MAX98357A LRC | GPIO16 | 功放 LRC | I2S 输出 |
| BOOT 按键 | GPIO0 | 按键到 GND | 小智启动/配网/聊天控制 |
| 触摸/说话按键 | GPIO47 | 按键到 GND | 按下开始听，松开停止 |
| 音量加 | GPIO40 | 按键到 GND | 单击加音量，长按最大音量 |
| 音量减 | GPIO39 | 按键到 GND | 单击减音量，长按静音 |
| 板载 LED | GPIO48 | 板载灯 | 状态指示 |

## 3. TJC 串口屏连接

当前固件参数：

```text
UART: UART2
ESP32 TX: GPIO41
ESP32 RX: GPIO42
Baud: 9600
Format: 8N1
Command end: FF FF FF
```

接线方式：

| ESP32-S3 | TJC 串口屏 |
| --- | --- |
| GPIO41 / TX | RX |
| GPIO42 / RX | TX |
| GND | GND |
| 屏幕电源正极 | 按屏幕规格接 5V 或指定电压 |

注意事项：

- TX/RX 必须交叉连接。
- ESP32 与屏幕必须共地。
- 屏幕供电优先使用独立稳定电源，不建议直接从 ESP32 开发板 3.3V 脚给 4.3 寸屏供电。
- 如果屏幕 TX 是 5V TTL，必须加电平转换或分压后再接 ESP32 RX。
- 当前固件波特率为 `9600`，TJC 屏幕工程也必须设置为同一波特率。
- 如果后续想改成 `115200`，需要同时修改 `config.h` 和 TJC 屏幕工程串口设置。

## 4. DHT11 温湿度连接

建议接线：

| DHT11 | ESP32-S3 |
| --- | --- |
| VCC | 3.3V |
| GND | GND |
| DATA | GPIO18 |

注意事项：

- DATA 建议通过 `4.7k-10k` 电阻上拉到 3.3V。
- DHT11 采样速度较慢，当前固件每 5 秒读取一次，符合 DHT11 使用习惯。
- 如果读取失败，屏幕温度显示 `-- C`，湿度显示 `-- %`。

## 5. MQ135 空气质量连接

建议接线：

| MQ135 模块 | ESP32-S3 |
| --- | --- |
| VCC | 按模块规格接 5V 或 3.3V |
| GND | GND |
| AO | GPIO1 / ADC1_CH0 |

重要安全注意：

- ESP32 ADC 输入电压不能超过 3.3V。
- 很多 MQ135 模块使用 5V 供电时，AO 最高可能接近 5V，不能直接接 ESP32 ADC。
- 如果 AO 可能超过 3.3V，必须使用分压电路或电平适配。
- MQ135 需要预热，刚上电时数值漂移明显，不能马上作为准确空气质量结果。
- 当前固件只用 MQ135 原始 ADC 值做演示级空气质量估算，不是精确 ppm 检测。

当前估算规则：

| MQ135 原始值 | 屏幕状态 | 空气评分 |
| ---: | --- | ---: |
| `< 500` | 优 | 90 |
| `500-999` | 良 | 75 |
| `1000-1999` | 轻度污染 | 45 |
| `>= 2000` | 重度污染 | 20 |

## 6. 音频连接

### INMP441 麦克风

| INMP441 | ESP32-S3 |
| --- | --- |
| VCC | 3.3V |
| GND | GND |
| WS / LRCK | GPIO4 |
| SCK / BCLK | GPIO5 |
| SD / DOUT | GPIO6 |
| L/R | GND 或 3.3V，按模块说明选择声道 |

### MAX98357A 功放

| MAX98357A | ESP32-S3 |
| --- | --- |
| VIN | 5V 或模块允许电压 |
| GND | GND |
| DIN | GPIO7 |
| BCLK | GPIO15 |
| LRC | GPIO16 |
| SPK+ / SPK- | 扬声器 |

注意：

- 功放和屏幕电流较大时，建议使用外部供电，并与 ESP32 共地。
- 扬声器不要短接。
- 首次验证时可以先不接功放，只验证串口日志和屏幕。

## 7. TJC 屏幕工程准备

在 TJC 编辑器中新建工程：

```text
屏幕尺寸：4.3 寸
方向：横屏
建议分辨率：480x272
风格：家居温暖风
波特率：9600
```

按 `serial_hmi_widgets.json` 创建控件。当前推荐使用 `ui_assets/` 中的页面背景和图标资源：

```text
main/boards/bread-compact-wifi/ui_assets/page0_home_launcher_hmi_blank.png
main/boards/bread-compact-wifi/ui_assets/page1_air_detail_hmi_blank.png
main/boards/bread-compact-wifi/ui_assets/page2_smart_home_hmi_blank.png
main/boards/bread-compact-wifi/ui_assets/page3_ai_settings_hmi_manual_env.png
```

具体每个图标、返回按钮、左右滑动热区需要填写的事件命令，见：

```text
../文档/manual_hmi_event_setup.md
```

页面建议：

| 页面 | 用途 |
| --- | --- |
| `page0` | 手机式首页和软件图标入口 |
| `page1` | 空气评分详情，显示评分、等级、MQ135 原始值、温湿度和舒适度 |
| `page2` | 智能家居控制 |
| `page3` | AI 与设置 |

首页 `page 0` 必须至少包含：

```text
t_temp
t_humi
j_air
t_air_state
t_air
t_advice
t_ai_state
```

空气详情页建议包含：

```text
t_air_score
j_air_detail
t_air_state
t_air_raw
t_temp_d
t_humi_d
t_comfort
hs_back
```

说明：当前 MQ135 只作为 ADC 原始值和演示级评分来源，不能直接输出 PM2.5、CO2、TVOC 浓度。不要把 `t_pm25`、`t_co2`、`t_tvoc_level` 作为当前 page1 必备控件；后续接入真实传感器后再扩展。

智能家居控制页建议包含：

```text
hs_purifier
hs_fan
hs_humid
hs_auto
```

图标入口和滑动热区建议发送：

```text
BTN,PAGE,AIR_DETAIL
BTN,PAGE,SMART_HOME
BTN,PAGE,AI
BTN,PAGE,SETTINGS
SWIPE,LEFT
SWIPE,RIGHT
```

控件名称必须完全一致，包括大小写和下划线。

## 8. TJC 屏幕下载验证

可以使用两种方式把屏幕工程下载到 TJC 屏幕：

### 方式 A：串口下载

1. 使用 USB 转 TTL 连接电脑和 TJC 屏幕。
2. 打开 TJC 编辑器。
3. 选择正确串口和屏幕型号。
4. 下载工程到屏幕。
5. 下载完成后重启屏幕。

### 方式 B：SD 卡下载

1. 在 TJC 编辑器中编译并导出屏幕工程。
2. 将导出的文件放到 SD 卡根目录。
3. 插入屏幕 SD 卡槽。
4. 给屏幕上电，等待自动下载。
5. 下载完成后断电，拔出 SD 卡，再重新上电。

## 9. ESP-IDF 构建验证

### VSCode ESP-IDF 操作

1. 用 VSCode 打开工程根目录：

```text
E:/espwork/xiaozhi-esp32/xiaozhi-esp32
```

2. 确认当前目标芯片为：

```text
esp32s3
```

3. 确认当前板型配置为：

```text
CONFIG_BOARD_TYPE_BREAD_COMPACT_WIFI=y
```

4. 执行：

```text
ESP-IDF: Build your project
```

5. 如果原 `build` 目录提示绑定了旧绝对路径，不要急着删除。可以先用独立构建目录验证：

```powershell
& 'D:\esp\Espressif\frameworks\esp-idf-v5.5.2\export.ps1'
idf.py -B build_codex_check build
```

本机已验证该命令可以构建通过。

### 已验证结果

最近一次验证命令：

```powershell
& 'D:\esp\Espressif\frameworks\esp-idf-v5.5.2\export.ps1'
idf.py -B build_codex_check build
```

验证结果：

```text
Project build complete.
xiaozhi.bin binary size 0x23f030 bytes.
Smallest app partition is 0x3f0000 bytes.
0x1b0fd0 bytes (43%) free.
```

生成固件：

```text
build_codex_check/xiaozhi.bin
```

## 10. 烧录和串口监视

确认开发板连接电脑后，查看串口号，例如 `COM3`、`COM5`。

使用临时构建目录烧录：

```powershell
& 'D:\esp\Espressif\frameworks\esp-idf-v5.5.2\export.ps1'
idf.py -B build_codex_check -p COMx flash monitor
```

将 `COMx` 替换为实际串口号。

VSCode 中也可以执行：

```text
ESP-IDF: Flash your project
ESP-IDF: Monitor your device
```

或：

```text
ESP-IDF: Build, Flash and Start a Monitor on Your Device
```

## 11. 固件启动后应看到的日志

启动后，USB monitor 中应看到类似信息：

```text
TestBoard - TJC 串口屏测试模式
输出: UART2(GPIO41/42) + USB Serial
TJC UART ready: UART2 TX=GPIO41 RX=GPIO42 baud=9600
[TJC] page 0
[TJC] t_ai_state.txt="IDLE"
[TJC] t_temp.txt="-- C"
[TJC] t_humi.txt="-- %"
[TJC] t_air_state.txt="UNKNOWN"
[TJC] j_air.val=0
```

每 5 秒应看到一次传感器读取：

```text
---- Sensor Read ----
DHT11  : OK    temp=26.0°C  humi=58.0%
MQ135  : OK    raw=820      level=良
[TJC] t_temp.txt="26.0 C"
[TJC] t_humi.txt="58.0 %"
[TJC] t_air_state.txt="良"
[TJC] t_air.txt="良(820)"
[TJC] j_air.val=75
[TJC] t_advice.txt="Keep ventilation"
```

点击图标或触发滑动热区时，应看到类似事件日志：

```text
Screen event: raw=BTN,PAGE,AIR_DETAIL target=AIR_DETAIL action=
[TJC] page 1
Screen event: raw=SWIPE,LEFT target=LEFT action=
[TJC] page 2
```

如果没有接传感器，可能看到：

```text
DHT11  : FAIL
MQ135  : FAIL
```

这时屏幕仍应显示占位值，固件不应崩溃。

## 12. 屏幕显示验证步骤

按以下顺序验证：

1. 不接 DHT11/MQ135，只接 ESP32 和 TJC 屏幕。
2. 烧录固件并打开 monitor。
3. 确认 USB 串口有 `[TJC] page 0`、`[TJC] t_temp.txt=...`、`[TJC] j_air.val=...` 等控件更新命令。
4. 看屏幕是否保持 HMI 工程中的手工页面布局。
5. 看 `t_ai_state`、`t_temp`、`t_humi`、`t_air_state`、`j_air` 等控件是否刷新。
6. 看空气评分条是否从 0 变成传感器估算分数。
7. 点击首页软件图标，确认可以切到空气详情、智能家居、AI 与设置页。
8. 触发 `SWIPE,LEFT` 和 `SWIPE,RIGHT`，确认页面按顺序切换。
9. 接 DHT11，确认温度和湿度从 `--` 变成真实值。
10. 接 MQ135，确认空气等级、MQ 原始值和评分刷新。
11. 如果 USB 有 `[TJC]` 输出但屏幕不刷新，优先检查 TX/RX、GND、波特率和屏幕工程是否已下载。

## 13. 常见问题排查

### USB 串口有 `[TJC]`，屏幕没有变化

优先检查：

- ESP32 GPIO41 是否接屏幕 RX。
- ESP32 GPIO42 是否接屏幕 TX。
- GND 是否共地。
- 屏幕工程是否已经下载到屏幕。
- 屏幕波特率是否为 `9600`。
- 屏幕供电是否稳定。
- 当前固件依赖 HMI 工程中的控件名；重点检查 `serial_hmi_widgets.json` 中列出的控件是否存在且命名完全一致。

### 屏幕显示乱码

可能原因：

- TJC 工程没有配置包含中文的字体。
- 文本控件字体资源不支持当前字符。
- 屏幕工程编码/字库与固件发送文本不匹配。

处理建议：

- 先用英文验证，例如 `IDLE`、`System starting`。
- 确认英文正常后，再逐步切换中文文本。
- 在 TJC 编辑器中检查字体资源。

### DHT11 一直失败

检查：

- DATA 是否接 GPIO18。
- DATA 是否有上拉电阻。
- VCC 是否为 3.3V。
- GND 是否共地。
- 线是否过长。
- DHT11 采样间隔是否过短。当前固件为 5 秒，一般可以。

### MQ135 数值异常或一直很高

检查：

- AO 是否接 GPIO1。
- AO 电压是否超过 3.3V。
- 是否忘记共地。
- MQ135 是否完成预热。
- 模块电位器阈值只影响 DO，固件读取的是 AO。

### 构建提示 build 目录路径不一致

当前工程曾出现：

```text
Build directory ... configured for project 'D:\Project\...' not 'E:\espwork\...'
```

推荐处理：

- 不想影响原 `build` 目录时，使用 `idf.py -B build_codex_check build`。
- 确认不需要保留原构建缓存后，再考虑 `idf.py fullclean`。

## 14. 当前未完成项

当前固件已经完成发送侧验证基础，但仍有以下后续项：

- `SerialHmi::PollEvent()` 已实现基础解析，板级逻辑已消费页面切换和滑动事件；设备控制事件仍仅记录日志，尚未控制真实外设。
- 当前 page1 已改为空气评分详情页；PM2.5、CO2、TVOC 没有真实传感器，后续接入硬件后再新增对应控件和固件字段。
- MQ135 空气评分只是演示估算，不应作为精确检测值。
- 需要在真实 TJC 屏幕工程中确认控件名、字体和页面 ID。
- 如果最终屏幕波特率改为 `115200`，必须同步修改屏幕工程和 `config.h`。

## 15. 推荐初次上板顺序

1. 只接 ESP32 和 USB，确认固件能启动。
2. 接 TJC 屏幕电源、GND、TX/RX，确认首页能刷新。
3. 接 DHT11，确认温湿度刷新。
4. 接 MQ135，确认空气状态和评分刷新。
5. 接音频输入输出，确认小智语音功能。
6. 最后再接触摸按钮或智能家居控制页面事件。

这样可以把问题分层定位，避免一开始所有外设同时接入导致排查困难。

