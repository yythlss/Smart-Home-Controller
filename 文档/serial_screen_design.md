# bread-compact-wifi 串口屏设计说明

本文档是当前 `bread-compact-wifi` 板型的串口屏设计入口。当前方向是“手机式首页 + 软件图标入口 + 左右滑动切页”，页面结构由 USART HMI 工程维护，ESP32 固件只负责更新控件值和消费页面事件。

## 当前边界

- HMI 工程文件：`D:/QQ/serial_warm_home .HMI`。
- 屏幕规格：TJC/USART HMI 4.3 寸，建议 `480x272` 横屏。
- 固件通信：`UART2`，ESP32 `GPIO41` 发到屏幕 RX，`GPIO42` 收屏幕 TX，`9600 8N1`。
- 固件不再默认使用 `cls/fill/draw/xstr` 整页绘图，避免覆盖手工页面。
- 页面图标、背景、触摸热区、滑动动画由 USART HMI 编辑器实现。
- ESP32 通过 `SerialHmi::SetText()`、`SerialHmi::SetValue()` 更新控件。

## 视觉资源

当前推荐使用的资源目录：

```text
main/boards/bread-compact-wifi/ui_assets
```

资源说明：

| 文件 | 用途 |
| --- | --- |
| `page0_home_launcher_hmi_blank.png` | 手机式首页 HMI 实际导入底图，不含固定传感器数据 |
| `page1_air_detail_hmi_blank.png` | 空气详情页 HMI 实际导入底图，不含固定传感器数据 |
| `page2_smart_home_hmi_blank.png` | 智能家居控制页 HMI 实际导入底图 |
| `page3_ai_settings_hmi_manual_env.png` | AI 与设置页 HMI 实际导入底图，不含固定状态数据，底部包含手动环境模拟按钮 |
| `icon_air.png` | 空气详情图标 |
| `icon_home.png` | 智能家居图标 |
| `icon_ai.png` | AI 图标 |
| `icon_settings.png` | 设置图标 |
| `transition_swipe_left.png` | 左滑动效示意 |
| `transition_swipe_right.png` | 右滑动效示意 |

USART HMI 编辑器中可以有两种用法：

- 把 `page*.png` 作为页面背景，叠加透明文本、进度条和触摸热区。
- 仅按资源图复刻控件布局，不导入背景图；这种方式更容易后续编辑，但搭建时间更长。

## 页面规划

### page0：首页与软件图标

用途：开机默认页，像手机桌面一样提供入口。

必须保留的数据控件：

| 控件名 | 类型 | 固件更新示例 |
| --- | --- | --- |
| `t_temp` | 文本 | `t_temp.txt="26.3 C"` |
| `t_humi` | 文本 | `t_humi.txt="58.0 %"` |
| `t_air_state` | 文本 | `t_air_state.txt="良"` |
| `t_air` | 文本 | `t_air.txt="良(820)"` |
| `j_air` | 进度条 | `j_air.val=75` |
| `t_advice` | 文本 | `t_advice.txt="Keep ventilation"` |
| `t_ai_state` | 文本 | `t_ai_state.txt="IDLE"` |

软件图标热区：

| 控件名 | 位置建议 | 事件 |
| --- | --- | --- |
| `hs_air` | 空气图标 | `BTN,PAGE,AIR_DETAIL` |
| `hs_home` | 家居图标 | `BTN,PAGE,SMART_HOME` |
| `hs_ai` | AI 图标 | `BTN,PAGE,AI` |
| `hs_settings` | 设置图标 | `BTN,PAGE,SETTINGS` |

### page1：空气评分详情

用途：按当前硬件能力展示空气评分、空气等级、MQ135 原始值、温湿度和舒适度。当前 MQ135 不能直接提供 PM2.5、CO2、TVOC 浓度，所以本页不再把这些浓度作为必备字段。

控件：

| 控件名 | 类型 | 当前状态 |
| --- | --- | --- |
| `t_air_score` | 文本 | 显示 `75/100` 这类评分文本 |
| `j_air_detail` | 进度条 | 显示 `0-100` 空气评分 |
| `t_air_state` | 文本 | 显示 `优`、`良`、`轻度污染`、`重度污染` |
| `t_air_raw` | 文本 | 显示 MQ135 ADC 原始值 |
| `t_temp_d` | 文本 | 显示 DHT11 温度 |
| `t_humi_d` | 文本 | 显示 DHT11 湿度 |
| `t_comfort` | 文本 | 根据 DHT11 温湿度估算 |
| `hs_back` | 触摸热区 | `BTN,PAGE,HOME` |

