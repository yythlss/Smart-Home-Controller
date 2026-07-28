# 2026-07-05 当日进度记录

## 当前工程状态

- 工程目录：`E:/espwork/xiaozhi-esp32/xiaozhi-esp32`
- 当前板型目录：`main/boards/bread-compact-wifi`
- 今天主要完成两条线：
  - 智能家居执行器控制：净化、新风、加湿、自动、节能、语音 MCP 工具、串口屏事件接入。
  - 小程序方案 A：ESP32S3 局域网 HTTP API 和微信小程序最小演示工程。
- 没有修改 `.vscode/**`、`sdkconfig`、`sdkconfig.defaults*`、ESP-IDF 安装目录或全局环境变量。

## 今天完成的功能

### 智能家居控制

- 净化：`GPIO13` 红色 LED，使用 PWM 亮度表示 `0-3` 档。
- 加湿：`GPIO14` 蓝色 LED，使用 PWM 亮度表示 `0-3` 档。
- 新风：`GPIO21` 360°连续旋转舵机风扇，按档位写入 `1500us/1600us/1750us/1900us` 控制停止、低速、中速、高速。
- 自动模式：根据湿度、温度、MQ135 原始值自动调整加湿、净化、新风。
- 节能模式：软件节能，不进入 deep sleep；开启后关闭自动模式和全部执行器。
- 语音控制：已注册 MCP 工具：
  - `self.home.get_state`
  - `self.home.set_purifier`
  - `self.home.set_fresh_air`
  - `self.home.set_humidifier`
  - `self.home.set_auto`
  - `self.home.set_eco`

### 串口屏与曲线

- 固件继续使用 `SerialHmi` 作为 TJC 串口屏唯一 UART 所有者。
- 已确认空气质量曲线控件 `c_air` 的数字 ID 为 `12`，固件可通过 `add 12,0,val` 写入空气评分点。
- 温度曲线 `c_temp`、湿度曲线 `c_humi` 的数字 ID 还没有确认，固件暂时保持禁用，避免向不存在控件发送 `add`。
- `hs_eco` 仍需要在 HMI 编辑器中人工确认释放事件是否为：

```text
prints "BTN,MODE,ECO,TOGGLE",0
printh 0a
```

### 小程序方案 A

- 新增 ESP32S3 局域网 HTTP API，端口固定为 `8080`，避免和配网页面默认 `80` 端口冲突。
- 2026-07-06 修正：HTTP API 已改为 WiFi 连接成功后再启动，不再在板级构造函数中提前启动，避免 `tcpip_send_msg_wait_sem Invalid mbox` 断言重启。
- 已实现接口：
  - `GET /api/state`：读取当前设备、模式、温湿度、MQ135、空气评分。
  - `GET /api/history`：读取最近最多 30 条环境采样数据。
  - `POST /api/device`：控制净化、新风、加湿。
  - `POST /api/mode`：控制自动、节能模式。
- 新增微信小程序最小演示工程：`docs/mini_program_demo`
  - 支持填写 `<ESP32_IP>:8080`
  - 支持显示温湿度、空气评分、近期空气质量条形历史
  - 支持远程控制净化、新风、加湿、自动、节能

## 今天新增或重点修改的文件

- `main/boards/bread-compact-wifi/config.h`
- `main/boards/bread-compact-wifi/compact_wifi_board.cc`
- `main/boards/bread-compact-wifi/smart_home_controller.h`
- `main/boards/bread-compact-wifi/smart_home_controller.cc`
- `main/boards/bread-compact-wifi/smart_home_http_server.h`
- `main/boards/bread-compact-wifi/smart_home_http_server.cc`
- `main/CMakeLists.txt`
- `tests/test_bread_compact_wifi_regressions.py`
- `docs/mini_program_demo/**`
- `docs/phase-handoff-2026-07-05-smart-home-control.md`
- `docs/phase-handoff-2026-07-05-mini-program-http-api.md`
- `docs/phase-handoff-2026-07-05-end-of-day-summary.md`

## 验证结果

源码级回归测试通过：

```text
python -m unittest discover -s tests -v
Ran 10 tests in 0.005s
OK
```

固件构建通过：

```text
powershell -ExecutionPolicy Bypass -File scripts/build_codex_check.ps1
Successfully created esp32s3 image.
Generated E:/espwork/xiaozhi-esp32/xiaozhi-esp32/build_codex_check/xiaozhi.bin
xiaozhi.bin binary size 0x243c00 bytes. Smallest app partition is 0x3f0000 bytes. 0x1ac400 bytes (42%) free.
```

小程序 JSON 文件解析通过。

## 明天建议验证顺序

1. 先烧录当前固件。
2. 串口监视器先确认刚开机阶段没有反复出现 `Invalid mbox` 和 `Rebooting...`。
3. WiFi 连接成功后确认启动日志里依次出现：

```text
Network connected, starting mini program HTTP API
Mini program HTTP API started on port 8080
```

4. 确认 ESP32S3 已连上 WiFi，并记录局域网 IP。
5. 用浏览器访问：

```text
http://<ESP32_IP>:8080/api/state
```

6. 如果能返回 JSON，再导入 `docs/mini_program_demo` 到微信开发者工具。
7. 微信开发者工具里关闭合法域名校验，填写 `<ESP32_IP>:8080` 测试小程序控制。
8. 实机检查净化红 LED、加湿蓝 LED、新风舵机、自动模式、节能模式是否和屏幕/语音/小程序状态一致。

## 遗留问题与风险

- 小程序 HTTP API 当前没有鉴权，只适合局域网演示和答辩展示。
- 小程序真机预览可能受微信 HTTP/域名策略影响；优先先用微信开发者工具模拟器验证。
- `hs_eco` 的 HMI 事件仍建议明天在 HMI 编辑器里复查。
- 温度、湿度曲线控件数字 ID 未确认，暂时只启用空气质量曲线。
- 连续旋转舵机风扇建议使用稳定 5V 供电并与 ESP32S3 共地，避免启动电流导致复位或串口屏闪烁。
- LED 必须串联限流电阻。

## 明天继续时优先读这些文件

1. `docs/phase-handoff-2026-07-05-end-of-day-summary.md`
2. `docs/phase-handoff-2026-07-05-mini-program-http-api.md`
3. `docs/phase-handoff-2026-07-05-smart-home-control.md`
4. `main/boards/bread-compact-wifi/smart_home_controller.cc`
5. `main/boards/bread-compact-wifi/smart_home_http_server.cc`
6. `docs/mini_program_demo/README.md`
