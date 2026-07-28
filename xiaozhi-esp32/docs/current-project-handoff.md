# xiaozhi-esp32 当前工程交付说明

> 2026-07-28 已完成多轮“只增加代码、不更换硬件”增强。稳定性内容见 [`phase-handoff-2026-07-28-software-reliability.md`](phase-handoff-2026-07-28-software-reliability.md)，小程序演示增强见 [`phase-handoff-2026-07-28-mini-program-demo-enhancements.md`](phase-handoff-2026-07-28-mini-program-demo-enhancements.md)，主看板与后台拆分见 [`phase-handoff-2026-07-28-mini-program-admin-split.md`](phase-handoff-2026-07-28-mini-program-admin-split.md)。本节更新优先于文档中较早的 HMI 外部路径和“雷达/光敏尚未接入”描述；当前 HMI 工程已保存在仓库 `hmi/`。

本文档记录截至 `2026-07-19` 的当前工程状态，供交付和接续开发使用。

## 工程位置

| 项目 | 路径/值 |
| --- | --- |
| ESP-IDF 工程根目录 | `E:/espwork/xiaozhi-esp32/xiaozhi-esp32` |
| 当前使用板型 | `CONFIG_BOARD_TYPE_BREAD_COMPACT_WIFI=y` |
| 当前目标芯片 | `esp32s3` |
| 当前分区表 | `partitions/v2/16m.csv` |
| 队友代码来源 | `E:/espwork/xiaozhi-esp32/work1/bread-compact-wifi` |
| 当前 HMI 工程文件 | `hmi/serial_warm_home.HMI`；page1 背景测试版为 `hmi/serial_warm_home_page1_background_test.HMI` |

`sdkconfig` 属于受保护配置文件，本阶段未修改。

## 当前总体状态

当前 `bread-compact-wifi` 板型已经形成“ESP32 固件 + TJC/USART HMI 串口屏 + DHT11 + MQ135”的控件化页面方案：

- HMI 工程负责页面布局、图标、触摸热区、字体、背景和滑动/过渡效果。
- ESP32 固件负责 UART2 通信、控件赋值、页面切换、触摸事件解析和传感器数据刷新。
- 固件不再默认使用 `cls/fill/draw/xstr` 运行时整页绘制页面。
- 当前已整合队友 `work1` 中的 GBK 转码和 DHT11 时序优化。
- 队友 `work1/compact_wifi_board.cc` 中的直接 UART 串口屏实现未直接覆盖，因为它会和当前 `SerialHmi` 的事件轮询、页面缓存刷新架构冲突。
- 2026-07-05 已新增智能家居控制器、MCP 工具、局域网 HTTP API 和微信小程序演示工程。
- 2026-07-07 风扇方案已恢复为 `GPIO21` 上的 180°角度舵机扇叶；`fresh_air_servo` 后台任务按档位执行不同角度范围的往复摆动。
- 2026-07-06 最新稳定性修复：HTTP API 不再在 `CompactWifiBoard()` 构造函数中提前启动，已改为 WiFi 连接成功后启动，修复 `tcpip_send_msg_wait_sem Invalid mbox` 断言重启。
- 2026-07-06 最新外设控制修复：屏幕设备事件进入控制器后会输出 `SmartHome: Apply ... output` 诊断日志，并修复设备触摸切换无法真正回到 0 档的问题；若出现 `Brownout detector was triggered`，优先处理供电。
- 2026-07-06 最新界面/联动增强：`c_air` 空气质量曲线改为 ESP32 缓存最近 30 条空气评分，进入 `page1` 时先 `cle 12,0` 再回放 `add 12,0,val`；`t_comfort`、`t_advice`、空气等级和手动模拟环境已接入统一环境规则。
- 2026-07-06 最新手动测试能力：新增手动环境模式，HTTP `/api/environment`、小程序“手动输入数据”、串口屏 `BTN,ENV,...` 热区和 MCP 工具均可模拟舒适/高温/干燥/污染环境，用于自动模式验证。
- 2026-07-07 最新串口屏资源：已生成 `ui_assets/page3_ai_settings_hmi_manual_env.png`，在 `page3` 底部画入 `手动/舒适/高温/干燥/污染` 五个按钮；继续在 USART HMI 编辑器中用 `hs_env_m`、`hs_env_good`、`hs_env_hot`、`hs_env_dry`、`hs_env_bad` 透明触摸热区覆盖这些按钮。
- 2026-07-14 自动控制增强：节能模式改为按环境使用较低档位；新增雷达占用、环境亮度、智能灯光、无人关机和环境突变报警规则。
- 2026-07-14 AI 联网增强：电脑端小智 MCP 桥接新增灯光、占用/亮度、报警确认、天气、RSS 新闻和室内外综合建议工具。
- 2026-07-18 已接入 GL5528 类光敏模块的 ADC 驱动，以及 HLK-LD2450 的 UART1 收包和标准目标帧解析；雷达实物线束和收包验证仍待完成。
- 2026-07-19 已根据实物确认光敏模块的 AO 为“遮光数值升高、照亮数值降低”，默认校准已改为 `DARK_RAW=3300`、`BRIGHT_RAW=300`，需重新烧录后验证。
- 2026-07-28 小程序新增离线演示、雷达二维位置图、三指标趋势曲线、自定义自动化规则、五种一键场景和最近 32 条事件日志；新增 `/api/events`、`/api/automation`、`/api/scene`。
- 2026-07-28 小程序进一步拆分为家居主看板和操作后台；主页面底部灰色小字进入后台，真实/手动传感器系统、连接、规则、日志和诊断均移至后台。
- 蜂鸣器和独立 LED 灯的真实输出仍未接入，原因是供电、驱动电路和 GPIO 尚未最终确认；`light_on` 目前仅是可验证的逻辑状态。

