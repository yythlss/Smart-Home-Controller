# 2026-07-05 智能家居控制阶段交付

## 当前工程状态

- 工程目录：`E:/espwork/xiaozhi-esp32/xiaozhi-esp32`
- 当前板型目录：`main/boards/bread-compact-wifi`
- 当前 HMI 文件：`D:/QQ/serial_warm_home  (1).HMI`
- 本阶段已加入智能家居执行器控制、屏幕事件转发、MCP 语音控制工具、空气质量曲线写入。

## HMI 数字 ID 复扫结论

- 已从 HMI 文件二进制中再次确认：`c_air` 存在，数字 ID 为 `12`。
- 固件当前使用 `add 12,0,val` 写入空气质量曲线。
- `c_temp`、`c_humi` 当前 HMI 文件中未找到，因此温度/湿度曲线仍在固件中保持禁用门控，避免向不存在控件发送 `add` 命令。
- `hs_eco` 控件存在，但当前 HMI 文件里它仍发送 `BTN,MODE,AUTO,TOGGLE`，没有找到 `BTN,MODE,ECO,TOGGLE`。需要在 HMI 编辑器中把 `hs_eco` 的释放事件改成：

```text
prints "BTN,MODE,ECO,TOGGLE",0
printh 0a
```

## 本阶段改动文件

- `main/boards/bread-compact-wifi/config.h`
  - 新增智能家居 GPIO 和 LEDC PWM 配置。
- `main/boards/bread-compact-wifi/smart_home_controller.h`
  - 新增智能家居控制器接口和状态结构。
- `main/boards/bread-compact-wifi/smart_home_controller.cc`
  - 实现净化、加湿、新风舵机、自动模式、节能模式、MCP 工具、环境历史记录和空气曲线写入。
- `main/boards/bread-compact-wifi/compact_wifi_board.cc`
  - 初始化 `SmartHomeController`，转发屏幕按键事件和传感器数据。
- `main/boards/bread-compact-wifi/serial_hmi_widgets.json`
  - 记录 `c_air` 数字 ID 为 `12`，补充 `hs_eco` 的目标事件。
- `../文档/串口屏设计说明.md`
  - 更新串口屏控件状态说明。
- `../文档/串口屏手动事件配置手册.md`
  - 更新节能模式事件和曲线 ID 说明。
- `tests/test_bread_compact_wifi_regressions.py`
  - 新增智能家居控制相关源码级回归测试。
- `scripts/build_codex_check.ps1`
  - 新增工程内构建脚本，只在当前 PowerShell 进程内补齐 ESP-IDF/CMake/Ninja/ccache/Git 工具路径。
  - 设置工程内 `.ccache` 目录，规避用户全局 ccache 临时目录权限问题。
  - 设置 `ESP_ROM_ELF_DIR` 和 `ESP_IDF_VERSION`，避免未加载完整 ESP-IDF 环境时的部分配置警告。

## GPIO 接线约定

- `GPIO13`：净化红色 LED PWM 输出，红 LED 需串联限流电阻后接地。
- `GPIO14`：加湿蓝色 LED PWM 输出，蓝 LED 需串联限流电阻后接地。
- `GPIO21`：360°连续旋转舵机风扇信号线。
- 连续旋转舵机风扇供电：红线接稳定 5V，棕/黑线接 GND，必须与 ESP32S3 共地。
- `GPIO47`、`GPIO48`、`GPIO45`：本阶段保留未使用。

## 功能行为

- 净化：屏幕或 MCP 控制 `0-3` 档，红色 LED 亮度表示档位。
- 加湿：屏幕或 MCP 控制 `0-3` 档，蓝色 LED 亮度表示档位。
- 新风：屏幕或 MCP 控制 `0-3` 档，`GPIO21` 输出 50Hz PWM 控制 360°连续旋转舵机风扇。
  - 0 档：`1500us`，中位停止。
  - 1 档：`1600us`，低速连续旋转。
  - 2 档：`1750us`，中速连续旋转。
  - 3 档：`1900us`，高速连续旋转。
