# 小程序局域网演示工程验证手册

这是“方案 A”的微信小程序最小演示版，用局域网 HTTP 直接访问 ESP32S3。它不依赖云服务器，适合答辩、课堂演示和同一 WiFi 下的功能联调。

工程目录：

```text
<仓库根目录>/mini_program_demo
```

固件端依赖：

```text
E:/espwork/xiaozhi-esp32/xiaozhi-esp32/main/boards/bread-compact-wifi/smart_home_http_server.cc
```

## 1. 验证前准备

先确认以下条件都满足，再打开微信开发者工具：

```text
[ ] ESP32S3 已烧录当前 bread-compact-wifi 固件
[ ] ESP32S3 已经能连接现场 WiFi
[ ] 串口 monitor 中没有反复重启
[ ] 串口 monitor 中没有持续出现 Brownout detector was triggered
[ ] 电脑与 ESP32S3 在同一个局域网
[ ] 如需真机预览，手机也与 ESP32S3 在同一个局域网
[ ] 已安装微信开发者工具
```

注意：HTTP API 只有在 WiFi 连接成功后才会启动，不是刚上电立刻启动。

串口 monitor 中需要看到：

```text
Network connected, starting mini program HTTP API
Mini program HTTP API started on port 8080
```

如果看不到第二行，先不要验证小程序，先检查 WiFi 是否连上、设备是否重启、是否还停留在配网流程。

## 2. 获取 ESP32S3 局域网 IP

小程序首页需要填写的是：

```text
<ESP32_IP>:8080
```

例如：

```text
192.168.1.23:8080
```

不要只填 IP，也不要填配网页面的 `80` 端口。

获取 IP 的常用方式：

1. 查看 ESP32 串口 monitor 的联网日志。
2. 查看路由器后台的已连接设备列表。
3. 如果使用手机热点，查看热点的已连接设备信息。

记录给队友时建议写成：

```text
本次验证 ESP32S3 IP：192.168.1.23
小程序填写地址：192.168.1.23:8080
电脑 IP：192.168.1.xx
手机 IP：192.168.1.xx
```

## 3. 先验证 HTTP API

小程序只是 HTTP API 的前端。必须先确认 API 可访问，再判断小程序问题。

### 3.1 浏览器验证当前状态

在电脑浏览器打开：

```text
http://<ESP32_IP>:8080/api/state
```

通过标准：

```text
[ ] 浏览器能打开页面
[ ] 返回内容是 JSON
[ ] JSON 中有 purifier_level
[ ] JSON 中有 fresh_air_level
[ ] JSON 中有 humidifier_level
[ ] JSON 中有 auto_mode
[ ] JSON 中有 eco_mode
[ ] JSON 中有 air_score
```

如果浏览器打不开，小程序也一定打不开。优先检查 IP、端口、WiFi、路由器 AP 隔离和 ESP32 是否重启。

### 3.2 PowerShell 验证状态接口

在电脑 PowerShell 中执行：

```powershell
Invoke-RestMethod -Method Get -Uri "http://<ESP32_IP>:8080/api/state"
```

预期能打印当前状态对象。重点看：

```text
purifier_level    净化器档位，0-3
fresh_air_level   新风/风扇档位，0-3
humidifier_level  加湿器档位，0-3
auto_mode         自动模式 true/false
eco_mode          节能模式 true/false
temperature_c     温度
humidity_percent  湿度
mq135_raw         MQ135 原始 ADC 值
air_score         空气质量评分
air_state         空气等级，例如 优/良/差
comfort           环境舒适度描述
advice            当前环境建议
manual_environment_mode  是否使用手动模拟环境
environment_source       sensor 或 manual
```

### 3.3 PowerShell 验证历史接口

等待 ESP32 运行至少 10 秒后执行：

```powershell
Invoke-RestMethod -Method Get -Uri "http://<ESP32_IP>:8080/api/history"
```

