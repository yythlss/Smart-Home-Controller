# 2026-07-18 LD2450 与光敏模块接入阶段交付

## 1. 阶段目标

本阶段的目标是把已到货的 GL5528 类光敏模块和 HLK-LD2450 雷达接入 `bread-compact-wifi` 固件，并提供可复现的硬件连接与验证流程。

## 2. 已完成的功能

### 光敏模块

- `AO` 规划为 `GPIO2 / ADC1_CH1`，仅允许 3.3V 模拟输入。
- 复用 MQ135 已创建的 ADC1 oneshot 句柄，避免同一 ADC 单元重复初始化。
- 新增原始 ADC 到 0-100% 的归一化和指数滤波。
- 初始校准值为暗 `300`、亮 `3300`，现场可在 `config.h` 中按串口原始值调整。
- 现有灯光逻辑改为 25% 开灯、35% 关灯的回差控制，避免阈值附近频繁切换。

### LD2450 雷达

- 使用 `UART1`，ESP32 RX=`GPIO11`、TX=`GPIO12`，速率 `256000`，不占用 TJC 屏的 UART2。
- 新增数据帧缓存、帧头/帧尾校验和三个目标的 X/Y/速度/距离解析。
- 雷达日志每秒最多输出一次目标细节，每五秒输出一次累计收包统计，避免再次造成串口输出过量。
- 目标数量大于 0 时调用 `UpdateRadarObservation()`，状态切换为有人时触发已有的 AI 开麦回调。
- 目标数量为 0 时仅更新状态，不调用 `UpdatePresence(false)`，因此不会误关净化器、新风、加湿器和灯光。

## 3. 修改文件

```text
main/boards/bread-compact-wifi/config.h
main/boards/bread-compact-wifi/mq135_sensor.h
main/boards/bread-compact-wifi/ambient_light_filter.h
main/boards/bread-compact-wifi/ambient_light_filter.cc
main/boards/bread-compact-wifi/ambient_light_sensor.h
main/boards/bread-compact-wifi/ambient_light_sensor.cc
main/boards/bread-compact-wifi/ld2450_protocol.h
main/boards/bread-compact-wifi/ld2450_protocol.cc
main/boards/bread-compact-wifi/ld2450_sensor.h
main/boards/bread-compact-wifi/ld2450_sensor.cc
main/boards/bread-compact-wifi/smart_home_controller.h
main/boards/bread-compact-wifi/smart_home_controller.cc
main/boards/bread-compact-wifi/compact_wifi_board.cc
tests/test_bread_compact_wifi_regressions.py
../文档/LD2450雷达与光敏模块接线验证.md
docs/superpowers/plans/2026-07-18-ld2450-ambient-light-integration.md
docs/phase-handoff-2026-07-18-ld2450-ambient-light-integration.md
```

## 4. 已完成验证

### Python 回归测试

命令：

```powershell
python -m unittest discover -s tests -p test_bread_compact_wifi_regressions.py -v
```

结果：19 项通过，包含新增的：

- 光敏与 LD2450 GPIO/模块接入契约。
- 雷达状态字段和亮度回差契约。
- 雷达接收字节数、有效帧数和拒绝帧数诊断契约。

### ESP-IDF 构建

命令：

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build_codex_check.ps1
```

结果：构建成功。

```text
Successfully created esp32s3 image.
Generated build_codex_check/xiaozhi.bin
xiaozhi.bin binary size 0x2485c0 bytes.
Smallest app partition is 0x3f0000 bytes.
0x1a7a40 bytes (42%) free.
```

本阶段没有修改 `.vscode`、`sdkconfig`、`sdkconfig.defaults`、工具链或系统环境变量。

## 5. 需要人工完成的事项

1. 按 [硬件连接和验证手册](../../文档/LD2450雷达与光敏模块接线验证.md) 完成接线。
2. 使用实际串口确认 `LD2450 stats` 中 `bytes` 和 `valid` 均增长。
3. 记录完全遮光和明亮环境下的光敏 `raw`，替换 `AMBIENT_LIGHT_DARK_RAW` 与 `AMBIENT_LIGHT_BRIGHT_RAW`。
4. 使用 `/api/state` 或小程序确认 `ambient_light_percent`、`radar_target_count`、`occupied`、`light_on` 的变化。
5. 记录从门外进入、门内离开的雷达坐标日志，为后续进出方向和人数估计提供数据。

## 6. 当前限制与风险

1. 当前没有单独的物理照明 LED/MOSFET 输出，`light_on` 是已可验证的逻辑状态；不得把板载 AI 状态灯 GPIO48 当作正式照明负载。
2. 当前没有门磁或室内存在雷达，不能仅凭“门口无目标”判定全屋无人。
3. 无人自动关闭保持安全关闭状态，待真实门口坐标、进出方向和延时策略完成后再启用。
4. 用户提供的 LD2450 PDF 位于当前受限执行环境不可读取的 D 盘路径；实现采用模块默认 256000 baud 和标准目标帧格式，并通过实际收包统计支持现场核对。若 `bytes` 增长而 `valid=0`，应以 PDF 的实际帧格式或模块运行模式为准继续调整。
5. 本机没有可执行的主机 C++ 编译器，因此纯 C++ 主机测试未运行；新增接口使用现有 Python 契约回归测试和真实 ESP-IDF 构建验证。

## 7. 下一阶段建议

1. 收集一段真实 LD2450 串口日志，确认帧解析和 X/Y 坐标方向。
2. 根据门口位置划分门外、过渡和门内三个区域。
3. 实现进入/离开方向状态机和人数估计。
4. 增加门磁或室内 LD2410C 后，再启用 2-5 分钟无人自动关闭。
5. 确认独立 LED 灯的电压和电流后，接入 MOSFET 驱动并注册正式灯光输出回调。
