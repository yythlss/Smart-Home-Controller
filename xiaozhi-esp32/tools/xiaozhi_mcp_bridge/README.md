# Xiaozhi External Services MCP Bridge

这个目录提供小智 AI 的电脑端 MCP 服务。默认只注册天气、新闻和室内外组合建议；净化、新风、加湿、灯光、模式和场景控制由 ESP32S3 设备端 MCP 直接负责，避免同一个语音意图出现两套控制工具。

旧部署仍可设置 `XIAOZHI_MCP_BRIDGE_MODE=full`，临时恢复通过 ESP32 HTTP API 转发控制的兼容工具。

数据流：

```text
小智 AI 语音
  -> 小智 MCP 接入点 wss://api.xiaozhi.me/mcp/?token=...
  -> 本机 mcp_pipe.py
  -> tools/xiaozhi_mcp_bridge/smart_home_bridge.py
  -> Open-Meteo / 可信 RSS
  -> 组合建议需要时读取 ESP32S3 http://<ESP32_IP>:8080/api/state
```

注意：不要把真实 `token` 写进源码、文档、截图或提交记录。请用环境变量传入。

## 1. 前置条件

先确认 ESP32S3 固件已经工作：

```text
[ ] ESP32S3 已连接 WiFi
[ ] 串口 monitor 出现 Network connected, starting mini program HTTP API
[ ] 串口 monitor 出现 Mini program HTTP API started on port 8080
[ ] 浏览器能打开 http://<ESP32_IP>:8080/api/state
```

如果 HTTP API 还不能访问，先不要调 MCP 桥接。

## 2. 准备 Python 环境

在工程根目录执行：

```powershell
cd E:\espwork\Smart-Home-Controller\xiaozhi-esp32
python -m venv .venv_mcp_bridge
.\.venv_mcp_bridge\Scripts\Activate.ps1
python -m pip install --upgrade pip
python -m pip install -r tools\xiaozhi_mcp_bridge\requirements.txt
```

还需要准备示例仓库里的 `mcp_pipe.py`：

```powershell
git clone https://github.com/78/mcp-calculator .cache\mcp-calculator
```

如果现场网络不能拉 GitHub，可以让队友提前把示例仓库下载好，重点需要其中的 `mcp_pipe.py`。

## 3. 设置环境变量

PowerShell 示例：

```powershell
$env:MCP_ENDPOINT = "wss://api.xiaozhi.me/mcp/?token=你的新token"
$env:XIAOZHI_MCP_BRIDGE_MODE = "external"
$env:ESP32_BASE_URL = "http://192.168.1.23:8080"
$env:ESP32_API_TOKEN = ""  # 固件未启用鉴权时留空
$env:NEWS_RSS_URL = "https://你的可信新闻源/rss.xml"
```

说明：

- `MCP_ENDPOINT`：小智平台给出的 MCP 接入点。
- `XIAOZHI_MCP_BRIDGE_MODE`：默认 `external`；只有旧部署兼容时才设为 `full`。
- `ESP32_BASE_URL`：组合建议或 `full` 兼容模式使用的 ESP32S3 局域网 HTTP API 地址。
- `ESP32_API_TOKEN`：可选；必须与固件 `SMART_HOME_API_TOKEN` 一致，桥接会通过 `X-API-Key` 转发。
- `NEWS_RSS_URL`：可选，AI 读取新闻时使用的可信 RSS 地址；不配置时新闻工具会返回明确错误。
- 天气工具使用 Open-Meteo，不需要 API 密钥；桥接电脑必须能访问互联网。
- 不要把真实 token 写入 `mcp_config.json`。

## 4. 启动桥接服务

推荐使用工程脚本启动：

```powershell
$env:MCP_ENDPOINT = "wss://api.xiaozhi.me/mcp/?token=你的新token"
$env:ESP32_BASE_URL = "http://192.168.1.23:8080"
powershell -ExecutionPolicy Bypass -File scripts\start_xiaozhi_mcp_bridge.ps1
```

如果第一次运行需要安装依赖：

```powershell
powershell -ExecutionPolicy Bypass -File scripts\start_xiaozhi_mcp_bridge.ps1 -InstallDeps
```

使用示例仓库的 `mcp_pipe.py` 启动：

```powershell
python .cache\mcp-calculator\mcp_pipe.py tools\xiaozhi_mcp_bridge\smart_home_bridge.py
```

如果使用示例仓库默认读取配置的方式，可以参考本目录的 `mcp_config.json`，但真实 token 仍然只放在环境变量里。

## 5. 默认注册工具

`external` 模式只暴露以下工具给小智 AI：

```text
home_get_weather
home_get_news
home_get_combined_advice
```

ESP32 设备端注册以下权威控制工具：

```text
self.home.get_state
self.home.get_summary
self.home.get_health
self.home.set_purifier
self.home.set_fresh_air
self.home.set_humidifier
self.home.set_light
self.home.set_auto
self.home.set_eco
self.home.apply_scene
self.home.acknowledge_alarm
self.home.get_advice
```

需要回退到旧 HTTP 转发方式时：

```powershell
$env:XIAOZHI_MCP_BRIDGE_MODE = "full"
```

`full` 模式会额外注册原有 `home_get_state`、`home_set_*`、`home_update_context` 和手动环境工具。它只用于兼容旧端点，不应与设备端 MCP 长期同时暴露。

常见语音意图示例：

```text
查询杭州天气
读三条新闻
结合杭州天气给我室内环境建议
```

设备控制语句由设备端 MCP 处理，例如“打开净化器二档”“进入睡眠场景”“检查传感器是否正常”。

## 6. 错误与日志

工具调用失败时返回结构化错误，不会因为一次网络异常退出 MCP 进程：

```json
{
  "ok": false,
  "tool": "home_get_weather",
  "error": {
    "code": "external_unreachable",
    "message": "Cannot reach external service"
  }
}
```

日志写入标准错误流，不占用 MCP 的标准输入输出通道。可通过 `XIAOZHI_MCP_LOG_LEVEL=DEBUG|INFO|WARNING` 调整级别。

## 7. 排查顺序

如果语音控制失败，按这个顺序排查：

1. 浏览器访问 `http://<ESP32_IP>:8080/api/state` 是否成功。
2. PowerShell 调用 `/api/device` 是否能控制设备。
3. `ESP32_BASE_URL` 是否是 `http://<ESP32_IP>:8080`，不是配网页面的 80 端口。
4. `MCP_ENDPOINT` token 是否有效，建议重新生成后再用。
5. `mcp_pipe.py` 是否已连接到小智平台。
6. 小智 AI 在电脑端是否只看到 3 个外部服务工具，并同时看到设备端 `self.home.*` 控制工具。

## 8. 本地测试

ESP32 HTTP API 预检查：

```powershell
powershell -ExecutionPolicy Bypass -File scripts\test_esp32_http_api.ps1 -Esp32BaseUrl http://192.168.1.23:8080
```

如果要顺便测试外设控制和手动环境接口：

```powershell
powershell -ExecutionPolicy Bypass -File scripts\test_esp32_http_api.ps1 -Esp32BaseUrl http://192.168.1.23:8080 -ControlTest -EnvironmentTest
```

桥接层单元测试：

```powershell
python -m unittest discover -s tests -p test_xiaozhi_mcp_bridge.py -v
```

完整源码级回归测试：

```powershell
python -m unittest discover -s tests -v
```
