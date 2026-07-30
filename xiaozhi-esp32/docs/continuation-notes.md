# 接续开发注意事项

本文档给后续接手 `xiaozhi-esp32` 的同学使用，重点说明开发前阅读顺序、当前工程边界、串口屏注意事项和阶段交付要求。

## 开发前必须先读

每次继续开发前，先按顺序阅读：

1. 若当前仓库存在 `AGENTS.md` 则先阅读；当前版本未包含该文件
2. `docs/current-project-handoff.md`
3. `docs/project-file-map.md`
4. `docs/phase-handoff-2026-06-09-work1-integration.md`
5. `docs/phase-handoff-2026-06-10-hmi-background-script-probe.md`
6. `docs/phase-handoff-2026-07-05-smart-home-control.md`
7. `docs/phase-handoff-2026-07-06-continuous-servo-fan.md`
8. `docs/phase-handoff-2026-07-06-http-api-startup-crash-fix.md`
9. `docs/phase-handoff-2026-07-06-smart-home-actuator-output-debug.md`
10. `docs/phase-handoff-2026-07-06-mini-program-verification-guide.md`
11. `docs/phase-handoff-2026-07-06-air-curve-comfort-manual-env-ai.md`
12. `docs/phase-handoff-2026-07-07-xiaozhi-mcp-bridge.md`
13. `docs/phase-handoff-2026-07-14-smart-sensing-ai-automation.md`
14. `docs/phase-handoff-2026-07-18-ld2450-ambient-light-integration.md`
15. `docs/phase-handoff-2026-07-19-ambient-light-polarity-and-radar-cable.md`
16. `docs/phase-handoff-2026-07-28-software-reliability.md`
17. `docs/phase-handoff-2026-07-28-mini-program-demo-enhancements.md`
18. `docs/phase-handoff-2026-07-28-mini-program-admin-split.md`
19. `docs/phase-handoff-2026-07-30-logic-ai-optimization-final.md`
20. `../文档/串口屏与环境监测项目交付说明.md`
21. 如果涉及小程序，再读：
    - 仓库根目录 `mini_program_demo/README.md`
    - `../文档/智能家居外设接线与验证步骤.md` 的第 12、13 节
22. 如果涉及串口屏，再读：
   - `../文档/串口屏手动事件配置手册.md`
   - `main/boards/bread-compact-wifi/serial_hmi_widgets.json`
   - `../文档/串口屏设计说明.md`
   - `main/boards/bread-compact-wifi/serial_hmi.cc`

读完后再检查当前工作树状态，避免覆盖用户或队友已有改动。

## 每阶段交付要求

每完成一个明确阶段任务，都要更新或新增中文交付文档。文档至少包含：

- 当前工程状态。
- 本阶段改动文件。
- 验证命令和结果。
- 已删除或保留的文件说明。
- 遗留风险。
- 下一阶段目标。
- 需要人工完成或确认的事项。

用户明确说“结束”“暂停”“先到这里”“记录当前进度”时，也必须补充交付文档。

## 优先不要改的内容

除非团队明确决定重新配置工程，否则不要改：

- `.vscode/**`
- `sdkconfig`
- `sdkconfig.defaults*`
- `CMakePresets.json`
- ESP-IDF 安装目录或 VSCode ESP-IDF 插件配置
- 全局环境变量、Python 环境、ESP-IDF tools

当前工程已经能被 VSCode/ESP-IDF 识别，后续优先在业务代码和文档内迭代。

## 当前架构边界

- `CompactWifiBoard` 是板级入口，负责创建任务和设备对象。
- `SerialHmi` 是唯一 TJC UART 所有者，负责初始化 UART、发送命令、解析事件和按页刷新控件。
- `Dht11Sensor` 只负责 DHT11 单总线读取。
- `Mq135Sensor` 只负责 MQ135 ADC 读取和演示级等级估算。
- `SmartHomeController` 只负责智能家居执行器状态、PWM 输出、自动/节能模式和 MCP 工具；不要把这些控制逻辑重新塞回 `compact_wifi_board.cc`。
- `SmartHomePolicy` 是自动/节能档位规则的纯 C++ 模块；阈值、回差和自定义规则优先在这里修改并补原生测试。
- `SerialHmi` 的页面切换和批量刷新必须保持在同一个递归互斥事务中，避免恢复命令与控件更新交错。
- 设备端 MCP 是家居控制权威入口；PC MCP 桥默认使用 `external` 模式，只提供天气、新闻和室内外组合建议。
- `SmartHomeHttpServer` 只允许在 WiFi 已连接后启动。不要在 `CompactWifiBoard()` 构造函数里调用 `smart_home_http_.Start()`，否则会在 lwIP/tcpip 未就绪时触发 `Invalid mbox` 重启。
- 排查外设不动作时，先看 monitor 是否同时有 `Screen event` 和 `SmartHome: Apply ... output`。前者说明 HMI 事件进入，后者说明控制器已经写 GPIO/PWM；如果出现 `Brownout detector was triggered`，先处理供电，不要继续按软件事件排查。
- `utf8_to_gbk.*` 和 `gbk_table.inc` 只服务于屏幕中文显示，不负责 UI 逻辑。