## 当前固件能力

- TJC 4.3 寸串口屏 UART2 通信。
- TJC 命令发送前自动 UTF-8 转 GBK，屏幕更容易正确显示中文；USB monitor 仍输出 UTF-8 原文。
- DHT11 温湿度读取，使用开漏上拉和临界区读数提高单总线时序稳定性。
- MQ135 ADC 原始值读取。
- 基于 MQ135 原始值的演示级空气评分。
- USB monitor 中同步输出 `[TJC]` 命令，方便无屏或串口排查。
- 手机式图标入口事件和左右滑动/边缘热区切页事件。
- 智能家居执行器控制：`GPIO13` 净化 LED、`GPIO14` 加湿 LED、`GPIO21` 180°角度舵机扇叶。
- 新风/风扇档位：`0` 档停在 `0°`；`1` 档 `20°-70°` 慢速摆动；`2` 档 `10°-120°` 中速摆动；`3` 档 `0°-180°` 快速摆动。
- MCP 工具和 HTTP API 已接入同一套智能家居状态：净化、新风、加湿、自动、节能。
- MCP 工具已补充环境能力：`self.home.set_manual_environment`、`self.home.set_environment_preset`、`self.home.get_advice`。
- MCP 工具已补充智能上下文：`self.home.set_light`、`self.home.update_context`、`self.home.acknowledge_alarm`、`self.home.get_environment_briefing`。
- HTTP API 已补充 `POST /api/context` 和 `POST /api/alarm/ack`，用于雷达/光敏联调和报警确认。
- 当前 `sdkconfig` 已启用 AFE 唤醒词；本阶段没有修改该受保护配置。雷达占用由无到有时的软件回调会调用 `Application::StartListening()`。
- HTTP API 已补充 `POST /api/environment`：支持 `{enabled:false}` 退出手动环境，支持 `temperature_c/humidity_percent/air_score` 手动输入，也支持 `preset=GOOD/HOT/DRY/POLLUTED`。
- 小程序演示工程已补充舒适度、环境建议、数据来源、手动输入数据、预设场景和恢复真实传感器入口。
- HTTP API 启动时机：只有收到 `NetworkEvent::Connected` 后才启动 `SmartHomeHttpServer`；刚开机未联网时不会立刻出现 `Mini program HTTP API started on port 8080`。
- 原小智框架的按键、音频接口和 LED 接口仍保留。

## 当前代码框架