可选曲线控件：

| 控件名 | 类型 | 当前状态 |
| --- | --- | --- |
| `c_air` | 曲线/波形 | 当前 `D:/QQ/serial_warm_home  (1).HMI` 已确认数字 ID 为 `12`，固件发送 `add 12,0,<air_score>` |

`c_temp` 和 `c_humi` 暂未在当前 HMI 文件中确认。温度/湿度曲线需要后续在 HMI 编辑器中创建控件并记录数字 ID 后再启用。

### page2：智能家居控制

用途：展示净化器、新风、加湿器、自动模式等演示功能。

控件：

| 控件名 | 事件 |
| --- | --- |
| `hs_purifier` | `BTN,DEVICE,AIR_PURIFIER,TOGGLE` |
| `hs_fan` | `BTN,DEVICE,FAN,TOGGLE` |
| `hs_humid` | `BTN,DEVICE,HUMIDIFIER,TOGGLE` |
| `hs_auto` | `BTN,MODE,AUTO,TOGGLE` |
| `hs_eco` | `BTN,MODE,ECO,TOGGLE` |
| `hs_back` | `BTN,PAGE,HOME` |

### page3：AI 与设置

用途：展示 AI 状态、串口屏连接状态和后续设置入口。

控件：

| 控件名 | 类型 | 固件更新示例 |
| --- | --- | --- |
| `t_ai_state` | 文本 | `t_ai_state.txt="IDLE"` |
| `t_link_state` | 文本 | `t_link_state.txt="UART2 9600"` |
| `hs_back` | 触摸热区 | `BTN,PAGE,HOME` |

## 滑动切换设计

目标效果：接近手机屏幕左右滑动切换页面。

固件当前支持的事件：

```text
SWIPE,LEFT
SWIPE,RIGHT
BTN,PAGE,NEXT
BTN,PAGE,PREV
```

建议在 HMI 中实现：

1. 每个页面放置全屏透明触摸热区或边缘滑动热区。
2. 左滑时先播放页面向左移动的 HMI 动画或快速切换过渡帧，再发送 `SWIPE,LEFT`。
3. 右滑时先播放页面向右移动的 HMI 动画或快速切换过渡帧，再发送 `SWIPE,RIGHT`。
4. ESP32 收到事件后调用 `page <id>` 切换到下一页或上一页。

如果 USART HMI 编辑器不支持真正的滑动识别，可以用两个透明触摸热区替代：

- 屏幕右侧窄热区发送 `BTN,PAGE,NEXT`。
- 屏幕左侧窄热区发送 `BTN,PAGE,PREV`。

## 串口事件发送格式

TJC 触摸热区事件中建议发送字符串并追加换行：

```text
prints "BTN,PAGE,AIR_DETAIL",0
printh 0a
```

设备控制事件示例：

```text
prints "BTN,DEVICE,AIR_PURIFIER,TOGGLE",0
printh 0a
```

固件的 `SerialHmi::PollEvent()` 同时支持换行和 `0xFF` 作为一条事件结束。

## 调试顺序

1. 在 USART HMI 编辑器中确认页面为 `480x272` 横屏。
2. 导入或复刻 `ui_assets/page*.png` 设计。
3. 创建 `serial_hmi_widgets.json` 中列出的控件，控件名必须完全一致。
4. 给图标和滑动热区配置事件字符串。
5. 下载 HMI 工程到屏幕。
6. 烧录 ESP32 固件，打开 monitor。
7. 确认日志中能看到 `[TJC] page 0`、控件更新命令和 `Screen event` 事件日志。

手动事件配置的完整清单见：

```text
../文档/manual_hmi_event_setup.md
```

## 后续扩展

- 接入真实 PM2.5、CO2、TVOC 传感器后，更新 `SerialHmiAirQualityData` 和 `UpdateAirQuality()`。
- 如果要控制真实外设，在 `CompactWifiBoard::ScreenEventTask()` 中消费 `BTN,DEVICE,...`。
- 最终交付时建议同时提供 `.HMI` 和屏幕可直接下载的编译产物。

