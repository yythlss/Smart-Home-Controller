# 工程文件索引与整理说明

本文档说明当前 `xiaozhi-esp32` 工程中与 `bread-compact-wifi` 串口屏/环境监测改造直接相关的有效文件、协作文档和已清理内容。接续开发前请先读本文档，再读对应源码和交付文档。

## 当前有效工程

| 项目 | 路径 |
| --- | --- |
| ESP-IDF 工程根目录 | `E:/espwork/xiaozhi-esp32/xiaozhi-esp32` |
| 当前板型目录 | `main/boards/bread-compact-wifi` |
| 队友代码来源 | `E:/espwork/xiaozhi-esp32/work1/bread-compact-wifi` |
| 当前 HMI 工程文件 | `hmi/serial_warm_home.HMI` |

`.HMI` 屏幕工程已经纳入 Git。修改页面、控件或事件后，应同步提交 `hmi/` 中的工程文件、`serial_hmi_widgets.json` 和相关说明文档。

## 当前有效源码

| 文件 | 作用 | 接续开发注意点 |
| --- | --- | --- |
| `main/boards/bread-compact-wifi/compact_wifi_board.cc` | 板级入口，初始化串口屏、按键、传感器任务和屏幕事件任务 | `SerialHmi` 是唯一 TJC UART 所有者，不要再新增第二套 UART 驱动 |
| `main/boards/bread-compact-wifi/config.h` | TJC、DHT11、MQ135、音频、按键引脚定义 | 改硬件接线前先同步硬件文档 |
| `main/boards/bread-compact-wifi/serial_hmi.h` | 串口屏数据结构和接口声明 | 新增页面/控件时先扩展契约 |
| `main/boards/bread-compact-wifi/serial_hmi.cc` | TJC 命令发送、GBK 转码、控件刷新、页面切换、事件解析 | 控件名变化时必须同步 `serial_hmi_widgets.json` |
| `main/boards/bread-compact-wifi/utf8_to_gbk.h` | UTF-8 到 GBK 转码接口 | 来自队友 `work1`，由 `SerialHmi::SendCommand()` 调用 |
| `main/boards/bread-compact-wifi/utf8_to_gbk.cc` | UTF-8 解码和 GBK 表查找实现 | 随板型目录自动参与构建 |
| `main/boards/bread-compact-wifi/gbk_table.inc` | Unicode 到 GBK 映射表 | 大型生成表，供屏幕中文显示使用，不手工编辑 |
| `main/boards/bread-compact-wifi/dht11_sensor.h` | DHT11 驱动声明 | 可保持 |
| `main/boards/bread-compact-wifi/dht11_sensor.cc` | DHT11 开漏上拉、微秒级时序读取实现 | 已整合队友时序优化 |
| `main/boards/bread-compact-wifi/mq135_sensor.h` | MQ135 驱动声明 | 可保持 |
| `main/boards/bread-compact-wifi/mq135_sensor.cc` | MQ135 ADC 读取和演示级等级估算 | 后续需要标定或替换为真实空气质量传感器 |

## 当前有效串口屏资源

| 文件/目录 | 作用 |
| --- | --- |
| `main/boards/bread-compact-wifi/serial_hmi_widgets.json` | HMI 页面、控件、资源和事件契约 |
| `../文档/串口屏设计说明.md` | 手机式图标页和滑动切换设计说明 |
| `../文档/串口屏手动事件配置手册.md` | USART HMI 编辑器中需要手动配置的触摸热区、事件和验收步骤 |
| `main/boards/bread-compact-wifi/ui_assets/` | 当前推荐导入或复刻到 HMI 编辑器的页面背景、图标和滑动示意图 |
| `../文档/硬件连接与软件验证步骤.md` | 硬件接线、构建、烧录和验证流程 |
| `../文档/智能家居外设接线与验证步骤.md` | 2026-07-05 新增智能家居、MCP、HTTP API、小程序、曲线和外设的完整接线与验证手册 |
| `scripts/hmi_background_probe.ps1` | HMI 后台资源探测脚本，只用于扫描资源和生成/替换测试副本 |
| `tools/xiaozhi_mcp_bridge/` | 小智远端 MCP 到 ESP32 局域网 HTTP API 的桥接服务，同时提供天气、RSS 新闻和综合建议工具 |

## 当前重要协作文档

