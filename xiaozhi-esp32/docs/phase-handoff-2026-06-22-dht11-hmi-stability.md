# 2026-06-22 DHT11 与串口屏显示稳定性阶段交付

## 当前工程状态

- 工程目录：`E:/espwork/xiaozhi-esp32/xiaozhi-esp32`
- 当前板型：`CONFIG_BOARD_TYPE_BREAD_COMPACT_WIFI=y`
- 当前芯片：`esp32s3`
- 当前 HMI 工程文件：`D:/QQ/serial_warm_home .HMI`
- 当前串口屏控件契约：`main/boards/bread-compact-wifi/serial_hmi_widgets.json`
- 本阶段未修改 `.vscode/**`、`sdkconfig`、`sdkconfig.defaults*`、`CMakePresets.json`、ESP-IDF 工具链和 HMI 二进制文件。

## 本阶段问题判断

串口日志中出现：

```text
DHT11: Checksum error: 159+0+12+128=43 != 172
DHT11  : FAIL  temp=-99.0 C  humi=-1.0%
```

`159%` 湿度和 `128` 温度小数字节都不符合 DHT11 的有效输出范围，说明根因首先在 DHT11 单总线读数被扰乱，常见来源是上拉不足、DATA 线接线不稳、电源或地线问题、时序被干扰。固件侧需要做防护，避免偶发错误帧或失败帧进入显示层。

串口屏显示短暂出现和闪烁的问题，固件侧没有主动清空 `t_temp`、`t_humi` 控件。更可能的链路是：DHT11 单次成功后下一次失败导致固件发 `--`，或 HMI 侧页面/热区事件重复触发切页，造成页面重载和控件刷新时序不稳定。本阶段先在固件端做最小保护，HMI 侧仍需要实机确认。

## 已改动文件

- `main/boards/bread-compact-wifi/dht11_sensor.cc`
  - 增加 `ValidateReading()`。
  - 校验 DHT11 小数字节、湿度和温度范围。
  - 对离谱帧输出 `Range error`，并返回读取失败，不更新内部温湿度。

- `main/boards/bread-compact-wifi/compact_wifi_board.cc`
  - 在 `SensorTask()` 中缓存上一组有效 DHT11 温湿度。
  - 单次 DHT11 失败时，如果已有有效读数，则继续向串口屏发送上一组有效值，避免显示被短暂清成 `--`。
  - 日志增加 `Use cached DHT11 reading after transient failure`，便于确认是否进入缓存路径。

- `main/boards/bread-compact-wifi/serial_hmi.cc`
  - `ShowPage()` 增加同页保护：当前页重复请求不再发送新的 `page <id>` 命令。
  - 切页后等待 `80 ms` 再刷新当前页控件，降低页面尚未加载完成时控件命令丢失或闪动的概率。

- `main/boards/bread-compact-wifi/serial_hmi.h`
  - `current_page_id_` 初始值改为 `-1`，确保开机第一次 `ShowPage(0)` 仍会真正发送 `page 0`。

- `tests/test_bread_compact_wifi_regressions.py`
  - 新增源码级回归测试，覆盖 DHT11 合理性校验、失败时保留上一组有效读数、串口屏切页防闪保护。

## 验证结果

已通过：

```powershell
python -m unittest discover -s tests -v
```

结果：

```text
Ran 3 tests
OK
```

ESP-IDF 构建未能完成确认：

```powershell
& 'D:\esp\Espressif\frameworks\esp-idf-v5.5.2\export.ps1'
idf.py -B build_codex_check build
```

结果：`ninja` 在编译 `compact_wifi_board.cc` 第一步失败，日志中没有 C++ 语法诊断，失败点为 `ccache ... xtensa-esp32s3-elf-g++` 进程异常退出，`idf.py` 报告 `ninja failed with exit code 3221226356`。

尝试关闭 ccache 并重新构建时，沙箱拦截了 ESP-IDF 构建进程；申请非沙箱执行时审批系统返回 404，因此本轮不能继续完成编译验证。

建议在本机正常 ESP-IDF 终端中执行：

```powershell
& 'D:\esp\Espressif\frameworks\esp-idf-v5.5.2\export.ps1'
idf.py -B build_codex_check -DCCACHE_ENABLE=0 reconfigure
idf.py -B build_codex_check build
```

## 实机验证重点

烧录新固件后观察 monitor：

- DHT11 错误帧应继续出现 `Checksum error` 或新增 `Range error`，但不应把湿度大于 `100%` 的值发到屏幕。
- 如果至少有一次 DHT11 读数成功，后续偶发失败时应看到：

```text
Use cached DHT11 reading after transient failure
```