| 文件 | 当前职责 |
| --- | --- |
| `main/boards/bread-compact-wifi/compact_wifi_board.cc` | 板级入口；创建 `SerialHmi`、传感器任务和屏幕事件任务 |
| `main/boards/bread-compact-wifi/config.h` | UART、DHT11、MQ135、I2S、按键、LED 等引脚定义 |
| `main/boards/bread-compact-wifi/serial_hmi.h` | 串口屏事件和空气质量数据结构、对外接口 |
| `main/boards/bread-compact-wifi/serial_hmi.cc` | TJC UART 初始化、GBK 转码发送、控件刷新、页面切换、事件解析 |
| `main/boards/bread-compact-wifi/utf8_to_gbk.h/.cc` | UTF-8 到 GBK 转码 |
| `main/boards/bread-compact-wifi/gbk_table.inc` | GBK 映射表，生成数据，不手工编辑 |
| `main/boards/bread-compact-wifi/dht11_sensor.h/.cc` | DHT11 单总线读数 |
| `main/boards/bread-compact-wifi/mq135_sensor.h/.cc` | MQ135 ADC 读数和演示级空气等级 |
| `main/boards/bread-compact-wifi/ambient_light_filter.h/.cc` | 光敏 AO 原始值归一化、反向标定和滤波 |
| `main/boards/bread-compact-wifi/ambient_light_sensor.h/.cc` | GL5528 类光敏模块 ADC 读取 |
| `main/boards/bread-compact-wifi/ld2450_protocol.h/.cc` | LD2450 目标帧校验和解析 |
| `main/boards/bread-compact-wifi/ld2450_sensor.h/.cc` | LD2450 UART1 收包、帧缓存和诊断计数 |
| `main/boards/bread-compact-wifi/smart_home_controller.h/.cc` | 智能家居执行器状态、PWM 输出、自动/节能模式、MCP 工具 |
| `main/boards/bread-compact-wifi/smart_home_http_server.h/.cc` | 局域网 HTTP API，服务小程序演示 |
| `main/boards/bread-compact-wifi/serial_hmi_widgets.json` | HMI 页面、控件、资源和事件契约 |

`main/CMakeLists.txt` 当前会按板型目录 `file(GLOB ...)` 收集 `.cc/.c` 文件，因此新增 `utf8_to_gbk.cc` 会自动参与构建。

## 串口屏通信参数

| 项目 | 当前值 |
| --- | --- |
| UART | `UART2` |
| ESP32 TX | `GPIO41`，接屏幕 RX |
| ESP32 RX | `GPIO42`，接屏幕 TX |
| 波特率 | `9600` |
| 格式 | `8N1` |
| 命令结束字节 | `FF FF FF` |

注意：ESP32 和屏幕必须共地。若屏幕 TX 为 5V TTL，接 ESP32 RX 前需要电平转换或分压。

## 页面和事件状态

当前推荐页面：

| 页面 | 用途 |
| --- | --- |
| `page0` | 手机式首页 |
| `page1` | 空气评分详情 |
| `page2` | 智能家居控制 |
| `page3` | AI 与设置 |

当前图标入口事件：

```text
BTN,PAGE,AIR_DETAIL
BTN,PAGE,SMART_HOME
BTN,PAGE,AI
BTN,PAGE,SETTINGS
```

当前滑动/边缘热区事件：

```text
SWIPE,LEFT
SWIPE,RIGHT
BTN,PAGE,NEXT
BTN,PAGE,PREV
```

当前设备控制事件已接入真实执行逻辑：

```text
BTN,DEVICE,AIR_PURIFIER,TOGGLE
BTN,DEVICE,FAN,TOGGLE
BTN,DEVICE,HUMIDIFIER,TOGGLE
BTN,MODE,AUTO,TOGGLE
BTN,MODE,ECO,TOGGLE
```

其中 `BTN,DEVICE,FAN,TOGGLE` 继续作为屏幕事件名使用，固件内部映射到 `fresh_air_level` 和 `GPIO21` 180°角度舵机扇叶。

当前手动环境测试事件：

```text
BTN,ENV,MANUAL,TOGGLE
BTN,ENV,SCENE,GOOD
BTN,ENV,SCENE,HOT
BTN,ENV,SCENE,DRY
BTN,ENV,SCENE,POLLUTED
```

这些事件建议放在 `page3` AI 与设置页，热区名使用 `hs_env_m`、`hs_env_good`、`hs_env_hot`、`hs_env_dry`、`hs_env_bad`，均不超过 14 个字符。

## 当前有效资源和文档

