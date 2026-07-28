# TJC / USART HMI 屏幕工程

本目录保存 `bread-compact-wifi` 使用的 TJC 4.3 英寸串口屏工程源码。`.HMI` 是 USART HMI 编辑器的二进制工程格式，应使用对应编辑器打开、保存、编译并下载到屏幕，不要使用文本编辑器直接修改。

## 文件说明

| 文件 | 用途 | SHA-256 |
| --- | --- | --- |
| `serial_warm_home.HMI` | 从原始屏幕工程生成的可编辑源码副本，作为当前版本控制基线 | `892173DB649541701A2F48470BAEEC5CC39CADA97602598FFFB23174A78439E6` |
| `serial_warm_home_page1_background_test.HMI` | 在基线副本上替换 page1 背景资源的测试版本，使用前需在编辑器和实机中确认 | `C6050895CDFE0FAFD1E690CD92A071A04118422EE7B8A78886F56A71E8DDE988` |

## 当前契约

- 屏幕分辨率：`480 × 272`。
- 串口：UART2，`9600 8N1`。
- ESP32 TX GPIO41 → 屏幕 RX。
- ESP32 RX GPIO42 ← 屏幕 TX。
- 页面、控件和触摸事件契约见 [`serial_hmi_widgets.json`](../main/boards/bread-compact-wifi/serial_hmi_widgets.json)。
- 固件端实现见 [`serial_hmi.cc`](../main/boards/bread-compact-wifi/serial_hmi.cc)。

修改屏幕工程后，应同步检查控件名称、数字 ID、事件字符串和固件协议，并同时提交新的 `.HMI` 文件与相关 JSON/文档变更。