- 串口屏命令中，温湿度应保持上一组有效值，直到新有效值覆盖旧值。
- 若 HMI 仍闪屏，重点检查 HMI 编辑器：
  - `t_temp`、`t_humi`、`t_temp_d`、`t_humi_d` 是否是真实文本控件，且名字完全一致。
  - 页面初始化事件、定时器事件是否写了清空文本或重载页面的命令。
  - 透明热区是否只在 `弹起事件(0)` 里发送 `prints ...` 和 `printh 0a`。
  - 不要勾选 `发送键值`。
  - 左右翻页热区不要覆盖图标热区或返回热区，避免重复触发 `BTN,PAGE,NEXT/PREV`。

## 需要人工确认

- DHT11 DATA 是否接 `GPIO18`。
- DHT11 DATA 是否有 `4.7k-10k` 外部上拉到 `3.3V`，不要只依赖 ESP32 内部弱上拉。
- DHT11 供电、GND 是否稳定并与 ESP32 共地。
- TJC 串口屏接线是否与当前固件一致：ESP32 `GPIO41/TX` 接屏幕 `RX`，ESP32 `GPIO42/RX` 接屏幕 `TX`，波特率 `9600`。
- 当前屏幕内下载的 HMI 工程是否确实来自 `D:/QQ/serial_warm_home .HMI` 的最新编译产物。

## 下一阶段建议

1. 先完成 `CCACHE_ENABLE=0` 的本地构建验证。
2. 烧录后连续观察 2-3 分钟 DHT11 和 `[TJC]` 日志。
3. 若仍高频 `Checksum error`，优先加外部上拉、缩短 DATA 线、确认 3.3V 供电和共地。
4. 若温湿度日志稳定但屏幕仍闪，回到 USART HMI 编辑器检查页面事件、定时器和热区覆盖关系。

## 2026-06-22 续查补充

### 新增现象判断

用户补充确认：即使停留在同一个页面，不做不同页面切换，串口屏也会闪烁，温湿度数据仍表现为刷新时短暂出现、随后消失。因此页面快速切换不是主因，排查重点转为当前页控件刷新链路和 HMI 页面自身绘制行为。

### 本次补充检查

- 固件侧同一页刷新没有发现 `cls`、`fill`、`draw`、`xstr` 等整屏重绘命令。
- 当前页每次传感器刷新会连续写入多个控件，例如首页会写 `t_ai_state`、`t_temp`、`t_humi`、`t_air_state`、`t_air`、`j_air`、`t_advice`。
- 只读扫描 `D:/QQ/serial_warm_home .HMI`：
  - 没有直接扫到 `cls`、`fill`、`timer`、`vis`、`ref` 这类明显自动清屏或定时器清空命令。
  - 能看到 `t_temp`、`t_humi`、`t_temp_d`、`t_humi_d` 等控件名。
  - 能看到 HMI 内部仍存在若干直接 `page` 命令和 `page4` 记录，后续如果还有无触摸自动跳动，需要继续在 USART HMI 编辑器里人工核对页面和热区事件。

### 本次新增改动

- `main/boards/bread-compact-wifi/serial_hmi.cc`
  - 在 `RefreshCurrentPage()` 中增加批量刷新包裹。
  - 当前页控件写入前发送 `ref_stop`，控件写完后发送 `ref_star`，减少同一页多个控件逐个重绘造成的闪烁。
  - 保留前面添加的快速切页防抖，但这只是辅助，不再视为同页闪烁主因。
- `main/boards/bread-compact-wifi/serial_hmi.h`
  - 新增 `BeginBatchRefresh()`、`EndBatchRefresh()` 私有方法声明。
- `tests/test_bread_compact_wifi_regressions.py`
  - 串口屏回归测试增加对 `ref_stop`、`ref_star` 批量刷新流程的检查。

### 验证结果

已通过源码级回归测试：

```text
python -m unittest discover -s tests -v
Ran 4 tests
OK
```

ESP-IDF 新构建仍未能在当前 Codex 沙箱内完成。上一次构建日志停在 `ccache` 调用 ESP32-S3 C++ 编译器阶段，日志没有给出正常 C++ 语法诊断；本轮申请非沙箱构建被审批系统拒绝，不能绕过执行。因此本阶段不能声称固件完整编译通过。

### 下一步实机重点

1. 在本机 ESP-IDF 终端关闭 ccache 后重新构建，确认是否还有真实 C++ 编译错误。
2. 烧录后观察同页停留状态：
   - `[TJC] ref_stop` 和 `[TJC] ref_star` 应包住当前页控件更新。
   - 温湿度数据不应被 DHT11 失败帧覆盖成异常值或短暂空白。
   - 如果数据仍然只显示约 1 秒，优先回到 USART HMI 编辑器检查文本控件是否被背景图、滑动容器、动画控件或隐藏页面对象覆盖。
