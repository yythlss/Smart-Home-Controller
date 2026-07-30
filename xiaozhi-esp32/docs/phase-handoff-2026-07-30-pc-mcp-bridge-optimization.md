# 2026-07-30 PC MCP 桥优化阶段交付

## 完成内容

- PC 桥默认模式改为 `external`，只向小智注册天气、新闻和室内外组合建议三个工具。
- 设备状态与控制函数继续保留在代码中，但默认不注册，供旧部署调用和测试。
- 设置 `XIAOZHI_MCP_BRIDGE_MODE=full` 可恢复原有 HTTP 转发工具。
- 新增 `BridgeError` 和统一错误结构：`ok=false`、`tool`、`error.code`、`error.message`、可选 `details`。
- ESP32 不可达、HTTP 错误、无效 JSON、外部服务不可达、无效 XML、缺少配置等情况都有稳定错误码。
- 新增标准错误流日志，MCP 标准输入输出协议不受日志干扰。
- 启动脚本新增 `BridgeMode` 参数；`external` 模式不再强制要求 ESP32 地址，`full` 模式仍要求地址并执行 HTTP 预检查。
- 更新 `mcp_config.json` 和桥接 README，说明设备端 MCP 与电脑端外部服务的职责划分。

## 验证结果

- `python -m py_compile tools\xiaozhi_mcp_bridge\smart_home_bridge.py`：通过。
- `python -m unittest discover -s tests -v`：43 项通过，1 项因本机无主机 C++ 编译器而跳过。
- 单元测试确认默认仅注册 3 个外部服务工具，`full` 模式仍可识别。

## 部署说明

- 推荐：设备端 MCP 负责家居控制，PC 桥使用 `external`。
- 旧部署过渡：暂时设置 `XIAOZHI_MCP_BRIDGE_MODE=full`，完成设备端 MCP 验证后切回 `external`。
- 实际 MCP token 仍只通过环境变量提供，禁止写入仓库。