不要把队友 `work1/compact_wifi_board.cc` 中的直接 UART 初始化代码整段复制进来，否则会和当前 `SerialHmi` 冲突。

## 页面开发原则

当前页面布局来源是：

```text
D:/QQ/serial_warm_home .HMI
D:/QQ/serial_warm_home  (1).HMI
```

固件侧只更新控件值，不负责画页面结构。接续开发时请保持这个方向：

- 在 USART HMI 编辑器里新增或调整控件。
- 控件名同步更新 `main/boards/bread-compact-wifi/serial_hmi_widgets.json`。
- 固件只通过 `SerialHmi::SetText()`、`SerialHmi::SetValue()` 写控件属性。
- 不要把 `cls/fill/draw/xstr` 整页绘制逻辑重新作为默认启动路径。
- 当前页面推荐采用手机式图标入口：page0 首页，page1 空气评分详情，page2 智能家居，page3 AI 与设置。
- page1 当前只按 DHT11 和 MQ135 的真实能力展示：空气评分、空气等级、MQ135 原始值、温度、湿度和舒适度；不要再把 PM2.5、CO2、TVOC 作为当前必备控件。
- `c_air` 曲线当前由固件缓存最近 30 条空气评分，进入 `page1` 后执行 `cle 12,0` 并回放 `add 12,0,val`。如果曲线不显示，先确认 `c_air` 数字 ID 是否仍为 12。
- `t_comfort`、`t_advice`、`air_state` 和手动环境模式由 `SmartHomeController` 统一计算，不要在 `SerialHmi` 里重复写一套舒适度规则。
- 当前编辑器只有“触摸热区”，且命名限制 14 个字符；热区对象名用 `hs_*`，事件字符串才是固件真正读取的内容。
- 左右滑动效果优先用边缘触摸热区发送 `BTN,PAGE,NEXT` 和 `BTN,PAGE,PREV` 实现。

如果确实要恢复运行时绘图，需要先确认不会覆盖手工 HMI 页面，并在文档中明确切换原因。

## HMI 后台脚本边界

已新增 `scripts/hmi_background_probe.ps1` 用于 HMI 资源级探测。它当前可以：

- 扫描 `.HMI` 内嵌 PNG 资源。
- 生成轻量 page1 背景。
- 在测试副本 `tmp_hmi_probe_page1_bg_replace.HMI` 中替换旧 page1 背景槽位。

它当前不能安全完成：

- 新增或重命名 HMI 控件。
- 写入触摸热区事件。
- 创建滑动动画。
- 自动记录曲线控件数字 ID。

所有后台脚本实验必须继续只作用于副本；原始 `D:/QQ/serial_warm_home .HMI` 只能通过 USART HMI 编辑器保存修改。

## 触摸事件约定

`SerialHmi::PollEvent()` 当前可以解析以下格式：

```text
BTN,PAGE,<target>
BTN,DEVICE,<target>,<action>
BTN,MODE,<target>,<action>
SWIPE,<LEFT|RIGHT>
```

当前推荐事件：

```text
BTN,PAGE,AIR_DETAIL
BTN,PAGE,SMART_HOME
BTN,PAGE,AI
BTN,PAGE,SETTINGS
BTN,PAGE,NEXT
BTN,PAGE,PREV
BTN,DEVICE,AIR_PURIFIER,TOGGLE
BTN,DEVICE,FAN,TOGGLE
BTN,DEVICE,HUMIDIFIER,TOGGLE
BTN,MODE,AUTO,TOGGLE
BTN,MODE,ECO,TOGGLE
BTN,ENV,MANUAL,TOGGLE
BTN,ENV,SCENE,GOOD
BTN,ENV,SCENE,HOT
BTN,ENV,SCENE,DRY
BTN,ENV,SCENE,POLLUTED
```

在 USART HMI 编辑器里，命令应写在右侧 `事件 -> 弹起事件(0)`，不要写到左侧 `输出`；不要勾选 `发送键值`。