3. HMI 编辑器人工重点核对：
   - `t_temp`、`t_humi` 是否是当前页最上层真实文本控件。
   - 是否存在重复同名控件、隐藏控件、透明热区或页面动画控件覆盖在文本之上。
   - 页面或控件事件中是否有直接 `page` 命令、背景重绘、隐藏显示或清空文本的逻辑。
   - 不需要的 `page4` 或旧页面对象是否仍参与当前下载工程。

## 2026-06-22 DHT11 实机日志后续修复

### 新增实机证据

用户烧录后 monitor 连续出现：

```text
DHT11: Range error: raw=162,0,12,131 humidity=162.0 temp=25.1
DHT11: Checksum error: 162+0+12+129=47 != 176
```

这说明 DHT11 并不是完全无响应，而是每轮都读到了 5 字节但数据稳定错位。屏幕温湿度显示 `--` 是固件防护逻辑生效：没有任何一次有效 DHT11 数据时，不向屏幕发送异常温湿度。

### 根因判断

`dht11_sensor.cc` 在完成 DHT11 应答检测 `80us 低电平 + 80us 高电平` 后，直接进入 40 位数据读取；少了一步等待应答高电平结束。这样第一个数据位会被应答高电平污染，后续 40 位整体错位，容易得到 `162,0,12,128/129` 这类稳定但不合理的数据。

### 本次改动

- `main/boards/bread-compact-wifi/dht11_sensor.cc`
  - 在 `No response (high)` 检测后新增 `WaitForLevel(0, 200)`。
  - 如果应答高电平没有结束，记录 `Response high timeout` 并返回失败。
  - 确保进入 40 位数据循环时，已经处于第一个数据位起始低电平阶段。
- `tests/test_bread_compact_wifi_regressions.py`
  - 新增 `test_dht11_driver_consumes_response_high_before_data_bits`，防止后续再次漏掉该时序步骤。

### 验证结果

已通过回归测试：

```text
python -m unittest discover -s tests -v
Ran 5 tests
OK
```

已通过低并发固件构建：

```powershell
& 'D:\esp\Espressif\frameworks\esp-idf-v5.5.2\export.ps1'
ninja -C build_codex_check -j 1
```

结果：

```text
Successfully created esp32s3 image.
Generated E:/espwork/xiaozhi-esp32/xiaozhi-esp32/build_codex_check/xiaozhi.bin
xiaozhi.bin binary size 0x23f350 bytes. Smallest app partition is 0x3f0000 bytes. 0x1b0cb0 bytes (43%) free.
```

### 下一步实机验证

重新烧录 `build_codex_check` 后观察：

- 如果 DHT11 接线和上拉正常，应开始出现 `DHT11  : OK temp=... humi=...`。
- 屏幕日志应从 `t_temp.txt="-- C"`、`t_humi.txt="-- %"` 变成真实数值。
- 若仍然 `Range error` 或 `Checksum error`，再重点检查 DHT11 DATA 是否接 `GPIO18`、是否有 `4.7k-10k` 外部上拉到 `3.3V`、供电和 GND 是否稳定。
- `Unknown screen event` 是串口屏 TX 回传的杂散/非协议数据，和 DHT11 温湿度无效是两个问题；温湿度先以 DHT11 OK 为验收标准。

## 2026-06-22 HMI 控件状态调试观察

### 新增人工观察

用户在 USART HMI 编辑器中修改串口屏设计文件的控件状态后，调试界面中已经能看到数据保持上一组显示，不再在下一次数据到来前回到初始状态。

这说明当前“数据只显示一会儿、随后回初始状态”的主因很可能在 HMI 工程内的控件属性、控件状态或页面初始化显示行为，而不一定需要 ESP32 固件持续重复发送整页画面。

### 当前处理决定

- 本次暂不修改固件源码。
- 先保留当前固件状态，等待明天烧录下载新的 HMI 工程到真实串口屏后验证。
- 若实机验证后画面能保持上一组数据且刷新无明显闪烁，优先把 HMI 控件状态修改作为正式修复路径记录下来。
- 若 HMI 编辑器调试正常但实机仍闪烁或回初始状态，再回到固件侧排查 `ref_stop`、`ref_star` 批量刷新策略，并考虑改为“只发送发生变化的控件值”。

### 明天烧录验证重点

1. 确认下载到串口屏的是已经修改控件状态后的最新 `.HMI` 编译产物。
2. 烧录/运行 ESP32 后停留在同一页面至少观察 2-3 个 5 秒刷新周期。
3. 验收标准：
   - 下一次数据到来前，温度、湿度、空气质量等控件持续显示上一组数据。
   - 新数据到达时只覆盖旧数据，不整页回初始态。
   - 刷新时没有明显黑屏、白屏、整页跳变或控件短暂消失。
4. 同时观察 monitor 中 `[TJC]` 命令，确认没有异常频繁的 `page <id>` 切页命令。
