# 2026-07-07 小智远端 MCP 桥接服务阶段交付

## 本阶段目标

用户提供 `https://github.com/78/mcp-calculator` 作为小智 MCP 接入示例，并提供小智平台的 `wss://api.xiaozhi.me/mcp/?token=...` 接入点。

本阶段目标是新增一个外部 Python MCP 服务，使小智 AI 可以通过远端 MCP 接入点调用工具，再由本地电脑把工具调用转发到 ESP32S3 局域网 HTTP API。

本阶段没有把真实 token 写入工程文件。

## 当前判断

- 当前 ESP32 固件内已经注册 `self.home.*` 设备端 MCP 工具。
- 用户提供的 `wss://api.xiaozhi.me/mcp/?token=...` 属于小智平台远端 MCP 接入点，需要外部 MCP 服务通过 `mcp_pipe.py` 连接。
- ESP32 的局域网 HTTP API 已存在，端口为 `8080`。
- 因为小智云端不能直接访问 `192.168.x.x`，所以桥接服务应运行在和 ESP32 同一局域网的电脑上。

## 新增文件

```text
tools/xiaozhi_mcp_bridge/__init__.py
tools/xiaozhi_mcp_bridge/smart_home_bridge.py
tools/xiaozhi_mcp_bridge/requirements.txt
tools/xiaozhi_mcp_bridge/mcp_config.json
tools/xiaozhi_mcp_bridge/README.md
tests/test_xiaozhi_mcp_bridge.py
docs/superpowers/plans/2026-07-07-xiaozhi-mcp-bridge.md
docs/phase-handoff-2026-07-07-xiaozhi-mcp-bridge.md
```

## 桥接服务工具

`smart_home_bridge.py` 使用 `FastMCP` 暴露以下工具：

```text
home_get_state
home_set_purifier
home_set_fresh_air
home_set_humidifier
home_set_auto
home_set_eco
home_set_environment_preset
home_set_manual_environment
home_disable_manual_environment
home_get_advice
```

对应 ESP32 HTTP API：

```text
GET  /api/state
POST /api/device
POST /api/mode
POST /api/environment
```

## 运行方式

先确认 ESP32 HTTP API 可访问：

```text
http://<ESP32_IP>:8080/api/state
```

然后设置环境变量：

```powershell
$env:MCP_ENDPOINT = "wss://api.xiaozhi.me/mcp/?token=你的新token"
$env:ESP32_BASE_URL = "http://<ESP32_IP>:8080"
```

安装依赖：

```powershell
python -m venv .venv_mcp_bridge
.\.venv_mcp_bridge\Scripts\Activate.ps1
python -m pip install --upgrade pip
python -m pip install -r tools\xiaozhi_mcp_bridge\requirements.txt
```

使用示例仓库的 `mcp_pipe.py` 启动：

```powershell
git clone https://github.com/78/mcp-calculator .cache\mcp-calculator
python .cache\mcp-calculator\mcp_pipe.py tools\xiaozhi_mcp_bridge\smart_home_bridge.py
```

如果现场网络无法访问 GitHub，需要提前下载示例仓库，至少准备 `mcp_pipe.py`。

## 验证结果

已按 TDD 红绿流程验证：

红灯：

```text
python -m unittest discover -s tests -p test_xiaozhi_mcp_bridge.py -v
ModuleNotFoundError: No module named 'tools.xiaozhi_mcp_bridge.smart_home_bridge'
```

绿灯：

```text
python -m unittest discover -s tests -p test_xiaozhi_mcp_bridge.py -v
Ran 5 tests
OK
```

完整源码级回归测试：

```text
python -m unittest discover -s tests -v
Ran 20 tests
OK
```

Python 语法检查和 JSON 检查通过：

```text
python -m py_compile tools\xiaozhi_mcp_bridge\smart_home_bridge.py tests\test_xiaozhi_mcp_bridge.py
python -m json.tool tools\xiaozhi_mcp_bridge\mcp_config.json
```

固件构建通过：

```text
idf.py -B build_codex_check build
Generated E:/espwork/xiaozhi-esp32/xiaozhi-esp32/build_codex_check/xiaozhi.bin
xiaozhi.bin binary size 0x245d80 bytes.
Smallest app partition is 0x3f0000 bytes.
0x1aa280 bytes (42%) free.
```

说明：本次先运行 `scripts/build_codex_check.ps1` 时，因为当前工作区缺少 `build_codex_check` 目录，脚本的前置路径检查失败。随后使用同一套 ESP-IDF 本地路径执行 `idf.py -B build_codex_check build` 重建构建目录并完成编译。构建完成后 `build_codex_check` 已恢复存在。

## 额外兼容修正

为了让现有回归测试通过，本阶段还做了一个不改变行为的日志文案修正：

```text
main/boards/bread-compact-wifi/smart_home_controller.cc
```

将新风输出日志中的：

```text
Apply fresh air servo fan output
```

改为：

```text
Apply fresh air fan output
```

该修改不改变当前 180°舵机往复逻辑，只统一测试期望的日志关键字。

## 安全注意

- 用户此前贴出的 token 已暴露在对话中，建议在小智平台重新生成。
- 不要把 `MCP_ENDPOINT` 真实 token 写入 `README.md`、`mcp_config.json`、截图或提交记录。
- `ESP32_BASE_URL` 是局域网地址，桥接服务必须运行在能访问 ESP32 的电脑上。

## 下一阶段建议

1. 用户重新生成小智 MCP token。
2. 确认 ESP32 已连 WiFi，浏览器可访问 `/api/state`。
3. 在电脑上启动 `mcp_pipe.py + smart_home_bridge.py`。
4. 在小智端确认能看到 `home_*` 工具。
5. 用语音测试“打开净化器二档”“模拟污染环境”“现在环境怎么样”。
6. 如果 AI 可以看到工具但控制失败，先看桥接服务日志和 ESP32 monitor 中 HTTP/执行器日志。
