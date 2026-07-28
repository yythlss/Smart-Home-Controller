# 2026-07-06 阶段交付：360°连续旋转舵机风扇改造

> 历史说明：本文件记录 2026-07-06 当时的连续旋转方案。当前源码已在 2026-07-07 恢复为 180°角度舵机扇叶往复摆动，接续开发不要按本文的固定脉宽表修改代码。

## 本阶段目标

用户确认新风/风扇硬件已经从原先“180°角度舵机加扇叶”更换为“可持续 360°旋转的舵机风扇”。本阶段只处理该硬件变化对应的固件和文档调整，暂不继续排查串口屏 5 秒刷新闪屏问题。

## 开发前只读确认

- 已按接续规则检查 `E:/espwork/AGENTS.md` 中的开发前阅读和阶段交付要求。
- 已检查当前工程目录：`E:/espwork/xiaozhi-esp32/xiaozhi-esp32`。
- 已检查当前板型目录：`main/boards/bread-compact-wifi`。
- 已检查当前工作树状态，确认工作区已有多项历史改动和未跟踪文件，本阶段未回退用户或队友已有内容。
- 已检查关键源码：`smart_home_controller.h/.cc`、`config.h`、`compact_wifi_board.cc`。
- 已检查关键文档：`docs/current-project-handoff.md`、`docs/continuation-notes.md`、`docs/phase-handoff-2026-07-05-smart-home-control.md`、`docs/superpowers/specs/2026-07-05-smart-home-control-design.md`。

## 当前工程状态

- `GPIO13`：净化器红色 LED，LEDC PWM 亮度表示档位。
- `GPIO14`：加湿器蓝色 LED，LEDC PWM 亮度表示档位。
- `GPIO21`：新风/风扇执行器，当前改为 360°连续旋转舵机风扇。
- 屏幕事件名保持不变：`BTN,DEVICE,FAN,TOGGLE`。
- 固件状态字段保持不变：`fresh_air_level`。
- MCP 工具保持不变：`self.home.set_fresh_air`。
- HTTP API 保持不变：`POST /api/device` 中继续使用 `fresh_air` 或 `fan`。

## 本阶段代码改动

### `main/boards/bread-compact-wifi/smart_home_controller.h`

- 删除旧的 FreeRTOS 舵机扫描任务声明。
- 删除旧的角度舵机状态字段：
  - `servo_target_min_angle_`
  - `servo_target_max_angle_`
  - `servo_step_degrees_`
  - `servo_step_delay_ms_`
  - `servo_task_running_`
  - `servo_task_handle_`
- 删除旧方法：
  - `SetServoAngle()`
  - `SetServoProfileForLevel()`
  - `ServoTaskEntry()`
  - `ServoTaskLoop()`
- 新增连续旋转舵机风扇方法：
  - `SetContinuousServoPulseUs(int pulse_us)`
  - `FreshAirPulseUsForLevel(int level) const`

### `main/boards/bread-compact-wifi/smart_home_controller.cc`

- `Initialize()` 不再创建 `fresh_air_servo` 后台任务。
- `ApplyFreshAir()` 改为在档位变化时直接写入固定 PWM 脉宽。
- 当前档位表：

| 档位 | 语义 | PWM 脉宽 |
| --- | --- | --- |
| `0` | 关闭/停止 | `1500us` |
| `1` | 低速连续旋转 | `1600us` |
| `2` | 中速连续旋转 | `1750us` |
| `3` | 高速连续旋转 | `1900us` |

- `SetContinuousServoPulseUs()` 仍使用原 `SMART_HOME_SERVO_CHANNEL` 和 `SMART_HOME_SERVO_PWM_HZ=50`，只把输入脉宽换算为 13-bit LEDC duty。
- `self.home.set_fresh_air` 的 MCP 描述已改为 continuous-rotation servo fan，接口参数不变。

## 本阶段文档改动