| 文件 | 作用 |
| --- | --- |
| `AGENTS.md` | 可选工作区规则文件；当前仓库未包含，后续若新增则开发前阅读 |
| `task_plan.md` | 本阶段任务拆解和状态 |
| `findings.md` | 本阶段工程发现、整合判断和风险记录 |
| `progress.md` | 本阶段执行进度记录 |
| `docs/current-project-handoff.md` | 当前工程状态交付说明 |
| `docs/continuation-notes.md` | 接续开发注意事项 |
| 仓库根目录 `mini_program_demo/README.md` | 小程序局域网演示工程的详细导入、HTTP API 预验证、模拟器/真机验收和故障排查手册 |
| `docs/phase-handoff-2026-07-28-mini-program-demo-enhancements.md` | 离线演示、雷达图、趋势曲线、自动化、场景和事件日志阶段交付 |
| `docs/project-file-map.md` | 本文件，说明有效文件和冗余文件边界 |
| `docs/phase-handoff-2026-06-09-work1-integration.md` | 本次队友代码整合阶段交付文档 |
| `docs/phase-handoff-2026-06-09-serial-screen-score-page.md` | 串口屏 page1 空气评分页阶段交付文档 |
| `docs/phase-handoff-2026-06-10-hmi-background-script-probe.md` | HMI 后台脚本探测阶段交付文档 |
| `docs/phase-handoff-2026-07-05-smart-home-control.md` | 智能家居执行器、自动/节能、MCP 和空气曲线阶段交付文档 |
| `docs/phase-handoff-2026-07-05-mini-program-http-api.md` | 小程序方案 A 与局域网 HTTP API 阶段交付文档 |
| `docs/phase-handoff-2026-07-06-mini-program-verification-guide.md` | 小程序详细验证步骤、模拟器/真机联调和故障排查交付文档 |
| `docs/phase-handoff-2026-07-06-continuous-servo-fan.md` | 360°连续旋转舵机风扇改造阶段交付文档 |
| `docs/phase-handoff-2026-07-06-full-verification-guide.md` | 7 月 5 日新增功能完整接线与验证手册阶段交付文档 |
| `docs/phase-handoff-2026-07-06-http-api-startup-crash-fix.md` | HTTP API 启动过早导致 `Invalid mbox` 重启的修复交付文档 |
| `docs/phase-handoff-2026-07-06-smart-home-actuator-output-debug.md` | 智能家居外设输出诊断、触摸切换 0 档修复和 Brownout 排查阶段交付文档 |
| `docs/phase-handoff-2026-07-06-air-curve-comfort-manual-env-ai.md` | 空气曲线、舒适度/建议、手动环境、小程序和 AI/MCP 工具增强交付文档 |
| `docs/phase-handoff-2026-07-07-xiaozhi-mcp-bridge.md` | 小智远端 MCP 桥接服务交付文档 |
| `docs/phase-handoff-2026-07-14-smart-sensing-ai-automation.md` | 雷达/光敏/灯光/报警接口、节能重做、天气新闻和唤醒方案的当前交付文档 |
| `docs/phase-handoff-2026-07-28-software-reliability.md` | 并发安全、健康诊断、手动覆盖、雷达区域、可选鉴权、小程序和 MCP 纯软件增强交付文档 |
| `../文档/串口屏与环境监测项目交付说明.md` | 板型目录内的队友交付说明 |

## 已清理的冗余内容

以下内容已经删除，不再作为交付文件：

| 路径 | 删除原因 |
| --- | --- |
| `main/boards/bread-compact-wifi/ui_mockups/` | 旧设计稿和生成脚本，已被 `ui_assets/` 中的有效资源替代 |
| `main/boards/bread-compact-wifi/tjc_screen_implementation_plan.md` | 旧运行时绘图方案，已被控件化 HMI 页面方案替代 |

## 低优先级上游资料

以下资料不是当前串口屏整合的核心交付内容，但属于上游工程说明，暂时保留：

| 文件/目录 | 说明 |
| --- | --- |
| `docs/blufi*.md`、`docs/mcp*.md`、`docs/mqtt*.md`、`docs/websocket*.md` | 小智上游通信说明，后续接入联网或协议能力时可参考 |
| `docs/v0/`、`docs/v1/` | 上游硬件图片资料，不是当前板型核心交付 |
| `managed_components/` | ESP-IDF 组件管理器拉取的工程局部依赖，按当前工程状态保留 |

## 临时和可再生成内容

| 路径 | 说明 |
| --- | --- |
| `build/` | ESP-IDF 默认构建目录 |
| `build_codex_check/` | 本机独立验证构建目录，不作为源码交付重点 |
| `dependencies.lock` | ESP-IDF 组件锁定文件，当前按工程 `.gitignore` 处理 |
| `.serena/` | Codex/Serena 本地项目记忆与索引，便于接续开发；是否提交由团队决定 |
| `tmp_hmi_probe_serial_warm_home_copy.HMI` | HMI 原文件探测副本，可按需重新生成 |
| `tmp_hmi_probe_page1_bg_replace.HMI` | 已替换 page1 背景资源槽位的 HMI 测试副本，需编辑器打开验证后才能继续使用 |
| `tmp_hmi_probe_pngs/` | HMI 内嵌 PNG 提取结果，用于定位资源，不作为正式交付重点 |

## 接续阅读顺序

1. 若仓库存在 `AGENTS.md`，先阅读；当前版本未包含该文件
2. `docs/current-project-handoff.md`
3. `docs/project-file-map.md`
4. `docs/continuation-notes.md`
5. `docs/phase-handoff-2026-06-10-hmi-background-script-probe.md`
6. `docs/phase-handoff-2026-07-06-http-api-startup-crash-fix.md`
7. `docs/phase-handoff-2026-07-06-smart-home-actuator-output-debug.md`
8. `docs/phase-handoff-2026-07-06-mini-program-verification-guide.md`
9. `docs/phase-handoff-2026-07-06-air-curve-comfort-manual-env-ai.md`
10. `docs/phase-handoff-2026-07-07-xiaozhi-mcp-bridge.md`
11. `docs/phase-handoff-2026-07-14-smart-sensing-ai-automation.md`
12. `docs/phase-handoff-2026-07-28-software-reliability.md`
13. `../文档/串口屏与环境监测项目交付说明.md`
14. `../文档/串口屏手动事件配置手册.md`
15. `../文档/智能家居外设接线与验证步骤.md`
16. 仓库根目录 `mini_program_demo/README.md`
17. `main/boards/bread-compact-wifi/serial_hmi_widgets.json`
18. `main/boards/bread-compact-wifi/compact_wifi_board.cc`
19. `main/boards/bread-compact-wifi/serial_hmi.cc`

## 整理原则

- 当前以 HMI 控件化页面为唯一有效方向，不再保留旧运行时整页绘图方案。
- 删除仅限明确冗余且已被新资源替代的板型目录文件。
- 上游协议文档暂不删除，避免后续接入联网、MCP、MQTT 或 WebSocket 时缺参考资料。
- 每完成一阶段任务后，必须更新阶段交付文档，保证队友可以准确接续。
