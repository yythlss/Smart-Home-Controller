# 阶段交付：串口屏 page1 空气评分详情页优化

日期：`2026-06-09`

适用工程：

```text
E:/espwork/xiaozhi-esp32/xiaozhi-esp32
```

当前板型：

```text
main/boards/bread-compact-wifi
```

当前 HMI 工程：

```text
D:/QQ/serial_warm_home .HMI
```

## 1. 本阶段目标

把 page1 从“PM2.5/CO2/TVOC 浓度占位页”调整为符合当前硬件事实的“空气评分详情页”。

当前传感器能力：

| 传感器 | 当前可用数据 | 当前不可直接得到的数据 |
| --- | --- | --- |
| DHT11 | 温度、湿度 | 无 |
| MQ135 | ADC 原始值、阈值估算等级、阈值估算评分 | PM2.5、CO2、TVOC 的准确浓度 |

因此当前 page1 不再要求 `t_pm25`、`t_co2`、`t_tvoc_level`。这些浓度字段只作为未来接入真实传感器后的扩展方向。

## 2. 本阶段已完成

- 重新生成 `page1_air_detail_hmi_blank.png`，改成空气评分仪表盘 + 等级卡片 + MQ135 原始值卡片 + 趋势图装饰 + 温湿度/舒适度卡片。
- 新背景不包含固定动态读数，也不写入中文标签，避免假数据、遮挡痕迹和中文渲染乱码。
- 修改 `SerialHmi::RefreshAirDetailPage()`，page1 改为刷新当前真实可用数据。
- 更新 `serial_hmi_widgets.json`，page1 控件契约改为评分页控件。
- 更新 `串口屏手动事件配置手册.md`，新增详细 page1 手工配置步骤。
- 更新 `串口屏设计说明.md`、`硬件连接与软件验证步骤.md`、`串口屏与环境监测项目交付说明.md`、`docs/current-project-handoff.md`、`docs/continuation-notes.md`、`docs/project-file-map.md`。

## 3. 改动文件

| 文件 | 改动 |
| --- | --- |
| `main/boards/bread-compact-wifi/ui_assets/page1_air_detail_hmi_blank.png` | 替换为新的空气评分详情页背景 |
| `main/boards/bread-compact-wifi/serial_hmi.cc` | page1 停止写 PM2.5/CO2/TVOC，占用新控件显示评分、等级、原始值、温湿度、舒适度 |
| `main/boards/bread-compact-wifi/serial_hmi.h` | 标注 PM2.5/CO2/TVOC 字段为未来真实传感器预留 |
| `main/boards/bread-compact-wifi/serial_hmi_widgets.json` | 更新 page1 必备控件清单，补充可选曲线控件说明 |
| `../文档/串口屏手动事件配置手册.md` | 写入 page1 新背景导入、控件创建、曲线控件限制、让 Codex 辅助导入的条件 |
| `../文档/串口屏设计说明.md` | page1 设计改为“空气评分详情” |
| `../文档/硬件连接与软件验证步骤.md` | 更新 HMI 控件清单和验证步骤 |
| `../文档/串口屏与环境监测项目交付说明.md` | 更新队友交付说明和 page1 刷新示例 |
| `docs/current-project-handoff.md` | 更新当前状态和下一阶段目标 |
| `docs/continuation-notes.md` | 更新接续开发注意事项 |
| `docs/project-file-map.md` | 增加本阶段交付文档索引 |

## 4. page1 当前控件契约

必须创建：

| 控件名 | 类型 | 建议位置 | 固件写入示例 |
| --- | --- | --- | --- |
| `t_air_score` | 文本 | `x=70,y=116,w=78,h=32` | `t_air_score.txt="75/100"` |
| `j_air_detail` | 进度条 | `x=42,y=160,w=132,h=10` | `j_air_detail.val=75` |
| `t_air_state` | 文本 | `x=224,y=92,w=84,h=22` | `t_air_state.txt="良"` |
| `t_air_raw` | 文本 | `x=348,y=92,w=92,h=22` | `t_air_raw.txt="820"` |
| `t_comfort` | 文本 | `x=38,y=158,w=124,h=22` | `t_comfort.txt="舒适"` |
| `t_temp_d` | 文本 | `x=38,y=226,w=78,h=20` | `t_temp_d.txt="26.0 C"` |
| `t_humi_d` | 文本 | `x=162,y=226,w=78,h=20` | `t_humi_d.txt="58.0 %"` |
| `hs_back` | 触摸热区 | `x=10,y=8,w=105,h=42` | `BTN,PAGE,HOME` |

不要作为当前 page1 必备控件：

```text
t_pm25
t_co2
t_tvoc_level
```

## 5. HMI 编辑器手工操作步骤

1. 打开 `E:/USART_HMI/USART HMI.exe`。
2. 打开 `D:/QQ/serial_warm_home .HMI`。
3. 进入 `page1`。
4. 删除或隐藏旧 page1 背景。
5. 导入新背景：

