# 2026-07-06 HTTP API 启动过早导致重启修复交付

## 问题现象

用户烧录 ESP32 固件并连接串口屏后，设备一上线就反复重启。串口工具在五六秒内收到接近三千行日志，屏幕无法稳定显示。

用户提供的日志文件：

```text
D:/QQ/串口输出信息.txt
```

日志中反复出现：

```text
assert failed: tcpip_send_msg_wait_sem /IDF/components/lwip/lwip/src/api/tcpip.c:454 (Invalid mbox)
Rebooting...
rst:0xc (RTC_SW_CPU_RST)
```

## 根因判断

这次重启不是串口屏主动刷屏或屏幕输出过多导致，直接触发点是 ESP-IDF HTTP server 在 lwIP/tcpip 邮箱尚未初始化完成时启动。

旧逻辑在 `CompactWifiBoard()` 构造函数内直接调用：

```cpp
smart_home_http_.Start();
```

`SmartHomeHttpServer::Start()` 内部会调用 `httpd_start()`。此时 `Application::Initialize()` 还没有完成网络事件回调注册，也还没有调用 `board.StartNetwork()`，WiFi/lwIP 网络栈尚未进入可用状态，因此触发：

```text
tcpip_send_msg_wait_sem Invalid mbox
```

串口工具中看到的大量 `[TJC]` 和启动日志，是设备不断重启后重复打印造成的，不是屏幕本身每秒发送了大量数据。

## 本阶段代码改动

- `main/boards/bread-compact-wifi/compact_wifi_board.cc`
  - 从 `CompactWifiBoard()` 构造函数中移除 `smart_home_http_.Start()`。
  - 新增 `SetNetworkEventCallback(NetworkEventCallback callback)` override。
  - 在回调中先保留原有 `Application` 网络状态处理，再在 `NetworkEvent::Connected` 时启动 HTTP API。
  - HTTP API 现在只会在 WiFi 连接成功、lwIP 网络栈就绪后启动。

当前启动时机：

```cpp
if (event == NetworkEvent::Connected) {
    ESP_LOGI(TAG, "Network connected, starting mini program HTTP API");
    smart_home_http_.Start();
}
```

`SmartHomeHttpServer::Start()` 本身已有幂等保护：

```cpp
if (server_ != nullptr) {
    return true;
}
```

因此 WiFi 重连或重复 Connected 事件不会重复创建第二个 HTTP server。

## 回归测试补充

- `tests/test_bread_compact_wifi_regressions.py`
  - 增加断言：`compact_wifi_board.cc` 中必须存在 `SetNetworkEventCallback(NetworkEventCallback callback)`。
  - 增加断言：HTTP API 启动必须绑定到 `NetworkEvent::Connected`。
  - 增加断言：`CompactWifiBoard()` 构造函数体内不得再包含 `smart_home_http_.Start()`。

## 同步更新的文档

- `../文档/智能家居外设接线与验证步骤.md`
  - 已说明 `Mini program HTTP API started on port 8080` 不会在刚开机立刻出现，必须等 WiFi 连接成功后才出现。
- `docs/phase-handoff-2026-07-05-mini-program-http-api.md`
  - 已补充 HTTP API 启动时机修正说明。
- `docs/phase-handoff-2026-07-05-end-of-day-summary.md`
  - 已补充第二天验证顺序中的启动日志预期。

## 验证结果

源码级回归测试通过：

```text
python -m unittest discover -s tests -v
Ran 10 tests in 0.005s
OK
```

HMI 控件契约 JSON 解析通过：

```text
python -m json.tool main/boards/bread-compact-wifi/serial_hmi_widgets.json
```

受保护配置文件检查无输出，表示本阶段未修改 `.vscode/**`、`sdkconfig*`、`CMakePresets.json`：

```text
git status --short -- .vscode sdkconfig sdkconfig.defaults sdkconfig.defaults.esp32 sdkconfig.defaults.esp32s3 CMakePresets.json
```

ESP-IDF 构建通过：

```text
powershell -ExecutionPolicy Bypass -File scripts/build_codex_check.ps1
Successfully created esp32s3 image.
Generated E:/espwork/xiaozhi-esp32/xiaozhi-esp32/build_codex_check/xiaozhi.bin
xiaozhi.bin binary size 0x243d20 bytes.
Smallest app partition is 0x3f0000 bytes.
0x1ac2e0 bytes (42%) free.
```

## 重新烧录后的预期现象

重新烧录本阶段固件后，刚开机不应再出现：

```text
assert failed: tcpip_send_msg_wait_sem ... (Invalid mbox)
Rebooting...
```

刚开机阶段仍应能看到串口屏初始化和传感器/控制器日志，例如：

```text
[TJC] page 0
Smart home controller initialized
```

WiFi 连接成功后，才应看到：

```text
Network connected, starting mini program HTTP API
Mini program HTTP API started on port 8080
```

如果还没有配置 WiFi，或者设备还停留在配网流程，则 HTTP API 日志不会立刻出现，这是正常现象。

## 用户和队友手动复测步骤

1. 使用 VSCode ESP-IDF 插件或命令行重新构建并烧录当前工程固件。
2. 打开 USB monitor。
3. 上电后先观察 20-30 秒，确认没有反复出现 `Invalid mbox` 和 `Rebooting...`。
4. 如果设备尚未配网，先完成 WiFi 配网。
5. WiFi 连接成功后确认 monitor 出现：

```text
Network connected, starting mini program HTTP API
Mini program HTTP API started on port 8080
```

6. 记录 ESP32 的局域网 IP。
7. 用浏览器或 curl 访问：

```text
http://<ESP32_IP>:8080/api/state
```

8. 若能返回 JSON，再继续验证小程序、MCP、串口屏 page2 控制和三类执行器。

## 遗留风险

- 本阶段修复的是 HTTP server 启动时机导致的 lwIP 断言重启；用户之前提到的“传感器每 5 秒刷新时屏幕闪一下”仍是独立问题，后续再排查。
- 如果重新烧录后仍然重启，需要保存新的完整串口日志；新的崩溃原因可能是供电、舵机启动电流、屏幕 TX 电平、其它任务栈或新的断言。
- HTTP API 仍只适合局域网演示，没有鉴权，不适合开放公网。

## 下一阶段目标

1. 用户重新烧录 ESP32 固件并提交新的 monitor 结果。
2. 若 `Invalid mbox` 不再出现，继续按 `智能家居外设接线与验证步骤.md` 验证串口屏、执行器、HTTP API 和小程序。
3. 若仍出现重启，优先从新日志中定位新的首个断言或 Guru Meditation，而不是继续假设是串口屏问题。