| 文件/目录 | 作用 |
| --- | --- |
| `main/boards/bread-compact-wifi/ui_assets/` | 当前有效页面背景、图标和滑动示意图 |
| `../文档/串口屏设计说明.md` | 页面设计说明 |
| `../文档/串口屏手动事件配置手册.md` | USART HMI 编辑器手动配置步骤 |
| `../文档/硬件连接与软件验证步骤.md` | 接线、构建、烧录、monitor 验证 |
| `../文档/智能家居外设接线与验证步骤.md` | 7 月 5 日新增智能家居、MCP、HTTP API、小程序、空气曲线和外设的完整接线与验证手册 |
| `../文档/串口屏与环境监测项目交付说明.md` | 板型目录内交付说明 |
| `../../mini_program_demo/README.md` | 小程序局域网演示工程导入、HTTP API 预验证、模拟器/真机验收和故障排查手册；实际工程位于仓库根目录 `mini_program_demo` |
| `docs/project-file-map.md` | 工程文件索引和清理说明 |
| `docs/phase-handoff-2026-06-09-work1-integration.md` | 本阶段整合交付文档 |
| `docs/phase-handoff-2026-06-09-serial-screen-score-page.md` | page1 空气评分页阶段交付文档 |
| `docs/phase-handoff-2026-06-10-hmi-background-script-probe.md` | HMI 后台脚本探测交付文档 |
| `docs/phase-handoff-2026-07-05-smart-home-control.md` | 智能家居执行器、自动/节能、MCP 工具交付文档 |
| `docs/phase-handoff-2026-07-05-mini-program-http-api.md` | 小程序局域网 HTTP API 交付文档 |
| `docs/phase-handoff-2026-07-06-mini-program-verification-guide.md` | 小程序详细验证步骤交付文档 |
| `docs/phase-handoff-2026-07-06-continuous-servo-fan.md` | 360°连续旋转舵机风扇改造交付文档 |
| `docs/phase-handoff-2026-07-06-full-verification-guide.md` | 7 月 5 日新增功能完整接线与验证手册交付文档 |
| `docs/phase-handoff-2026-07-06-http-api-startup-crash-fix.md` | HTTP API 启动过早导致 `Invalid mbox` 重启的修复交付文档 |
| `docs/phase-handoff-2026-07-06-smart-home-actuator-output-debug.md` | 智能家居外设输出诊断、按键 0 档循环修复和 Brownout 排查交付文档 |
| `docs/phase-handoff-2026-07-06-air-curve-comfort-manual-env-ai.md` | 空气质量曲线、舒适度/建议、手动环境、小程序和 AI/MCP 工具增强交付文档 |
| `docs/phase-handoff-2026-07-07-xiaozhi-mcp-bridge.md` | 小智远端 MCP 桥接服务交付文档 |
| `docs/phase-handoff-2026-07-14-smart-sensing-ai-automation.md` | 雷达/亮度/灯光/报警规则、节能重做、天气新闻和唤醒研究的当前交付文档 |

## HMI 后台脚本探测状态

2026-06-10 已验证：脚本可以只读扫描 `.HMI` 内嵌 PNG 资源，也能在测试副本中替换 page1 背景槽位。但从实际交付角度看，脚本副本方式不作为当前正式路径，因为它不能保证编辑器接受副本，也不能创建控件、热区和事件。

当前脚本和产物：

| 文件 | 状态 |
| --- | --- |
| `scripts/hmi_background_probe.ps1` | 可扫描 HMI 内嵌 PNG；加 `-PatchCopy` 时生成测试副本 |
| `main/boards/bread-compact-wifi/ui_assets/page1_air_detail_hmi_blank_light.png` | 轻量 page1 背景，`480x272`，`5319` 字节 |
| `tmp_hmi_probe_page1_bg_replace.HMI` | 已替换 page1 背景槽位的 HMI 测试副本 |

脚本验证结论：

- 原始 HMI `D:/QQ/serial_warm_home .HMI` 哈希保持不变。
- `tmp_hmi_probe_page1_bg_replace.HMI` 只作为脚本探测记录，当前不建议继续投入验证。
- 当前脚本只适合图片资源级替换，不能安全创建控件、改事件或写滑动动画。
- 正式路径改为：用 USART HMI 编辑器打开 `D:/QQ/serial_warm_home .HMI`，另存为人工编辑副本，然后手动导入背景、创建控件和填写事件。

已删除的冗余文件：

- `main/boards/bread-compact-wifi/ui_mockups/`
- `main/boards/bread-compact-wifi/tjc_screen_implementation_plan.md`