通过标准：

```text
[ ] 返回 JSON
[ ] capacity 为 30
[ ] samples 是数组
[ ] 等待 5 秒后再次请求，samples 中有新采样或最新采样内容更新
```

如果 `samples` 一直为空：

1. 等待至少一个 5 秒传感器采样周期。
2. 确认固件没有反复重启。
3. 确认传感器任务正常运行。

### 3.4 PowerShell 验证设备控制

净化器二档：

```powershell
Invoke-RestMethod -Method Post -Uri "http://<ESP32_IP>:8080/api/device" -ContentType "application/json" -Body '{"device":"purifier","power":true,"level":2}'
```

通过标准：

```text
[ ] 返回 JSON
[ ] purifier_level 变为 2
[ ] GPIO13 红色 LED 亮度变化
```

加湿器三档：

```powershell
Invoke-RestMethod -Method Post -Uri "http://<ESP32_IP>:8080/api/device" -ContentType "application/json" -Body '{"device":"humidifier","power":true,"level":3}'
```

通过标准：

```text
[ ] 返回 JSON
[ ] humidifier_level 变为 3
[ ] GPIO14 蓝色 LED 亮度变化
```

新风/风扇二档：

```powershell
Invoke-RestMethod -Method Post -Uri "http://<ESP32_IP>:8080/api/device" -ContentType "application/json" -Body '{"device":"fan","power":true,"level":2}'
```

通过标准：

```text
[ ] 返回 JSON
[ ] fresh_air_level 变为 2
[ ] GPIO21 连续旋转舵机风扇中速旋转
```

关闭三个设备：

```powershell
Invoke-RestMethod -Method Post -Uri "http://<ESP32_IP>:8080/api/device" -ContentType "application/json" -Body '{"device":"purifier","power":false,"level":0}'
Invoke-RestMethod -Method Post -Uri "http://<ESP32_IP>:8080/api/device" -ContentType "application/json" -Body '{"device":"humidifier","power":false,"level":0}'
Invoke-RestMethod -Method Post -Uri "http://<ESP32_IP>:8080/api/device" -ContentType "application/json" -Body '{"device":"fan","power":false,"level":0}'
```

通过标准：

```text
[ ] purifier_level 变为 0
[ ] humidifier_level 变为 0
[ ] fresh_air_level 变为 0
[ ] 两个 LED 关闭
[ ] 连续旋转舵机风扇停止或接近停止
```

如果 JSON 状态变化但外设不动作，说明小程序和 HTTP API 链路基本正常，下一步查 GPIO 接线、供电、共地、LED 极性或舵机控制脉宽。

### 3.5 PowerShell 验证模式控制

开启自动模式：

```powershell
Invoke-RestMethod -Method Post -Uri "http://<ESP32_IP>:8080/api/mode" -ContentType "application/json" -Body '{"mode":"auto","power":true}'
```

通过标准：

```text
[ ] 返回 JSON
[ ] auto_mode 变为 true
[ ] 等待至少 5 秒后，控制器会根据传感器状态调整设备档位
```

关闭自动模式：

```powershell
Invoke-RestMethod -Method Post -Uri "http://<ESP32_IP>:8080/api/mode" -ContentType "application/json" -Body '{"mode":"auto","power":false}'
```

开启节能模式：

```powershell
Invoke-RestMethod -Method Post -Uri "http://<ESP32_IP>:8080/api/mode" -ContentType "application/json" -Body '{"mode":"eco","power":true}'
```

通过标准：

```text
[ ] eco_mode 变为 true
[ ] auto_mode 变为 false
[ ] purifier_level / fresh_air_level / humidifier_level 都变为 0
[ ] 外设关闭
```

关闭节能模式：

```powershell
Invoke-RestMethod -Method Post -Uri "http://<ESP32_IP>:8080/api/mode" -ContentType "application/json" -Body '{"mode":"eco","power":false}'
```