`BTN,DEVICE,FAN,TOGGLE` 是屏幕侧事件名，固件内部仍使用 `fresh_air_level` 保存新风/风扇档位。当前方案为 `GPIO21` 180°角度舵机扇叶，后台 `fresh_air_servo` 任务按档位往复摆动；后续通过 `SmartHomeController::SetServoProfileForLevel()` 校准角度范围、步进和延时。

`BTN,ENV,...` 是手动模拟环境事件，用于无法制造真实高温、干燥或污染环境时验证自动模式。推荐放在 `page3`，热区名使用 `hs_env_m`、`hs_env_good`、`hs_env_hot`、`hs_env_dry`、`hs_env_bad`。

## 队友 work1 整合状态

已整合：

- `utf8_to_gbk.h`
- `utf8_to_gbk.cc`
- `gbk_table.inc`
- DHT11 开漏上拉和临界区读取时序优化

暂不直接整合：

- `work1/compact_wifi_board.cc` 的直接 UART 串口屏实现。
- `TjcChatDisplay`、`g_ask`、`g_answer`、`g_logs` 问答/日志控件方案。

暂不整合原因：

- 当前 HMI 控件契约尚未包含 `g_ask/g_answer/g_logs`。
- 当前 `SerialHmi` 已承担 UART 和事件轮询，不能再引入第二套 UART 驱动。
- AI 聊天显示需要先在 HMI 工程中增加对应控件，再通过 `SerialHmi` 封装接口接入。

## 构建注意事项

当前板型目录下的 `.cc/.c` 会被 `main/CMakeLists.txt` 自动收集。新增源文件放在：

```text
main/boards/bread-compact-wifi
```

如果新增代码依赖新的 ESP-IDF 组件，需要把组件名加入 `main/CMakeLists.txt` 的 `PRIV_REQUIRES`。

构建前先使用本机 ESP-IDF 安装提供的导出脚本或 VS Code Espressif 扩展初始化终端，不要复制历史文档中的固定盘符。例如：

```powershell
& '<你的 ESP-IDF 安装目录>\export.ps1'
idf.py build
```

不要为了验证随意运行 `idf.py set-target` 或 `menuconfig`，这些会改写受保护配置。
2026-07-28 已使用 ESP-IDF `5.5.3` 完整构建通过，结果见 `docs/phase-handoff-2026-07-28-software-reliability.md`。

## 硬件注意事项

- TJC 屏幕：ESP32 `GPIO41/TX` 接屏幕 RX，ESP32 `GPIO42/RX` 接屏幕 TX，必须共地。
- TJC 波特率：当前固件和 HMI 工程都应为 `9600`。
- DHT11：DATA 接 `GPIO18`，建议外部 4.7k-10k 上拉。
- MQ135：AO 接 `GPIO1 / ADC1_CH0`，必须保证 ADC 输入不超过 3.3V。
- 180°角度舵机扇叶：信号线接 `GPIO21`，红线接稳定 5V，棕/黑线接 GND，并与 ESP32S3 共地；首次测试先限制摆幅，确认扇叶不会碰撞结构。
- 麦克风和功放仍占用 I2S 引脚，后续换传感器引脚时先查 `config.h`。

## 当前遗留问题

- HMI 工程已提交到仓库 `hmi/`；旧文档中 `D:/QQ` 路径只作为历史记录，不再是唯一交付源。
- PM2.5、CO2、TVOC 目前没有真实传感器；page1 已改为评分页，后续接入真实硬件后再新增浓度控件和固件刷新逻辑。
- MQ135 空气质量算法只是演示阈值，后续需要标定。
- 智能家居事件已控制现有外设，但 180°舵机扇叶的安全摆幅仍需要实机校准。
- 雷达和光敏驱动已经接入代码，并新增健康状态、雷达坐标和区域输出；仍需烧录后进行实机数据验收。独立灯仍只有逻辑状态和回调接口，不在本轮“只改代码”范围内绑定新 GPIO。
- 队友 AI 聊天显示方案需先补 HMI 控件后再接。

## 2026-07-28 纯软件增强

- `SmartHomeController` 已使用递归互斥锁保护跨任务状态。
- DHT11 缓存有效期为 30 秒，健康接口会报告数据年龄和连续失败次数。
- 手动控制提供 30 分钟设备级覆盖；自动/节能模式状态保存到 NVS。
- 雷达输出最近目标坐标和左/中/右区域，连续 2 分钟无目标后才判定无人。
- 环境突变报警增加连续两次确认和 60 秒冷却。
- HTTP 新增 `/api/health` 与可选 Token，小程序和 MCP 桥接均支持 `X-API-Key`。
- 详细验证和字段说明见 `docs/phase-handoff-2026-07-28-software-reliability.md`。
