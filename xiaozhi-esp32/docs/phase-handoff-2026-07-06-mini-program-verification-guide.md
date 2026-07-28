# 2026-07-06 小程序验证步骤细化交付文档

## 当前工程状态

- 工程目录：`E:/espwork/xiaozhi-esp32/xiaozhi-esp32`
- 当前板型：`main/boards/bread-compact-wifi`
- 小程序演示工程：`docs/mini_program_demo`
- 小程序后端：ESP32S3 联网后启动 `SmartHomeHttpServer`，监听 `8080` 端口。
- 当前 HTTP API 入口：
  - `GET /api/state`
  - `GET /api/history`
  - `POST /api/device`
  - `POST /api/mode`

本阶段只补充验证文档，没有修改固件源码、HMI 工程、受保护配置或构建配置。

## 本阶段改动文件

| 文件 | 改动 |
| --- | --- |
| `docs/mini_program_demo/README.md` | 从简版说明扩展为完整小程序局域网验证手册，包含准备、IP 获取、HTTP API 预验证、微信开发者工具导入、模拟器验证、真机预览、故障排查和队友记录模板。 |
| `../文档/智能家居外设接线与验证步骤.md` | 扩展第 13 节“小程序演示工程验证”，把小程序验证嵌入整机验证流程。 |
| `docs/current-project-handoff.md` | 在有效文档列表和下一阶段目标中补充小程序详细验证手册。 |
| `docs/continuation-notes.md` | 在接续阅读顺序中补充小程序验证交付文档和小程序 README。 |
| `docs/project-file-map.md` | 在重要协作文档和接续阅读顺序中补充本阶段文档。 |
| `../文档/串口屏与环境监测项目交付说明.md` | 补充小程序详细验证文档入口，提醒队友先验证 HTTP API 再验证小程序。 |
| `progress.md` | 追加本阶段进度记录。 |

## 小程序验证核心顺序

后续人工验证请按这个顺序执行：

1. 烧录当前 ESP32S3 固件。
2. 打开串口 monitor，确认设备没有反复重启。
3. 等 WiFi 连接成功。
4. 确认 monitor 出现：

```text
Network connected, starting mini program HTTP API
Mini program HTTP API started on port 8080
```

5. 从串口日志、路由器后台或手机热点设备列表获取 ESP32S3 IP。
6. 先在电脑浏览器访问：

```text
http://<ESP32_IP>:8080/api/state
```

7. 再用 PowerShell 验证：

```powershell
Invoke-RestMethod -Method Get -Uri "http://<ESP32_IP>:8080/api/state"
Invoke-RestMethod -Method Get -Uri "http://<ESP32_IP>:8080/api/history"
```

8. 用 PowerShell 验证 `/api/device` 和 `/api/mode` 控制链路。
9. 导入微信开发者工具，关闭合法域名校验。
10. 先用开发者工具模拟器填写 `<ESP32_IP>:8080` 并点击“连接”。
11. 模拟器验证状态读取、历史数据、净化、新风/风扇、加湿、自动、节能。
12. 模拟器通过后再做手机真机预览。

## 关键判定标准

HTTP API 通过标准：

```text
[ ] /api/state 返回 JSON
[ ] /api/history 返回 samples
[ ] /api/device 能改变 purifier_level / fresh_air_level / humidifier_level
[ ] /api/mode 能改变 auto_mode / eco_mode
```

微信开发者工具模拟器通过标准：

```text
[ ] 页面能提示数据已刷新
[ ] 环境状态显示温度、湿度、空气评分
[ ] 近期空气质量显示评分条或暂无历史数据
[ ] 净化 0-3 档能控制 GPIO13 红色 LED
[ ] 新风 0-3 档能控制 GPIO21 连续旋转舵机风扇
[ ] 加湿 0-3 档能控制 GPIO14 蓝色 LED
[ ] 自动模式能开关
[ ] 节能模式能关闭自动模式和所有执行器
```

手机真机通过标准：

```text
[ ] 手机与 ESP32S3 同 WiFi
[ ] 手机小程序能连接 `<ESP32_IP>:8080`
[ ] 手机小程序能刷新状态和历史
[ ] 手机小程序能控制三类设备和两种模式
```

## 常见问题判断

- 浏览器打不开 `/api/state`：先查 WiFi、IP、端口、HTTP API 是否启动，不要先查小程序。
- PowerShell API 能用但小程序不能用：优先查微信开发者工具合法域名校验、输入框内容和调试器 Network 报错。
- 模拟器能用但手机不能用：优先查手机是否同 WiFi、是否用了移动数据/VPN/代理、路由器是否开启 AP 隔离。
- 小程序状态变化但外设不动作：优先查 `/api/state`、`SmartHome: Apply ... output` 日志、GPIO 接线、LED 极性、舵机 5V 供电和共地。
- 出现 `Brownout detector was triggered`：先处理供电，暂停小程序功能判断。

## 验证结果

本阶段为文档补充，未改固件源码，因此未重新运行 ESP-IDF 构建。

已做的静态验证：

```text
[x] 新增/更新文档中包含小程序验证入口
[x] 小程序 README 覆盖 HTTP API、模拟器、真机、故障排查
[x] 总验证手册第 13 节已细化
[x] 受保护配置未被修改
```

验证命令摘要：

```powershell
Select-String -LiteralPath docs/mini_program_demo/README.md -Pattern "/api/state|/api/history|/api/device|/api/mode|开发者工具模拟器验证|手机真机预览|交付给队友|Brownout"
Select-String -LiteralPath ../文档/智能家居外设接线与验证步骤.md -Pattern "13.1 小程序验证前置条件|13.4 HTTP API 控制预验证|13.6 开发者工具模拟器验证|13.8 手机真机预览验证|13.9 小程序验证记录模板"
git status --short -- .vscode sdkconfig sdkconfig.defaults sdkconfig.defaults.esp32 sdkconfig.defaults.esp32s3 CMakePresets.json
```

其中受保护配置检查命令无输出，表示本阶段没有改动 `.vscode/**`、`sdkconfig*` 或 `CMakePresets.json`。

## 仍需人工完成

1. 重新烧录当前 ESP32S3 固件。
2. 按 `docs/mini_program_demo/README.md` 完整执行小程序验证。
3. 把真实 ESP32S3 IP、电脑 IP、手机 IP、模拟器结果、真机结果记录下来。
4. 如果手机真机失败但模拟器成功，记录路由器型号、网络结构和开发者工具报错。
5. 如果外设状态变化但硬件不动作，按 `智能家居外设接线与验证步骤.md` 的接线与供电排查继续处理。

## 下一阶段目标

- 完成小程序模拟器实测。
- 有条件时完成手机真机预览实测。
- 把实测结果回填到本交付文档或新的阶段交付文档。
- 若答辩需要更直观展示，可继续把小程序历史区域升级为温度、湿度、空气评分三条趋势图。