### 3.6 PowerShell 验证手动环境输入接口

这个接口用于测试自动模式和舒适度/建议显示。真实环境不方便制造高温、干燥或污染时，用它模拟数据。

手动输入一组污染环境：

```powershell
Invoke-RestMethod -Method Post -Uri "http://<ESP32_IP>:8080/api/environment" -ContentType "application/json" -Body '{"enabled":true,"temperature_c":30,"humidity_percent":55,"air_score":30}'
```

通过标准：

```text
[ ] 返回 JSON
[ ] manual_environment_mode 变为 true
[ ] environment_source 变为 manual
[ ] air_score 变为 30 左右
[ ] comfort 显示空气差/污染相关描述
[ ] advice 显示开净化器、新风或保持通风相关建议
```

使用预设场景：

```powershell
Invoke-RestMethod -Method Post -Uri "http://<ESP32_IP>:8080/api/environment" -ContentType "application/json" -Body '{"enabled":true,"preset":"GOOD"}'
Invoke-RestMethod -Method Post -Uri "http://<ESP32_IP>:8080/api/environment" -ContentType "application/json" -Body '{"enabled":true,"preset":"HOT"}'
Invoke-RestMethod -Method Post -Uri "http://<ESP32_IP>:8080/api/environment" -ContentType "application/json" -Body '{"enabled":true,"preset":"DRY"}'
Invoke-RestMethod -Method Post -Uri "http://<ESP32_IP>:8080/api/environment" -ContentType "application/json" -Body '{"enabled":true,"preset":"POLLUTED"}'
```

预期：

```text
GOOD      舒适环境，建议环境舒适或保持通风
HOT       高温环境，建议开空调降温
DRY       干燥环境，建议开加湿器
POLLUTED  空气差环境，建议开净化器和新风
```

退出手动环境，恢复真实传感器：

```powershell
Invoke-RestMethod -Method Post -Uri "http://<ESP32_IP>:8080/api/environment" -ContentType "application/json" -Body '{"enabled":false}'
```

通过标准：

```text
[ ] manual_environment_mode 变为 false
[ ] 下一个 5 秒采样周期后，temperature_c / humidity_percent / air_score 回到真实传感器读数
```

如果开启自动模式后再发送手动环境数据，等待一个采样/刷新周期，设备档位会按模拟环境调整。比如 `POLLUTED` 应倾向打开净化器和新风，`DRY` 应倾向打开加湿器。

## 4. 导入微信小程序工程

打开微信开发者工具后按以下步骤操作：

1. 选择“导入项目”。
2. 项目目录选择：

```text
<仓库根目录>/mini_program_demo
```

3. AppID 选择测试号、游客模式或你自己的测试 AppID。
4. 项目名称可填：

```text
空气管家局域网演示
```

仓库内的 `project.config.json` 默认使用 `touristappid`，可直接以游客模式编译运行；准备真机预览或上传时，再在微信开发者工具中换成你自己的 AppID。

5. 导入后确认左侧文件包含：

```text
app.json
pages/index/index.js
pages/index/index.json
pages/index/index.wxml
pages/index/index.wxss
```

6. 打开右上角“详情”或“本地设置”。
7. 勾选或确认已启用“不校验合法域名、web-view 域名、TLS 版本以及 HTTPS 证书”。

不同版本微信开发者工具入口名称略有差异，目标就是关闭合法域名校验。否则本地 `http://192.168.x.x:8080` 请求会被拦截。

## 5. 开发者工具模拟器验证

先用模拟器验证，不要一开始就用手机真机。

操作步骤：

1. 确认 ESP32S3 已联网，monitor 出现 `Mini program HTTP API started on port 8080`。
2. 确认电脑浏览器能打开：

```text
http://<ESP32_IP>:8080/api/state
```

3. 在小程序首页的输入框填写：

```text
<ESP32_IP>:8080
```

示例：

```text
192.168.1.23:8080
```