```text
E:/espwork/xiaozhi-esp32/xiaozhi-esp32/main/boards/bread-compact-wifi/ui_assets/page1_air_detail_hmi_blank.png
```

6. 设置背景图片 `x=0,y=0,w=480,h=272`。
7. 把背景图放到最底层。
8. 创建第 4 节列出的文本、进度条和触摸热区控件。
9. 确认所有数据控件名与第 4 节完全一致。
10. 选中 `hs_back`，在右侧 `事件 -> 弹起事件(0)` 填：

```text
prints "BTN,PAGE,HOME",0
printh 0a
```

11. 不勾选 `发送键值`。
12. 保存 HMI 工程。
13. 编译 HMI 工程。
14. 下载到 TJC 串口屏。
15. 烧录 ESP32 固件并打开 monitor。
16. 从首页点击空气图标进入 page1。
17. 确认 monitor 出现以下新控件命令：

```text
[TJC] t_air_score.txt="75/100"
[TJC] j_air_detail.val=75
[TJC] t_air_state.txt="良"
[TJC] t_air_raw.txt="820"
[TJC] t_temp_d.txt="26.0 C"
[TJC] t_humi_d.txt="58.0 %"
[TJC] t_comfort.txt="舒适"
```

数值会随传感器实际读数变化，上面只是示例。

## 6. 关于曲线/波形控件

当前背景图已经包含静态趋势图装饰，页面视觉上不空。

如果要接入真实曲线控件，可以在 HMI 中创建：

| 控件名 | 类型 | 建议位置 |
| --- | --- | --- |
| `c_air` | 曲线/波形 | `x=224,y=166,w=218,h=54` |

但固件暂时不写入 `c_air`，原因：

- TJC 曲线命令是 `add objid,ch,val`。
- `objid` 是编辑器分配的数字 ID，不是 `c_air` 这个名字。
- 必须先由人工在编辑器里记录数字 ID，才能让固件安全发送曲线数据。

下一阶段如果要启用曲线，请先记录：

```text
c_air 数字 ID = <在编辑器属性栏看到的数字>
```

然后再在固件中封装 `AddCurvePoint()` 之类的接口。

## 7. 如何让我帮助完成设计元素导入

我已经能在工程里完成：

- 生成或修正 PNG 页面背景和图标。
- 给出控件坐标、尺寸、名称和事件脚本。
- 修改固件写入控件。
- 检查截图中的遮挡、命名、布局问题。
- 更新交付文档。

不能安全直接做：

- 直接用文本方式改 `.HMI` 二进制文件。
- 在未知曲线数字 ID 时改固件写 `add` 曲线命令。

如果希望我尽量代操作编辑器，需要：

1. 你手动打开 `E:/USART_HMI/USART HMI.exe`。
2. 你手动打开 `D:/QQ/serial_warm_home .HMI`。
3. 保持编辑器窗口在前台。
4. 当前 Codex 环境需要有桌面 GUI 自动化能力。如果没有，我只能继续通过资源文件、文档、截图检查和坐标清单辅助，最后点击、保存、编译、下载仍需要你手动完成。

更稳的方式是：你按本文档完成导入后发截图，我检查控件位置、遮挡、事件栏填写和日志是否匹配。

## 8. 当前验证状态

已完成静态和构建验证：

| 验证项 | 结果 |
| --- | --- |
| `serial_hmi_widgets.json` JSON 格式 | 通过 |
| HMI 控件名长度不超过 14 字符 | 通过 |
| `page1_air_detail_hmi_blank.png` 尺寸 | 通过，`480x272` |
| `serial_hmi.cc` 和 `serial_hmi_widgets.json` 中旧 page1 浓度控件写入残留 | 通过，未发现 `t_pm25`、`t_co2`、`t_tvoc_level` |
| 受保护配置文件检查 | 通过，未改动 `.vscode/**`、`sdkconfig*`、`CMakePresets.json` |
| ESP-IDF 构建 | 通过 |

构建命令：

```powershell
& 'D:\esp\Espressif\frameworks\esp-idf-v5.5.2\export.ps1'
idf.py -B build_codex_check build
```

构建结果：

```text
Project build complete.
xiaozhi.bin binary size 0x23f030 bytes.
Smallest app partition is 0x3f0000 bytes.
0x1b0fd0 bytes (43%) free.
```

仍需人工验证：

- `D:/QQ/serial_warm_home .HMI` 中 page1 是否已导入新背景。
- 新控件是否已经在 HMI 编辑器中创建。
- HMI 工程是否已经保存、编译、下载到屏幕。
- 实机点击 `hs_back` 是否返回首页。
- 实机 page1 是否显示新控件数据。

## 9. 下一阶段建议

1. 人工按本文档完成 HMI page1 导入和控件创建。
2. 实机验证 page1 新控件刷新。
3. 如需要真实趋势曲线，记录 `c_air` 数字 ID 后再改固件。
4. 标定 MQ135 阈值，避免当前演示评分与实际空气情况偏差过大。
5. 决定是否继续接入 AI 问答显示控件或真实智能家居外设。
