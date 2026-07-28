# 2026-07-19 光敏方向修正与 LD2450 线束准备阶段交付

## 1. 本阶段目标

根据实机串口日志修正光敏模块亮度百分比方向，并明确 HLK-LD2450 在尚未具备可用线束时的购买和接线边界，避免错误接线或买错连接器。

## 2. 实机现象与根因

用户已完成光敏模块接线，串口能够稳定输出 ADC 原始值和亮度百分比，例如：

```text
SmartHome: Ambient light updated: 2.5%
Light : OK raw=63 brightness=2.5%
```

实测现象为：遮光时 `raw` 上升，但旧固件把 `raw` 越大解释为环境越亮，所以遮光时屏幕百分比反而上升。

根因是初始默认标定使用了：

```cpp
DARK_RAW = 300
BRIGHT_RAW = 3300
```

该默认值适用于“越亮 ADC 越高”的分压方向；当前实际光敏模块的 `AO` 输出方向相反。`AmbientLightFilter` 的归一化公式本身已经支持反向标定，因此不需要重接 AO，也不需要改自动开灯阈值，只需正确填写暗、亮的真实原始值。

## 3. 已完成的修改

### 3.1 光敏默认标定方向

已将 `main/boards/bread-compact-wifi/config.h` 中的默认值调整为：

```cpp
#define AMBIENT_LIGHT_DARK_RAW   3300
#define AMBIENT_LIGHT_BRIGHT_RAW 300
```

这意味着：

- 遮住光敏模块时，`raw` 接近 `DARK_RAW`，`brightness` 下降到接近 `0%`。
- 照亮光敏模块时，`raw` 接近 `BRIGHT_RAW`，`brightness` 上升到接近 `100%`。
- 原有自动灯光规则保持不变：有人且亮度不高于 `25%` 时 `light_on=true`；亮度达到 `35%` 以上时 `light_on=false`。

### 3.2 回归保护

已向 `tests/test_bread_compact_wifi_regressions.py` 增加默认标定方向测试，要求：

```text
AMBIENT_LIGHT_DARK_RAW > AMBIENT_LIGHT_BRIGHT_RAW
```

它先在旧配置上失败，失败信息为 `300 not greater than 3300`；修改配置后通过。以后若再次把两个值写反，回归测试会直接指出问题。

### 3.3 LD2450 线束说明

已更新 `../文档/LD2450雷达与光敏模块接线验证.md`：

- LD2450 模块端不是普通 `2.54 mm` 杜邦排针，不能硬插普通杜邦母头。
- 用户提供的官方教程只确认模块端四线为 `5V`、`RX`、`TX`、`GND`，未标注连接器针距和端子型号。
- 首选购买店铺明确标注的“`HLK-LD2450 配套 4P 线束`”或“`HLK-LD2450 转接板`”。
- 若商品标题只有“`1.25 mm 4P`”等通用规格，购买前必须让卖家根据模块接口正面照片确认可插入；不能根据猜测下单。
- 收到线束后先离线试插，插头应自然插到底且无明显松动；不能插入时不能强压。

## 4. 改动文件

```text
main/boards/bread-compact-wifi/config.h
../文档/LD2450雷达与光敏模块接线验证.md
tests/test_bread_compact_wifi_regressions.py
docs/current-project-handoff.md
docs/continuation-notes.md
docs/phase-handoff-2026-07-19-ambient-light-polarity-and-radar-cable.md
```

## 5. 已完成验证

### 5.1 测试先失败

在旧默认标定下运行：

```powershell
python -m unittest discover -s tests -p test_bread_compact_wifi_regressions.py -v
```

新增测试按预期失败：

```text
AssertionError: 300 not greater than 3300
```

这证明测试确实覆盖了本次发现的方向错误，而不是无效的测试。

### 5.2 修改后回归测试

同一命令在修正后通过：

```text
Ran 20 tests
OK
```

### 5.3 ESP-IDF 构建

首次构建时发现原有 `build_codex_check` 验证目录已不存在，因此使用同名独立构建目录重新生成，不执行 `set-target` 或 `menuconfig`：

```powershell
& 'D:\esp\Espressif\frameworks\esp-idf-v5.5.2\export.ps1'
idf.py -B build_codex_check build
```

结果：

```text
Project build complete.
xiaozhi.bin binary size 0x2485c0 bytes.
Smallest app partition is 0x3f0000 bytes.
0x1a7a40 bytes (42%) free.
```

生成固件：`build_codex_check/xiaozhi.bin`。本阶段未修改 `.vscode/**`、`sdkconfig*`、ESP-IDF 工具链或全局环境变量。

## 6. 仍需人工完成的步骤

### 6.1 重新烧录并验证光敏方向

1. 在 VSCode 打开 `E:/espwork/xiaozhi-esp32/xiaozhi-esp32`。
2. 使用原有 ESP-IDF 插件执行 Build、Flash、Monitor；不要执行 `set-target` 或 `menuconfig`。
3. 先让光敏模块暴露在室内光线下，等待两次 `Light` 日志，记录 `raw` 和 `brightness`。
4. 用手掌或不透光物遮住光敏元件，等待两次日志。
5. 预期遮光后 `raw` 上升而 `brightness` 下降；照亮后 `raw` 下降而 `brightness` 上升。
6. 若预期成立，记录两种稳定 `raw`：遮光值填入 `AMBIENT_LIGHT_DARK_RAW`，照亮值填入 `AMBIENT_LIGHT_BRIGHT_RAW`。
7. 保存 `config.h` 后再次 Build、Flash、Monitor，确认百分比约束在 `0-100%` 且变化平稳。

不要仅在 HMI 上观察文字，必须同时确认 USB monitor 的 `raw` 与 `brightness` 方向一致。

### 6.2 购买和连接 LD2450 线束

1. 拍摄 LD2450 模块端 4P 插座的正面和侧面照片，要求能看清插座外壳、防呆缺口和丝印。
2. 优先联系购买模块的店铺，直接询问“是否有 HLK-LD2450 配套 4P 线束或转接板”。
3. 若店铺给出候选线束，要求其确认该线束可插入你的 LD2450 模块端，另一端最好为独立杜邦母头或裸线。
4. 收货后断电试插；确认线束方向正确、插到底且不松动。
5. 按信号名称接线，不能按线的颜色接：`5V -> 5V`、`GND -> GND`、`LD2450 TX -> GPIO11`、`LD2450 RX -> GPIO12`。
6. 接线完成才连接 USB 供电，打开 monitor，等待 `LD2450 stats: bytes=... valid=...`。
7. 只有 `bytes` 和 `valid` 都持续增长，才认为线束、供电和 UART 数据链路有效。

## 7. 遗留风险

1. `3300/300` 是根据模块电平方向修正的默认估计，最终值仍要以实际遮光和照亮时的稳定 `raw` 校准。
2. LD2450 官方教程未给出连接器精确针距，不能把任何 `1.25 mm` 通用线直接视为确认兼容。
3. LD2450 尚未实物接入，因此目标帧解析、目标坐标和雷达唤醒未完成物理验证。
4. 当前独立 LED 灯、蜂鸣器、可靠无人自动关闭和主动播报仍未接入实体硬件。

## 8. 下一阶段建议

1. 先完成光敏重新烧录和两组实际 ADC 校准值记录。
2. 收到并接好 LD2450 配套线束后，收集一段 `LD2450 stats` 和目标坐标日志。
3. 再根据门口实测坐标划分区域，设计进入、离开和无人延时策略；在有门磁或室内二次存在确认前，不启用雷达无目标即关机。
