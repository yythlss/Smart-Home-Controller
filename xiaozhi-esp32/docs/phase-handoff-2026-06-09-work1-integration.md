# 阶段交付：work1 队友代码整合与工程梳理

日期：`2026-06-09`

## 1. 本阶段目标

把队友目录：

```text
E:/espwork/xiaozhi-esp32/work1/bread-compact-wifi
```

中的有效代码整合进当前工程：

```text
E:/espwork/xiaozhi-esp32/xiaozhi-esp32
```

并完成当前板型目录的文件梳理、冗余清理、中文注释补充和交付文档更新。

## 2. 开发前检查结果

已按协作规则先做只读检查：

- 已阅读 `E:/espwork/AGENTS.md`。
- 已检查当前工程工作树状态。
- 已检查当前板型目录 `main/boards/bread-compact-wifi`。
- 已阅读 `docs/current-project-handoff.md`、`docs/project-file-map.md`、`docs/continuation-notes.md`。
- 已检查 `serial_hmi.*`、`compact_wifi_board.cc`、`dht11_sensor.*`、`mq135_sensor.*`、`serial_hmi_widgets.json`。
- 已检查队友目录 `work1/bread-compact-wifi`。
- 已确认受保护配置文件 `.vscode/**`、`sdkconfig*`、`CMakePresets.json` 本阶段未修改。

## 3. 整合决策

### 已整合

| 来源 | 当前去向 | 说明 |
| --- | --- | --- |
| `work1/utf8_to_gbk.h` | `main/boards/bread-compact-wifi/utf8_to_gbk.h` | 更新注释后保留 |
| `work1/utf8_to_gbk.cc` | `main/boards/bread-compact-wifi/utf8_to_gbk.cc` | 保留二分查表转换实现 |
| `work1/gbk_table.inc` | `main/boards/bread-compact-wifi/gbk_table.inc` | 保留生成表，供屏幕中文显示 |
| `work1/dht11_sensor.cc` 中的时序优化 | `main/boards/bread-compact-wifi/dht11_sensor.cc` | 改为开漏上拉、20ms 起始信号、临界区读取 40bit |

### 未直接整合

| 来源 | 不直接整合原因 |
| --- | --- |
| `work1/compact_wifi_board.cc` 整文件 | 它直接安装 UART 驱动并发送屏幕命令，会与当前 `SerialHmi` 的 UART 所有权、事件轮询和页面缓存刷新逻辑冲突 |
| `TjcChatDisplay` / `g_ask` / `g_answer` / `g_logs` | 当前 HMI 控件契约没有这些控件；后续需要先在 HMI 编辑器中新增控件，再通过 `SerialHmi` 封装接入 |

## 4. 当前代码框架

当前板型核心代码职责如下：

| 文件 | 职责 |
| --- | --- |
| `compact_wifi_board.cc` | 板级入口；初始化 NoDisplay、SerialHmi、按键、传感器任务和屏幕事件任务 |
| `serial_hmi.cc/.h` | TJC UART 初始化、GBK 转码发送、页面切换、控件刷新和事件解析 |
| `utf8_to_gbk.cc/.h` | UTF-8 到 GBK 转码 |
| `gbk_table.inc` | Unicode 到 GBK 映射表，生成数据 |
| `dht11_sensor.cc/.h` | DHT11 单总线温湿度读取 |
| `mq135_sensor.cc/.h` | MQ135 ADC 原始值读取和演示级空气等级 |
| `config.h` | 当前板型硬件引脚和串口参数 |
| `serial_hmi_widgets.json` | HMI 控件和事件契约 |

关键约束：

- `SerialHmi` 是当前板型唯一的 TJC UART 所有者。
- 固件不恢复 `cls/fill/draw/xstr` 整页绘图默认路径。
- 页面布局、图标、触摸热区和滑动效果继续由 `D:/QQ/serial_warm_home .HMI` 维护。

## 5. 本阶段改动文件

### 代码

- `main/boards/bread-compact-wifi/serial_hmi.cc`
  - 引入 `utf8_to_gbk.h`。
  - `SendCommand()` 发往屏幕前转 GBK。
  - USB monitor 继续打印 UTF-8 原文。
- `main/boards/bread-compact-wifi/utf8_to_gbk.h`
  - 新增文件，修正注释为完整 GBK 表方案。
- `main/boards/bread-compact-wifi/utf8_to_gbk.cc`
  - 新增文件，来自队友 `work1`。
- `main/boards/bread-compact-wifi/gbk_table.inc`
  - 新增文件，来自队友 `work1`。
- `main/boards/bread-compact-wifi/dht11_sensor.cc`
  - 整合队友 DHT11 开漏上拉与临界区读数方案。
- `main/boards/bread-compact-wifi/compact_wifi_board.cc`
  - 补充关键中文注释，明确任务边界和 UART 所有权。

### 文档/协作记录

- `E:/espwork/AGENTS.md`
  - 新增“每完成阶段任务也必须写中文交付文档”的规则。