- `docs/superpowers/specs/2026-07-05-smart-home-control-design.md`
  - 硬件说明改为 `360°连续旋转舵机风扇`。
  - 档位说明改为 `1500us/1600us/1750us/1900us`。
  - 删除旧 180°角度舵机扫描描述。
- `docs/phase-handoff-2026-07-05-smart-home-control.md`
  - 接线和验证说明更新为连续旋转舵机风扇。
- `docs/phase-handoff-2026-07-05-end-of-day-summary.md`
  - 当前新风执行器状态更新为连续旋转舵机风扇。
- `../文档/串口屏手动事件配置手册.md`
  - 手动验证说明更新为 `GPIO21` 连续旋转舵机风扇。
- `docs/current-project-handoff.md`
  - 当前状态更新到 2026-07-06。
  - 补充智能家居控制器、HTTP API、连续旋转舵机风扇状态。
- `docs/continuation-notes.md`
  - 接续阅读顺序补充本交付文档。
  - 增加连续旋转舵机风扇开发边界和硬件注意事项。
- `tests/test_bread_compact_wifi_regressions.py`
  - 更新回归断言，要求保留连续旋转舵机风扇脉宽表并禁止恢复旧扫描任务。

## 验证结果

源码级回归测试通过：

```text
python -m unittest discover -s tests -v
Ran 10 tests in 0.004s
OK
```

固件构建通过：

```text
powershell -ExecutionPolicy Bypass -File scripts/build_codex_check.ps1
Successfully created esp32s3 image.
Generated E:/espwork/xiaozhi-esp32/xiaozhi-esp32/build_codex_check/xiaozhi.bin
xiaozhi.bin binary size 0x243a40 bytes.
Smallest app partition is 0x3f0000 bytes.
0x1ac5c0 bytes (42%) free.
```

## 需要人工实机确认

1. 确认连续旋转舵机风扇三线连接：
   - 信号线接 `GPIO21`。
   - 红线接稳定 `5V`。
   - 棕/黑线接 `GND`。
   - 舵机风扇电源 GND 必须与 ESP32S3 GND 共地。
2. 烧录 ESP32 固件后测试 `BTN,DEVICE,FAN,TOGGLE` 或 MCP/小程序新风控制。
3. 确认档位循环为：关 -> 低速 -> 中速 -> 高速 -> 关。
4. 如果 0 档仍缓慢转动，说明该舵机的中位停止脉宽不是精确 `1500us`，需要微调 `FreshAirPulseUsForLevel()` 的 0 档返回值。
5. 如果低档不能启动，适当提高 `1600us`；如果高档过猛，适当降低 `1900us`。
6. 如果旋转方向与预期相反，可以把档位脉宽改到低于中位，例如 `1400us/1250us/1100us`，但需要先确认扇叶安装方向和风向需求。

## 遗留风险

- 连续旋转舵机风扇的中位停止脉宽存在个体差异，`1500us` 是常见初始值，不保证所有实物完全停止。
- 舵机风扇启动电流可能导致 ESP32S3 或串口屏复位、闪烁，建议使用稳定 5V 供电并共地。
- 本阶段没有处理串口屏 5 秒刷新时的闪屏问题，该问题仍作为后续独立排查项。
- 本阶段没有改变 HMI 事件名，所以如果屏幕侧 `hs_fan` 事件尚未写入或写错，仍需按 `串口屏手动事件配置手册.md` 手动修正。

## 下一阶段建议

1. 先烧录本次固件，实测连续旋转舵机风扇的停止点和三个档位。
2. 根据实测结果微调 `FreshAirPulseUsForLevel()` 的脉宽表。
3. 确认 HMI `hs_fan` 的弹起事件仍为：

```text
prints "BTN,DEVICE,FAN,TOGGLE",0
printh 0a
```

4. 风扇档位验证稳定后，再回到串口屏 5 秒刷新闪屏问题，优先检查 HMI 页面初始化脚本、固件刷新节流和供电稳定性。
