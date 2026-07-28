# Xiaozhi Smart Home MCP Bridge

这个目录提供一个外部 MCP 桥接服务，用于把小智 AI 的 MCP 工具调用转发到 ESP32S3 的局域网 HTTP API。

数据流：

```text
小智 AI 语音
  -> 小智 MCP 接入点 wss://api.xiaozhi.me/mcp/?token=...
  -> 本机 mcp_pipe.py
  -> tools/xiaozhi_mcp_bridge/smart_home_bridge.py
  -> ESP32S3 http://<ESP32_IP>:8080/api/*
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
cd E:\espwork\xiaozhi-esp32\xiaozhi-esp32
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
$env:ESP32_BASE_URL = "http://192.168.1.23:8080"
$env:NEWS_RSS_URL = "https://你的可信新闻源/rss.xml"
```

说明：

- `MCP_ENDPOINT`：小智平台给出的 MCP 接入点。
- `ESP32_BASE_URL`：ESP32S3 局域网 HTTP API 地址。
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

## 5. 已注册工具

桥接服务暴露这些工具给小智 AI：

```text
home_get_state
home_set_purifier
home_set_fresh_air
home_set_humidifier
home_set_light
home_set_auto
home_set_eco
home_update_context
home_acknowledge_alarm
home_set_environment_preset
home_set_manual_environment
home_disable_manual_environment
home_get_advice
home_get_weather
home_get_news
home_get_combined_advice
```

常见语音意图示例：

```text
打开净化器二档
关闭加湿器
开启新风三档
打开自动模式
进入节能模式
打开灯
家里有人，当前亮度百分之十八
模拟污染环境
模拟干燥环境
现在环境怎么样
查询杭州天气
读三条新闻
结合杭州天气给我室内环境建议
确认环境报警
恢复真实传感器数据
```

## 6. 排查顺序

如果语音控制失败，按这个顺序排查：

1. 浏览器访问 `http://<ESP32_IP>:8080/api/state` 是否成功。
2. PowerShell 调用 `/api/device` 是否能控制设备。
3. `ESP32_BASE_URL` 是否是 `http://<ESP32_IP>:8080`，不是配网页面的 80 端口。
4. `MCP_ENDPOINT` token 是否有效，建议重新生成后再用。
5. `mcp_pipe.py` 是否已连接到小智平台。
6. 小智 AI 是否能看到 `home_*` 工具列表。

## 7. 本地测试

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