4. 点击“连接”。
5. 观察页面顶部提示。

通过标准：

```text
[ ] 页面提示“数据已刷新”或类似成功信息
[ ] 环境状态区域显示温度、湿度、空气评分
[ ] 模式区域显示“自动”“节能”按钮
[ ] 设备控制区域显示“净化”“新风”“加湿”
[ ] 手动输入数据区域显示温度、湿度、空气评分输入框
[ ] 环境状态区域显示舒适度、环境建议、数据来源
[ ] 近期空气质量区域显示评分条，或在刚启动时显示“暂无历史数据”
```

如果提示 `连接失败`：

1. 复制小程序里填写的地址，确认没有多余空格。
2. 确认填写的是 `192.168.x.x:8080`，不是 `http://192.168.x.x:8080/api/state`。
3. 再用浏览器打开 `/api/state`。
4. 打开微信开发者工具调试器，查看 Network 或 Console 中的错误。
5. 确认已关闭合法域名校验。

## 6. 小程序功能逐项验证

每验证一项，都建议同步观察三处：

```text
1. 小程序页面是否变化
2. /api/state 返回值是否变化
3. 实物外设或串口 monitor 是否变化
```

### 6.1 状态读取

操作：

1. 点击“刷新数据”。
2. 查看“环境状态”区域。

通过标准：

```text
[ ] 温度位置显示数值，或没有 DHT11 有效值时显示 --
[ ] 湿度位置显示数值，或没有 DHT11 有效值时显示 --
[ ] 空气评分显示 0-100 范围内数值
[ ] 页面没有一直停留在 loading 状态
```

### 6.2 历史数据读取

操作：

1. ESP32S3 上电后等待 15-30 秒。
2. 点击“刷新数据”。
3. 查看“近期空气质量”区域。

通过标准：

```text
[ ] 出现一条或多条空气评分条
[ ] 等待 5 秒后刷新，评分条数量或最后一条内容更新
[ ] 评分条宽度随 air_score 变化
```

如果一直显示“暂无历史数据”，先回到 PowerShell 验证 `/api/history`。

### 6.3 净化器控制

操作：

1. 在“净化”一行点击 `1`。
2. 点击 `2`。
3. 点击 `3`。
4. 点击 `0`。

通过标准：

```text
[ ] 小程序中净化对应档位按钮高亮切换
[ ] /api/state 中 purifier_level 依次变为 1、2、3、0
[ ] GPIO13 红色 LED 亮度随档位变化，0 档关闭
```

### 6.4 新风/风扇控制

操作：

1. 在“新风”一行点击 `1`。
2. 点击 `2`。
3. 点击 `3`。
4. 点击 `0`。

通过标准：

```text
[ ] 小程序中新风对应档位按钮高亮切换
[ ] /api/state 中 fresh_air_level 依次变为 1、2、3、0
[ ] GPIO21 连续旋转舵机风扇按低速、中速、高速、停止变化
```

如果 0 档仍然缓慢旋转，记录现象，后续只需要微调固件中的 `FreshAirPulseUsForLevel(0)` 停止脉宽。

### 6.5 加湿器控制

操作：

1. 在“加湿”一行点击 `1`。
2. 点击 `2`。
3. 点击 `3`。
4. 点击 `0`。

通过标准：

```text
[ ] 小程序中加湿对应档位按钮高亮切换
[ ] /api/state 中 humidifier_level 依次变为 1、2、3、0
[ ] GPIO14 蓝色 LED 亮度随档位变化，0 档关闭
```

### 6.6 自动模式

操作：

1. 点击“自动”。
2. 访问 `/api/state`。
3. 等待至少 5 秒。
4. 再次访问 `/api/state`。
5. 再点击“自动”关闭。

通过标准：

```text
[ ] 点击后 auto_mode 变为 true
[ ] “自动”按钮显示激活状态
[ ] 节能模式不是开启状态
[ ] 等待采样周期后，设备档位可能根据传感器读数自动变化
[ ] 再次点击后 auto_mode 变为 false
```

