# 2026-07-05 小程序方案 A 阶段交付

## 当前工程状态

- 工程目录：`E:/espwork/xiaozhi-esp32/xiaozhi-esp32`
- 板型目录：`main/boards/bread-compact-wifi`
- 本阶段实现“方案 A”：ESP32S3 在局域网内启动 HTTP API，小程序通过同一 WiFi 访问设备状态和控制接口。
- HTTP API 端口固定为 `8080`，避免和配网组件默认 `80` 端口冲突。
- 未修改 `.vscode/**`、`sdkconfig`、`sdkconfig.defaults*`、ESP-IDF 安装目录或全局环境变量。

## 本阶段改动文件

- `main/boards/bread-compact-wifi/smart_home_http_server.h`
  - 新增智能家居局域网 HTTP 服务封装。
- `main/boards/bread-compact-wifi/smart_home_http_server.cc`
  - 新增 `/api/state`、`/api/history`、`/api/device`、`/api/mode` 和 `OPTIONS /api/*`。
  - 新增 CORS 响应头，便于小程序开发者工具和局域网页面调试。
  - 固定监听 `8080` 端口。
- `main/boards/bread-compact-wifi/smart_home_controller.h`
  - 暴露 `GetLastSample()`、`BuildStateJson()`、`BuildHistoryJson()` 给 HTTP API 使用。
- `main/boards/bread-compact-wifi/smart_home_controller.cc`
  - 新增历史环境数据 JSON 输出，供小程序展示近期空气质量/温湿度数据。
- `main/boards/bread-compact-wifi/compact_wifi_board.cc`
  - 板级对象中接入 `SmartHomeHttpServer`。
  - 2026-07-06 修正：HTTP API 不再在 `CompactWifiBoard()` 构造函数内启动，改为 WiFi 连接成功并收到 `NetworkEvent::Connected` 后启动，避免 lwIP/tcpip 尚未就绪时触发 `Invalid mbox` 断言重启。
- `main/CMakeLists.txt`
  - 增加 `esp_http_server` 局部组件依赖。
- `tests/test_bread_compact_wifi_regressions.py`
  - 增加小程序 HTTP API 源码级回归测试。
- `docs/mini_program_demo/**`
  - 新增微信小程序最小演示工程。

## HTTP API

### 查询当前状态

```text
GET http://<ESP32_IP>:8080/api/state
```

返回字段包括：

- `purifier_level`：净化档位，`0-3`
- `fresh_air_level`：新风档位，`0-3`
- `humidifier_level`：加湿档位，`0-3`
- `auto_mode`：自动模式
- `eco_mode`：节能模式
- `temperature_c`、`humidity_percent`、`mq135_raw`、`air_score`

### 查询近期历史

```text
GET http://<ESP32_IP>:8080/api/history
```

返回最近最多 `30` 条环境采样数据，顺序为旧到新。

### 控制设备

```text
POST http://<ESP32_IP>:8080/api/device
Content-Type: application/json

{"device":"purifier","power":true,"level":2}
```

支持设备名：

- `purifier` / `air_purifier`
- `fresh_air` / `fan`
- `humidifier`

### 控制模式

```text
POST http://<ESP32_IP>:8080/api/mode
Content-Type: application/json

{"mode":"auto","power":true}
```

支持模式：

- `auto`
- `eco`

## 小程序演示工程

目录：`docs/mini_program_demo`

使用步骤：

1. 微信开发者工具导入 `docs/mini_program_demo`。
2. 使用测试号或游客模式。
3. 在开发者工具里关闭合法域名校验。
4. ESP32S3 和运行小程序的手机/电脑连接同一个路由器。
5. 从串口日志或路由器后台获取 ESP32S3 IP。
6. 小程序首页填写 `<ESP32_IP>:8080`，例如 `192.168.1.23:8080`。

## 验证结果

源码级回归测试通过：

```text
python -m unittest discover -s tests -v
Ran 10 tests in 0.006s
OK
```

固件构建通过：

```text
powershell -ExecutionPolicy Bypass -File scripts/build_codex_check.ps1
Successfully created esp32s3 image.
Generated E:/espwork/xiaozhi-esp32/xiaozhi-esp32/build_codex_check/xiaozhi.bin
xiaozhi.bin binary size 0x243c00 bytes. Smallest app partition is 0x3f0000 bytes. 0x1ac400 bytes (42%) free.
```

## 实机验证建议

1. 烧录当前 `build_codex_check/xiaozhi.bin`。
2. 串口监视器先确认刚开机阶段没有反复出现 `Invalid mbox` 和 `Rebooting...`。
3. WiFi 连接成功后，串口监视器确认依次出现：

```text
Network connected, starting mini program HTTP API
Mini program HTTP API started on port 8080
```

4. 获取 ESP32S3 的局域网 IP。
5. 浏览器访问：

```text
http://<ESP32_IP>:8080/api/state
```

6. 用 Postman、curl 或小程序演示工程测试控制接口。

## 遗留风险

- HTTP API 当前没有鉴权，只适合局域网演示和答辩展示。
- 小程序真机请求 HTTP 可能受微信域名/HTTPS 策略影响；开发阶段优先使用微信开发者工具并关闭合法域名校验。
- 若设备处于配网 AP 流程，配网页面仍使用默认 `80` 端口；小程序 API 使用 `8080`，两者端口已避开，但实际并发还需要实机确认。
- 历史曲线目前由传感器 5s 采样驱动，最多保留 30 条，约 2.5 分钟窗口。

## 下一阶段目标

- 烧录后验证手机/电脑能访问 `http://<ESP32_IP>:8080/api/state`。
- 用小程序演示工程完成净化、新风、加湿、自动、节能的远程控制演示。
- 若答辩需要更强展示效果，可把小程序历史数据区域升级为 canvas 折线图，分别显示温度、湿度和空气评分。