- `task_plan.md`
  - 新增本阶段任务计划。
- `findings.md`
  - 新增本阶段发现记录。
- `progress.md`
  - 新增本阶段进度记录。
- `docs/current-project-handoff.md`
  - 更新为 2026-06-09 当前状态。
- `docs/continuation-notes.md`
  - 重写接续开发注意事项。
- `docs/project-file-map.md`
  - 重写当前有效文件索引和清理说明。
- `../文档/串口屏与环境监测项目交付说明.md`
  - 更新队友交付说明。

### 已删除

- `main/boards/bread-compact-wifi/ui_mockups/`
- `main/boards/bread-compact-wifi/tjc_screen_implementation_plan.md`

删除原因：旧设计稿和旧运行时绘图方案已被当前 `ui_assets/`、`串口屏设计说明.md`、`串口屏手动事件配置手册.md` 替代。

## 6. 当前有效交付文件

需要发给队友或提醒队友阅读：

```text
E:/espwork/AGENTS.md
E:/espwork/xiaozhi-esp32/xiaozhi-esp32/docs/current-project-handoff.md
E:/espwork/xiaozhi-esp32/xiaozhi-esp32/docs/continuation-notes.md
E:/espwork/xiaozhi-esp32/xiaozhi-esp32/docs/project-file-map.md
E:/espwork/xiaozhi-esp32/xiaozhi-esp32/docs/phase-handoff-2026-06-09-work1-integration.md
D:/Project/VSCode-project/xiaozhi-esp32/文档/串口屏与环境监测项目交付说明.md
D:/Project/VSCode-project/xiaozhi-esp32/文档/串口屏手动事件配置手册.md
D:/Project/VSCode-project/xiaozhi-esp32/文档/硬件连接与软件验证步骤.md
D:/Project/VSCode-project/xiaozhi-esp32/文档/串口屏设计说明.md
E:/espwork/xiaozhi-esp32/xiaozhi-esp32/main/boards/bread-compact-wifi/serial_hmi_widgets.json
D:/QQ/serial_warm_home .HMI
```

## 7. 验证结果

JSON 校验：

```powershell
python -m json.tool E:\espwork\xiaozhi-esp32\xiaozhi-esp32\main\boards\bread-compact-wifi\serial_hmi_widgets.json > $null
```

结果：通过。

控件名长度检查：

```text
all widget names <= 14 chars
```

结果：通过。

受保护文件检查：

```powershell
git status --short -- .vscode sdkconfig sdkconfig.defaults sdkconfig.defaults.esp32 sdkconfig.defaults.esp32s3 CMakePresets.json
```

结果：无输出，表示本阶段未修改受保护配置文件。

ESP-IDF 构建：

```powershell
& 'D:\esp\Espressif\frameworks\esp-idf-v5.5.2\export.ps1'
idf.py -B build_codex_check reconfigure
idf.py -B build_codex_check build
```

结果：通过。

```text
Project build complete.
xiaozhi.bin binary size 0x23efc0 bytes.
Smallest app partition is 0x3f0000 bytes.
0x1b1040 bytes (43%) free.
```

说明：第一次直接 `idf.py -B build_codex_check build` 曾在链接阶段失败，原因是旧构建目录没有重新配置，`utf8_to_gbk.cc` 未进入 `build.ninja`。执行 `reconfigure` 后构建通过。

## 8. 遗留问题

- `D:/QQ/serial_warm_home .HMI` 不在 Git 工程内，必须单独交付。
- PM2.5、CO2、TVOC 暂无真实传感器。
- MQ135 当前是演示级阈值估算，后续需要标定。
- 智能家居控制事件已解析，但没有真实外设动作。
- 队友 AI 问答显示方案暂未接入，需先新增 HMI 控件并确认控件命名是否满足编辑器限制。

## 9. 下一阶段目标

1. 在 USART HMI 编辑器打开 `D:/QQ/serial_warm_home .HMI`。
2. 按 `串口屏手动事件配置手册.md` 检查每个触摸热区的 `弹起事件(0)`。
3. 编译并下载 HMI 工程到串口屏。
4. 烧录本阶段固件，打开 monitor。
5. 验证中文显示、页面切换、边缘热区模拟滑动、DHT11 和 MQ135 读数。
6. 决定是否新增 AI 聊天控件并接入 `TjcChatDisplay` 思路。

## 10. 人工必须完成事项

硬件和 HMI 编辑器相关事项无法由代码自动完成，需人工执行：

- 手动打开 `D:/QQ/serial_warm_home .HMI`。
- 确认工程波特率为 `9600`。
- 确认 `page0-page3` 页面编号和固件一致。
- 确认所有数据控件名与 `serial_hmi_widgets.json` 一致。
- 确认所有触摸热区命令写在 `事件 -> 弹起事件(0)`。
- 确认未勾选 `发送键值`。
- 把 HMI 工程下载到串口屏。
- 接线并烧录 ESP32 后实机验证。