说明：自动模式是否立刻改变外设，取决于 DHT11/MQ135 当前读数和阈值。只要 `auto_mode` 状态正确、传感器有效、采样周期正常，就说明小程序控制链路有效。

### 6.7 节能模式

操作：

1. 先把净化、新风、加湿任意设置到非 0 档。
2. 点击“节能”。
3. 访问 `/api/state`。
4. 再点击“节能”关闭。

通过标准：

```text
[ ] eco_mode 变为 true
[ ] auto_mode 变为 false
[ ] purifier_level 变为 0
[ ] fresh_air_level 变为 0
[ ] humidifier_level 变为 0
[ ] 外设全部关闭
[ ] 再次点击后 eco_mode 变为 false
```

### 6.8 手动输入数据

操作：

1. 在“手动输入数据”区域填入：

```text
温度 C：30
湿度 %：55
空气评分：30
```

2. 点击“应用手动数据”。
3. 查看“环境状态”区域。
4. 点击“刷新数据”。

通过标准：

```text
[ ] 页面提示“手动输入数据已生效”
[ ] 数据来源显示“手动模拟”
[ ] 温度、湿度、空气评分变为刚才填写的值
[ ] 舒适度显示污染/空气差等描述
[ ] 环境建议显示开净化器、新风或通风相关内容
[ ] /api/state 中 manual_environment_mode 为 true
```

### 6.9 手动预设场景

操作：

1. 点击“舒适”。
2. 点击“高温”。
3. 点击“干燥”。
4. 点击“污染”。

通过标准：

```text
[ ] 点击“舒适”后 comfort 倾向显示舒适，advice 倾向显示环境舒适或保持通风
[ ] 点击“高温”后 advice 出现开空调降温相关建议
[ ] 点击“干燥”后 advice 出现开加湿器相关建议
[ ] 点击“污染”后 advice 出现开净化器/新风相关建议
[ ] 每次点击后 /api/state 的 manual_environment_mode 都为 true
```

### 6.10 恢复真实传感器

操作：

1. 点击“恢复传感器”。
2. 等待至少一个 5 秒采样周期。
3. 点击“刷新数据”。

通过标准：

```text
[ ] 页面提示“已恢复真实传感器数据”
[ ] /api/state 中 manual_environment_mode 为 false
[ ] 数据来源显示“真实传感器”
[ ] 温湿度和空气评分恢复为实际传感器更新值
```

### 6.11 手动环境联动自动模式

操作：

1. 点击“自动”，确认自动模式开启。
2. 点击“污染”预设。
3. 等待 5 秒并点击“刷新数据”。
4. 再点击“干燥”预设。
5. 等待 5 秒并点击“刷新数据”。

通过标准：

```text
[ ] 污染预设下，净化器和新风/风扇档位应自动升高
[ ] 干燥预设下，加湿器档位应自动升高
[ ] 串口屏首页和空气详情页同步显示新的舒适度、建议和空气评分
```

## 7. 手机真机预览验证

模拟器验证通过后，再做真机预览。

操作步骤：

1. 手机连接和 ESP32S3 同一个 WiFi。
2. 如果用电脑热点，确认手机和 ESP32S3 都连到这个热点。
3. 微信开发者工具点击“预览”生成二维码。
4. 手机微信扫码打开小程序。
5. 在手机小程序中填写：

```text
<ESP32_IP>:8080
```

6. 点击“连接”。
7. 重复第 6 节中的状态读取、历史读取、三个设备控制、自动模式、节能模式验证。

真机通过标准：

```text
[ ] 手机小程序能刷新状态
[ ] 手机小程序能显示近期空气评分条
[ ] 手机小程序能控制净化、新风/风扇、加湿
[ ] 手机小程序能开启/关闭自动模式
[ ] 手机小程序能开启/关闭节能模式
```

