# 2026-07-30 设备端 MCP 优化阶段交付

## 完成内容

- 设备端 MCP 继续作为净化、新风、加湿、灯光、自动模式和节能模式的权威控制入口。
- 所有设备端 MCP 工具统一返回 `ok`、`action`、`message`、`data` 四个字段，不再混用布尔值和裸状态对象。
- 新增 `self.home.get_summary`，返回适合语音播报的环境与设备摘要。
- 新增 `self.home.get_health`，返回网络、串口屏、DHT11、MQ135、光照和雷达健康状态。
- 新增 `self.home.apply_scene`，支持 `HOME`、`AWAY`、`SLEEP`、`VENTILATE`、`CLEAN`。
- `self.home.update_context`、`self.home.set_manual_environment`、`self.home.set_environment_preset` 改为调试工具。
- `config.h` 新增 `SMART_HOME_ENABLE_DEBUG_MCP_TOOLS`，默认值为 `0`，正常固件不注册调试注入工具。

## 对 AI 交互的影响

- AI 可以先调用 `get_summary` 回答“家里现在怎么样”。
- AI 可以调用 `get_health` 区分“环境数据异常”和“执行器控制失败”。
- “回家模式、离家模式、睡眠模式、通风、强力净化”等复合指令可以一次调用场景工具完成。
- 控制结果带有执行后的完整状态，AI 可以直接确认实际档位和模式。

## 验证结果

- `python -m unittest discover -s tests -v`：39 项通过，1 项因本机无主机 C++ 编译器而跳过。
- 回归测试确认三个调试 MCP 工具默认受编译开关保护。

## 后续事项

- PC MCP 桥需要默认只保留天气、新闻和组合建议，避免重复注册设备控制工具。
- 仍需 ESP-IDF 编译确认设备端 MCP 返回类型和新增方法均可在目标工具链通过。