- 自动模式：根据湿度、温度和 MQ135 raw 自动调整净化、加湿和新风。
- 节能模式：软件节能，不进入 deep sleep；开启后自动关闭自动模式和所有执行器。
- MCP 工具：
  - `self.home.get_state`
  - `self.home.set_purifier`
  - `self.home.set_fresh_air`
  - `self.home.set_humidifier`
  - `self.home.set_auto`
  - `self.home.set_eco`

## 验证结果

- 源码级回归测试已通过：

```text
python -m unittest discover -s tests -v
Ran 9 tests in 0.004s
OK
```

- 固件构建已通过：

```text
ninja -C build_codex_check -j 1
Successfully created esp32s3 image.
Generated E:/espwork/xiaozhi-esp32/xiaozhi-esp32/build_codex_check/xiaozhi.bin
xiaozhi.bin binary size 0x242ef0 bytes. Smallest app partition is 0x3f0000 bytes. 0x1ad110 bytes (43%) free.
```

- 构建前执行过 `rebuild_cache`，原因是当前工程使用 `file(GLOB ...)` 收集板级源码，新增 `smart_home_controller.cc` 后原 build 目录不会自动纳入新源文件。
- 编译器本体可用，`xtensa-esp32s3-elf-g++` 已成功链接并生成 `xiaozhi.bin`。
- 新增脚本验证通过：

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build_codex_check.ps1
powershell -ExecutionPolicy Bypass -File scripts/build_codex_check.ps1 -Reconfigure
```

- `scripts/build_codex_check.ps1 -Reconfigure` 可完成重新配置和构建，并生成 `xiaozhi.bin`。
- 剩余非阻断警告：
  - Bootloader 子工程重新配置时仍可能出现 `git-submodule: basename/sed command not found`。
  - 该问题属于 Git for Windows/MSYS 子模块脚本调用环境，不是 ESP32 编译器故障；当前不修改全局 Git/系统 PATH，避免破坏开发环境。
  - 该警告未影响 `xiaozhi.bin` 或 `generated_assets.bin` 生成。

## 烧录验证建议

1. 先在 HMI 编辑器中修正 `hs_eco` 事件为 `BTN,MODE,ECO,TOGGLE`。
2. 烧录固件：

```powershell
idf.py -B build_codex_check -p COMx flash monitor
```

如果当前终端没有完整加载 ESP-IDF 环境，可先用工程内脚本构建：

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build_codex_check.ps1 -Reconfigure
```

3. 实机重点验证：
   - 净化按键循环切换红色 LED 亮度。
   - 加湿按键循环切换蓝色 LED 亮度。
   - 新风按键循环切换连续旋转舵机风扇档位。
   - 自动模式会根据传感器数据退出节能并控制执行器。
   - 节能模式会关闭自动和所有执行器。
   - 空气质量曲线能随 5s 传感器刷新追加数据点。

## 遗留风险

- `hs_eco` 在当前 HMI 文件里仍是 AUTO 事件，需要人工修改 HMI 文件后再烧录屏幕资源验证。
- 温度/湿度曲线控件还没有数字 ID，固件暂不发送温湿度曲线数据。
- 连续旋转舵机风扇负载启动电流较大，若直接从开发板 5V 取电导致复位或屏幕闪烁，需要改用独立 5V 供电并共地。
- LED 需要串联限流电阻，避免 GPIO 过流。

## 下一阶段

- 烧录固件并实机验证屏幕按键、语音 MCP 控制、LED 亮度和连续旋转舵机风扇档位。
- 在 HMI 中新增或确认 `c_temp`、`c_humi` 曲线控件数字 ID 后，再开启温湿度曲线写入。
- 若屏幕仍闪烁，继续从串口屏刷新命令频率、页面初始化脚本和供电稳定性三条线排查。