如果模拟器可以、手机不可以，优先检查：

1. 手机和 ESP32S3 是否同一 WiFi。
2. 路由器是否开启 AP 隔离、访客网络隔离或无线客户端隔离。
3. 手机是否使用了移动数据/VPN/代理。
4. 微信真机对本地 HTTP 的限制；答辩演示可优先使用开发者工具模拟器。

## 8. 交付给队友的记录模板

完成小程序验证后，把下面内容填入阶段交付文档或群内记录：

```text
验证日期：
固件版本/提交：
ESP32S3 IP：
小程序填写地址：
电脑是否同网段：
手机是否同网段：

HTTP API：
[ ] /api/state 通过
[ ] /api/history 通过
[ ] /api/device 通过
[ ] /api/mode 通过
[ ] /api/environment 通过

微信开发者工具模拟器：
[ ] 状态读取通过
[ ] 历史数据通过
[ ] 净化控制通过
[ ] 新风/风扇控制通过
[ ] 加湿控制通过
[ ] 自动模式通过
[ ] 节能模式通过
[ ] 手动输入数据通过
[ ] 手动预设场景通过
[ ] 恢复真实传感器通过

手机真机预览：
[ ] 状态读取通过
[ ] 历史数据通过
[ ] 净化控制通过
[ ] 新风/风扇控制通过
[ ] 加湿控制通过
[ ] 自动模式通过
[ ] 节能模式通过
[ ] 手动输入数据通过
[ ] 手动预设场景通过
[ ] 恢复真实传感器通过
[ ] 未验证，原因：

异常现象：
处理结论：
下一步：
```

## 9. 常见问题

### 9.1 页面提示连接失败

按顺序检查：

1. 小程序填写的是 `<ESP32_IP>:8080`。
2. ESP32 monitor 出现 `Mini program HTTP API started on port 8080`。
3. 电脑浏览器能打开 `http://<ESP32_IP>:8080/api/state`。
4. 微信开发者工具关闭合法域名校验。
5. 电脑和 ESP32S3 在同一个局域网。
6. 路由器没有开启 AP 隔离。

### 9.2 API 能打开，小程序不能打开

优先看微信开发者工具调试器：

```text
Console：看 JS 报错
Network：看请求 URL、状态码和失败原因
```

常见原因：

1. 合法域名校验未关闭。
2. 小程序里多填了 `http://` 以外的路径，例如把 `/api/state` 也填进输入框。
3. IP 发生变化，路由器重新分配了 ESP32S3 地址。
4. 手机真机与 ESP32S3 不在同一网段。

### 9.3 小程序状态变化，外设不动作

这通常不是小程序问题。按顺序排查：

1. 访问 `/api/state`，确认档位是否变化。
2. 看 monitor 是否有 `SmartHome: Apply ... output` 日志。
3. 检查 LED 极性和限流电阻。
4. 检查舵机风扇 5V 独立供电和 ESP32 共地。
5. 如果出现 `Brownout detector was triggered`，先处理供电，暂停功能判断。

### 9.4 自动模式看起来没反应

自动模式按传感器采样周期工作，不是点击后必然立即改变外设。

检查：

1. `/api/state` 中 `auto_mode` 是否为 `true`。
2. `/api/state` 中 `eco_mode` 是否为 `false`。
3. 温度、湿度、MQ135 是否有有效读数。
4. 是否已经等待至少一个 5 秒采样周期。

### 9.5 真机预览失败但模拟器正常

优先用模拟器作为答辩演示方案，同时记录真机失败原因。真机常见限制来自网络隔离或微信真机环境对局域网 HTTP 的限制，不代表固件 HTTP API 一定有问题。

## 10. 当前能力边界

