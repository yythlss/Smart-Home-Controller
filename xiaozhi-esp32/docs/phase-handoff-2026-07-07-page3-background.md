# 2026-07-07 page3 手动环境背景图交付说明

## 1. 本阶段目标

为 USART HMI 的 `page3` 重新输出一张可直接导入的背景图，让底部的手动环境功能有明确可见入口。

本阶段只处理 HMI 背景图、控件说明和交付文档，不直接修改 `.HMI` 二进制工程文件。

## 2. 已生成资源

新版背景图：

```text
main/boards/bread-compact-wifi/ui_assets/page3_ai_settings_hmi_manual_env.png
```

图片参数：

```text
分辨率：480x272
用途：page3 AI 与设置页背景
特点：不写入动态传感器数据；底部已画出 手动 / 舒适 / 高温 / 干燥 / 污染 五个按钮
```

生成脚本：

```text
scripts/generate_page3_background.ps1
```

脚本使用 Windows 本地 `System.Drawing` 生成 PNG。脚本源码刻意避免直接写中文字符串，而是在运行时用 Unicode 码点生成中文，防止 Windows PowerShell 5 按 ANSI 解析 UTF-8 时出现乱码或语法错误。

## 3. HMI 编辑器手动修改步骤

打开当前 HMI 工程：

```text
D:/QQ/serial_warm_home  (1).HMI
```

进入 `page3` 后执行：

1. 删除或隐藏旧的 page3 背景图片对象。
2. 导入新版背景 `page3_ai_settings_hmi_manual_env.png`。
3. 将背景图片位置设置为：

```text
x=0
y=0
w=480
h=272
```

4. 将背景图片置于底层。
5. 如果编辑器支持锁定对象，锁定背景图片。
6. 保留或创建 `t_ai_state`：

```text
name=t_ai_state
type=文本
x=252
y=82
w=175
h=28
```

7. 保留或创建 `t_link_state`：

```text
name=t_link_state
type=文本
x=252
y=148
w=175
h=28
```

8. 在底部五个按钮上方创建透明触摸热区。热区只负责点击事件，按钮文字已经画在背景图里。

## 4. page3 触摸热区表

| 热区名 | 坐标 | 弹起事件(0) |
| --- | --- | --- |
| `hs_env_m` | `x=36,y=184,w=84,h=42` | `prints "BTN,ENV,MANUAL,TOGGLE",0` 后接 `printh 0a` |
| `hs_env_good` | `x=132,y=184,w=70,h=42` | `prints "BTN,ENV,SCENE,GOOD",0` 后接 `printh 0a` |
| `hs_env_hot` | `x=214,y=184,w=70,h=42` | `prints "BTN,ENV,SCENE,HOT",0` 后接 `printh 0a` |
| `hs_env_dry` | `x=296,y=184,w=70,h=42` | `prints "BTN,ENV,SCENE,DRY",0` 后接 `printh 0a` |
| `hs_env_bad` | `x=378,y=184,w=70,h=42` | `prints "BTN,ENV,SCENE,POLLUTED",0` 后接 `printh 0a` |
| `hs_back` | `x=10,y=8,w=120,h=42` | `prints "BTN,PAGE,HOME",0` 后接 `printh 0a` |

注意：

- 事件命令填写在右侧 `事件 -> 弹起事件(0)`，不是“输出”栏。
- 当前编辑器控件名限制为 14 个字符，上表命名均符合限制。
- 背景图上的按钮不能自动发送事件，必须叠加透明触摸热区。

## 5. 已同步文档和配置

已更新：

```text
main/boards/bread-compact-wifi/serial_hmi_widgets.json
../文档/串口屏手动事件配置手册.md
../文档/硬件连接与软件验证步骤.md
../文档/串口屏设计说明.md
docs/current-project-handoff.md
docs/phase-handoff-2026-07-06-air-curve-comfort-manual-env-ai.md
```

## 6. 验证结果

已完成：

```text
page3_ai_settings_hmi_manual_env.png 尺寸检查：480x272
serial_hmi_widgets.json JSON 语法检查：通过
```

本阶段未运行 ESP-IDF 编译，因为没有修改固件源码和构建配置。

## 7. 下一阶段目标

1. 用户在 USART HMI 编辑器中导入新版 `page3` 背景。
2. 用户按本文件第 4 节创建或调整透明触摸热区。
3. 编译并下载 HMI 工程到串口屏。
4. 烧录或运行当前 ESP32 固件，打开 monitor。
5. 点击五个环境按钮，确认 monitor 出现：

```text
Screen event: raw=BTN,ENV,MANUAL,TOGGLE
Screen event: raw=BTN,ENV,SCENE,GOOD
Screen event: raw=BTN,ENV,SCENE,HOT
Screen event: raw=BTN,ENV,SCENE,DRY
Screen event: raw=BTN,ENV,SCENE,POLLUTED
```

6. 再验证 `t_comfort`、`t_advice`、自动模式联动和小程序手动环境输入是否符合预期。

## 8. 遗留风险

- `.HMI` 文件仍需人工在编辑器中导入背景、创建热区、保存、编译和下载。
- 如果 `t_ai_state` 或 `t_link_state` 被背景图片遮挡，说明图层顺序错误，应把背景置于底层，把文本控件和热区放在上层。
- 如果点击按钮无日志，优先检查热区事件是否写在 `弹起事件(0)`，以及是否漏写 `printh 0a`。