删除原因：旧 mockup 和旧运行时绘图方案已被 `ui_assets/`、`串口屏设计说明.md`、`串口屏手动事件配置手册.md` 替代。

## 当前传感器状态

| 外设 | ESP32-S3 引脚 | 当前状态 |
| --- | --- | --- |
| DHT11 DATA | `GPIO18` | 已接入驱动，建议 4.7k-10k 外部上拉 |
| MQ135 AO | `GPIO1 / ADC1_CH0` | 已接入 ADC 读数，必须保证输入不超过 3.3V |
| GL5528 类光敏模块 AO | `GPIO2 / ADC1_CH1` | 驱动已接入；当前模块遮光时原始值升高，重新烧录后按反向标定验证 |
| HLK-LD2450 | `UART1`，RX=`GPIO11`、TX=`GPIO12` | 固件已接入；等待配套小间距 4P 线束或转接板后接线验证 |
| PM2.5 | 未定 | 暂未接入；当前 page1 不再显示占位浓度 |
| CO2 | 未定 | 暂未接入；当前 page1 不再显示占位浓度 |
| TVOC | 未定 | 暂未接入；当前 page1 不再显示占位浓度 |

MQ135 当前只做演示级估算，不是准确 ppm 检测。page1 当前显示空气评分、空气等级、MQ135 原始值、温度、湿度和舒适度；后续接入真实 PM2.5、CO2、TVOC 传感器后再扩展浓度控件。

## 最近一次构建验证

验证日期：`2026-07-28`

验证命令：

使用当前机器已安装的 ESP-IDF `5.5.3` 和现有 `build/` 目录执行 Ninja 完整构建。不要复制旧文档中的固定盘符；应先运行本机 ESP-IDF 导出脚本，或使用 VS Code Espressif 扩展初始化终端。

验证结果：

```text
Build completed successfully.
Generated build/xiaozhi.bin
xiaozhi.bin binary size 0x24bef0 bytes.
Smallest app partition is 0x3f0000 bytes.
0x1a4110 bytes (42%) free.
```

生成固件：

```text
build/xiaozhi.bin
```

说明：本次构建使用 ESP-IDF `5.5.3`；组件清单要求 `>=5.5.2`。当前机器旧的 `D:\esp\...5.5.2` 文档路径不存在，后续不要继续写死工具链路径。不要执行 `idf.py set-target` 或 `menuconfig`，除非明确需要重新配置板型。

## 下一阶段目标

1. 烧录 `build/xiaozhi.bin`，确认启动后没有互斥锁创建失败、任务创建失败或 Watchdog 日志。
2. 访问 `/api/health`，确认固件版本、空闲内存、Wi-Fi RSSI、DHT11/MQ135/光敏/雷达健康字段可读。
3. 临时断开 DHT11 数据线，确认 30 秒内显示缓存，超过 30 秒后 `dht_stale=true` 且温湿度不再作为有效实时数据。
4. 在自动模式中手动设置净化器档位，确认 `purifier_override_remaining_seconds` 开始倒计时，自动规则暂不覆盖该设备。
5. 重新开启自动模式，确认所有手动覆盖立即清零并恢复规则接管。
6. 连续验证雷达坐标和 `radar_zone=left/center/right`；连续 2 分钟无目标后确认无人关机逻辑生效。
7. 验证环境突变需要连续两个采样周期才报警，确认报警后 60 秒内不会重复弹出。
8. 重启设备，确认自动/节能模式从 NVS 恢复，但执行器具体档位不会被危险地直接恢复。
9. 微信小程序填写局域网地址，确认每 10 秒刷新、历史相对时间和设备诊断页正常。
10. 如需启用鉴权，在 `config.h` 设置非空 `SMART_HOME_API_TOKEN`，并在小程序、PowerShell 或 MCP 桥接设置同一 Token。

## 接续开发硬规则

- 开始开发前先阅读本文档、`docs/project-file-map.md`、`docs/continuation-notes.md` 和当前板型关键源码；若仓库后续新增 `AGENTS.md`，再同时遵循其中约束。
- 每完成一个明确阶段，必须新增或更新中文阶段交付文档。
- 不修改 `.vscode/**`、`sdkconfig*`、`CMakePresets.json`、ESP-IDF 工具链、全局环境变量等受保护内容，除非用户明确授权。
