# 2026-07-30 控制逻辑与 AI 优化最终交付

## 当前工程状态

- 仓库：`E:\espwork\Smart-Home-Controller`
- 开发分支：`codex/logic-ai-optimization`
- 基线提交：`6a3ee86756708b9efa68c066ae1845215cfee322`
- 当前板型：`bread-compact-wifi`
- 目标芯片：ESP32-S3
- 受保护的 `.vscode/**`、`sdkconfig*`、ESP-IDF 工具链和全局环境变量均未修改。
- 180 度角度舵机扇叶逻辑保持不变：`GPIO21`，按档位在不同角度范围内往复摆动。

## 本阶段完成内容

### 1. 自动与节能控制策略

- 新增纯 C++ 策略模块 `smart_home_policy.h/.cc`，统一计算净化、新风和加湿的完整目标档位。
- 自动模式增加空气质量、湿度和温度回差，减少阈值附近频繁启停。
- 保留 30 分钟手动覆盖；已知无人时关闭全部可控输出。
- 自定义自动化规则并入同一策略入口，模式、场景或无人关机后会清理旧的规则激活状态。
- 仅在目标档位变化时写 LED PWM 或更新舵机档位，减少重复操作和日志。

### 2. 传感器与手动环境状态

- 温度、湿度和 MQ135 原始值进入控制器前进行有限值与范围校验。
- `last_sensor_sample_` 与 `manual_sample_` 分开保存。
- 手动环境模式期间继续保存真实采样；退出后立即恢复最近一次真实数据。
- 手动数据不参与真实传感器突变告警，避免切换预设时误报警。

### 3. 串口屏连续显示与曲线刷新

- `SerialHmi` 使用 FreeRTOS 递归互斥锁串行化 UART 发送。
- 页面切换、`ref_stop`、控件更新和 `ref_star` 作为一个事务发送，页面号判断和防抖也位于事务内。
- 停留空气详情页时每次采样只追加一个曲线点；只有进入详情页时才清空并回放最近 30 点。
- 两次 5 秒采样之间不发送页面重置命令，屏幕继续保持上一帧数据。

### 4. 设备端 MCP

- 设备端继续作为净化、新风、加湿、灯光、自动、节能和场景控制的权威入口。
- 所有设备端工具统一返回 `ok`、`action`、`message`、`data`，错误响应也保留空 `data` 对象。
- 新增 `self.home.get_summary`、`self.home.get_health` 和 `self.home.apply_scene`。
- `self.home.update_context`、`self.home.set_manual_environment`、`self.home.set_environment_preset` 默认隐藏；需要联调时可临时调整 `SMART_HOME_ENABLE_DEBUG_MCP_TOOLS`。

### 5. PC MCP 桥

- 默认 `external` 模式只注册天气、新闻和室内外组合建议三个工具，避免与设备端 MCP 重复控制。
- `full` 模式保留旧 HTTP 控制工具，仅用于兼容已有部署。
- 网络、配置、HTTP、JSON 和 XML 错误统一转为结构化错误，单次调用失败不会退出 MCP 进程。
- 日志写入标准错误流，不干扰 MCP 标准输入输出协议。
- 兼容建议工具会正确传播 ESP32 状态依赖错误，不再返回内容为空的伪成功结果。

## 主要改动文件

- `.github/workflows/quality.yml`
- `main/boards/bread-compact-wifi/config.h`
- `main/boards/bread-compact-wifi/serial_hmi.h/.cc`
- `main/boards/bread-compact-wifi/smart_home_controller.h/.cc`
- `main/boards/bread-compact-wifi/smart_home_policy.h/.cc`
- `tools/xiaozhi_mcp_bridge/smart_home_bridge.py`
- `tools/xiaozhi_mcp_bridge/mcp_config.json`
- `tools/xiaozhi_mcp_bridge/README.md`
- `scripts/start_xiaozhi_mcp_bridge.ps1`
- `tests/native/smart_home_policy_test.cpp`
- `tests/test_smart_home_policy_native.py`
- `tests/test_bread_compact_wifi_regressions.py`
- `tests/test_xiaozhi_mcp_bridge.py`

## 验证结果

- `python -B -m unittest discover -s tests -v`：44 项通过，1 项跳过。跳过原因是 Windows PATH 中没有主机 `g++/clang++`；GitHub Actions 会在 Ubuntu 上强制编译并运行该测试。
- ESP-IDF v5.5.2 增量构建：通过，目标为 ESP32-S3。
- `xiaozhi.bin` 大小：`0x24c4d0` 字节；最小应用分区剩余 `0x1a3b30` 字节，约 42%。
- Python `py_compile`：通过。
- 小程序 4 个 JavaScript 文件 `node --check`：全部通过。
- HMI、MCP 和小程序 5 个 JSON 文件解析：全部通过。
- `git diff --check`：通过。
- 已跟踪文件和本次差异未发现 JWT、私钥等真实密钥。

## 构建生成文件说明

- 构建使用独立目录 `build-codex` 和临时 sdkconfig，没有修改仓库中的 `sdkconfig`。
- `dependencies.lock` 和 `main/assets/lang_config.h` 在构建过程中被触碰，但内容哈希与基线一致，不进入提交。

## 仍需人工验证

1. 烧录新固件，停留首页至少 30 秒，确认 5 秒采样间隔内数据一直保留。
2. 停留空气详情页，确认曲线每 5 秒追加一个点，没有整页清空闪烁。
3. 在采样刷新附近快速切换页面，确认不跳回初始页、不出现半页旧数据。
4. 分别测试净化、新风、加湿三档，确认 LED 亮度和 180 度舵机摆动范围符合实物。
5. 在小智控制台确认设备端只出现正式 `self.home.*` 工具，电脑端默认只出现 3 个外部服务工具。
6. 实测“打开净化二档”“进入睡眠场景”“检查设备健康”“查询天气并给出室内外建议”。
7. 此前在聊天中使用过的 MCP 接入 token 建议重新生成，继续只通过环境变量传入。

## 下一阶段建议

- 根据实物串口日志和屏幕录像微调页面切换延时与舵机档位参数。
- 为设备端 MCP 增加真机语音调用记录，确认每个意图只命中一个控制工具。
- 后续可把传感器健康状态和控制执行结果同步到小程序后台，形成统一诊断入口。