- 当前是局域网演示方案，没有公网服务器。
- 固件默认未启用 HTTP Token，以兼容可信 WiFi 内的现有演示；可在 `config.h` 设置 `SMART_HOME_API_TOKEN` 后启用。
- 当前小程序直接请求 `http://<ESP32_IP>:8080`，不适合直接发布上线。
- 空气质量传感器当前只有 MQ135 原始值和演示级空气评分，没有 PM2.5、CO2、TVOC 具体浓度。
- 历史数据最多保留 30 条，约等于最近 2.5 分钟采样窗口。

## 11. 诊断、自动刷新与可选鉴权

新版小程序会同时读取：

```text
GET /api/state
GET /api/history
GET /api/health
GET /api/events
```

页面每 10 秒自动刷新一次，并展示固件版本、运行时间、空闲内存、Wi-Fi RSSI、DHT11/MQ135/光敏数据新鲜度、雷达有效帧数和 API 鉴权状态。历史条目使用固件启动后的 `sample_time_ms` 显示相对时间。

如果固件的 `SMART_HOME_API_TOKEN` 非空，在首页的“可选 API Token”输入框填写同一字符串并点击“连接”。小程序会把 Token 保存到本机存储，并通过 `X-API-Key` 请求头发送。不要在截图、公开仓库或演示视频中暴露真实 Token。

PowerShell 验证示例：

```powershell
$headers = @{ "X-API-Key" = "你的本地Token" }
Invoke-RestMethod -Headers $headers -Uri "http://<ESP32_IP>:8080/api/health"

powershell -ExecutionPolicy Bypass -File xiaozhi-esp32\scripts\test_esp32_http_api.ps1 `
  -Esp32BaseUrl "http://<ESP32_IP>:8080" `
  -ApiToken "你的本地Token"
```

手动控制设备后，页面会显示“手动”标记；该设备默认在 30 分钟内不被自动模式覆盖。重新开启自动模式或节能模式，会立即交回规则控制。

## 12. 演示增强功能

当前小程序新增以下纯软件功能，不需要增加或更换硬件：

- 雷达二维目标位置图：把 LD2450 最近目标的 X/Y 坐标映射到左、中、右区域。
- 环境趋势曲线：同时绘制最近 30 条空气评分、温度和湿度数据。
- 自定义自动化：可配置空气评分、湿度、温度阈值，以及净化器、新风、加湿器动作档位。
- 一键场景：回家、离家、睡眠、通风和强力净化。
- 告警与操作日志：显示最近 32 条系统、雷达、设备、场景、告警和规则事件。
- 离线演示模式：无需 ESP32，在小程序本地模拟雷达移动、环境变化、设备联动和历史数据。

新增接口：

| 方法 | 路径 | 说明 |
| --- | --- | --- |
| GET | `/api/events` | 获取最近 32 条事件 |
| POST | `/api/automation` | 保存自动化规则 |
| POST | `/api/scene` | 应用一键场景 |

自动化请求示例：

```json
{
  "enabled": true,
  "air_score_below": 60,
  "humidity_below": 35,
  "temperature_above": 30,
  "purifier_level": 3,
  "fresh_air_level": 2,
  "humidifier_level": 2
}
```

一键场景的 `scene` 可取：`HOME`、`AWAY`、`SLEEP`、`VENTILATE`、`CLEAN`。

### 12.1 推荐演示流程

1. 打开“离线演示”，确认页面无需网络即可出现环境数据、雷达目标和趋势曲线。
2. 点击“回家”，展示灯光和环境设备联动。
3. 开启自动模式并保存自动化规则。
4. 点击“污染”环境预设，展示净化器、新风自动升档以及规则执行日志。
5. 点击“强力净化”，展示一键场景切换。
6. 查看“告警与操作日志”，说明所有关键动作都可追溯。
7. 关闭离线演示，连接真实 ESP32，展示同一页面无缝切换到真实数据。

离线演示模式只影响小程序本地数据，不会向 ESP32 发送控制请求；关闭开关后立即恢复真实局域网 API。
