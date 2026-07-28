# 2026-07-06 阶段交付：7 月 5 日新增功能完整接线与验证手册

## 本阶段目标

用户要求把线路连接、验证步骤等写成详细文档，并进一步明确不只是风扇部分，而是要覆盖 2026-07-05 新增的所有功能。本阶段补充一份完整验证手册，覆盖智能家居执行器、串口屏事件、自动/节能、空气曲线、MCP、HTTP API 和微信小程序。

## 本阶段新增或更新文件

- `../文档/智能家居外设接线与验证步骤.md`
  - 新增完整接线和验证手册。
  - 覆盖 `GPIO13` 净化 LED、`GPIO14` 加湿 LED、`GPIO21` 360°连续旋转舵机风扇、TJC 串口屏、DHT11、MQ135。
  - 覆盖串口屏 `BTN,DEVICE,...`、`BTN,MODE,...` 事件下载前和下载后验证。
  - 覆盖 `c_air` 空气质量曲线、`/api/state`、`/api/history`、`/api/device`、`/api/mode`、小程序演示工程和 MCP 工具验证。
- `docs/project-file-map.md`
  - 登记新验证手册。
  - 更新接续阅读顺序。
- `docs/current-project-handoff.md`
  - 把新验证手册加入当前有效文档列表。
- `../文档/串口屏与环境监测项目交付说明.md`
  - 登记新验证手册。
  - 修正旧状态描述：智能家居按钮事件已接入真实外设控制逻辑。
- `progress.md`
  - 记录本阶段文档交付。

## 验证手册覆盖范围

- 线路连接：
  - TJC 串口屏 `GPIO41/GPIO42`
  - DHT11 `GPIO18`
  - MQ135 `GPIO1 / ADC1_CH0`
  - 净化 LED `GPIO13`
  - 加湿 LED `GPIO14`
  - 连续旋转舵机风扇 `GPIO21`
- 固件验证：
  - `python -m unittest discover -s tests -v`
  - `scripts/build_codex_check.ps1`
  - 烧录与 monitor 启动日志
- 串口屏验证：
  - HMI 事件填写位置
  - `hs_purifier`
  - `hs_fan`
  - `hs_humid`
  - `hs_auto`
  - `hs_eco`
- 执行器验证：
  - 净化、加湿、新风/风扇三类设备的 `0-3` 档
  - 自动模式
  - 节能模式
- 数据验证：
  - DHT11 温湿度
  - MQ135 原始值
  - 空气评分
  - `c_air` 曲线 `add 12,0,val`
  - HTTP 历史数据最多 30 条
- 联网验证：
  - `/api/state`
  - `/api/history`
  - `/api/device`
  - `/api/mode`
  - 微信小程序 `docs/mini_program_demo`
  - MCP 工具

## 验证结果

本阶段主要是文档补充，未改动固件源码。已做文档结构和关键词核验，确认新手册包含接线、HMI、三类执行器、自动/节能、MCP、HTTP API、小程序、空气曲线和历史数据章节。

## 遗留事项

- 文档中的实机验收项仍需要用户或队友在真实硬件上逐项打勾。
- 连续旋转舵机风扇的 `1500us` 停止中位、`1600us/1750us/1900us` 档位仍需实机校准。
- `hs_eco` 事件需要在 HMI 编辑器中确认已改为 `BTN,MODE,ECO,TOGGLE`。
- 当前温度/湿度曲线控件数字 ID 尚未确认，固件只启用空气质量曲线 `c_air=12`。
