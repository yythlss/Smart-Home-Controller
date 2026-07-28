# 2026-07-28 纯软件稳定性与诊断增强交付

## 范围

本阶段遵循“不更改硬件，只增加代码并更新文档”的要求。没有调整 GPIO、传感器型号、执行器接线、分区表或 `sdkconfig`。

## 已实现

### 并发安全

- `SmartHomeController` 创建 FreeRTOS 递归互斥锁。
- 传感器任务、雷达任务、串口屏任务、HTTP 任务、MCP 回调和舵机任务访问共享状态时使用 `StateGuard`。
- 状态 JSON、历史 JSON 和健康 JSON 在一致快照下生成。

### 传感器和系统健康

- 新增 `GET /api/health`。
- 返回固件版本、运行时间、空闲堆、复位原因、网络连接、HMI 初始化状态和 Wi-Fi RSSI。
- DHT11、MQ135、光敏和雷达返回最近成功时间、数据年龄、连续失败次数或帧统计。
- DHT11 临时失败时最多使用 30 秒缓存；缓存过期后不再作为有效实时温湿度。
- `/api/state` 和 `/api/history` 新增 `sample_time_ms` 与 `cached_temperature_humidity`。

### 控制规则

- 手动控制净化器、新风、加湿器或灯光后，默认对该设备覆盖自动规则 30 分钟。
- `/api/state` 返回各设备 `*_override_remaining_seconds`。
- 重新开启自动模式或节能模式会清除覆盖并立即交回规则控制。
- 自动模式和节能模式保存到 NVS；执行器具体档位不持久化，避免重启后意外动作。
- 无人安全关闭优先级高于手动覆盖。

### 雷达软件能力

- 继续使用现有 LD2450，不增加硬件。
- 从最多三个目标中选取距离最近的目标，输出 X、Y 和速度。
- 根据 X 坐标提供 `left`、`center`、`right` 区域。
- 有目标时立即确认有人；连续 2 分钟没有目标后才确认无人，降低单帧丢失造成的误关机。

### 报警稳定性

- 温度、湿度、空气评分或 MQ135 突变需要连续两个采样周期确认。
- 报警触发后具有 60 秒冷却时间。
- 当前报警仍保持确认后清除的交互方式。

### HTTP、小程序和 MCP

- HTTP API 新增 `/api/health`。
- `config.h` 新增 `SMART_HOME_API_TOKEN` 和 `SMART_HOME_CORS_ORIGIN`。
- Token 非空时支持 `X-API-Key` 和 `Authorization: Bearer <token>`。
- 默认 Token 为空，保持旧局域网演示兼容；默认配置不能直接暴露到公网。
- 微信小程序新增 Token 输入、10 秒自动刷新、诊断面板、历史相对时间和手动覆盖标记。
- Python MCP 桥接支持 `ESP32_API_TOKEN`，新增 `home_get_health`。
- `scripts/test_esp32_http_api.ps1` 支持 `-ApiToken` 并验证 `/api/health`。

### 持续集成

- 新增 `.github/workflows/quality.yml`。
- 每次 push 和 pull request 自动运行 Python 回归、HMI/MCP JSON 校验和小程序 JavaScript 语法检查。
- ESP-IDF 完整固件构建仍作为本地或发布构建执行，避免普通 CI 重复下载大体积组件。

## 主要新增 JSON 字段

`/api/state`：

```text
sample_time_ms
cached_temperature_humidity
radar_nearest_x_mm
radar_nearest_y_mm
radar_nearest_speed_mm_per_s
radar_zone
purifier_override_remaining_seconds
fresh_air_override_remaining_seconds
humidifier_override_remaining_seconds
light_override_remaining_seconds
health
```

`/api/health`：

```text
uptime_ms
free_heap_bytes
reset_reason
firmware_version
network_connected
hmi_initialized
api_auth_enabled
wifi_rssi_dbm
dht_* / mq135_* / ambient_light_* / radar_*
```

## 验证结果

```text
python -m unittest discover -s tests -v
Ran 34 tests
OK

node --check ../mini_program_demo/pages/index/index.js
通过

ESP-IDF 5.5.3 完整构建
Build completed successfully
xiaozhi.bin binary size 0x24bef0 bytes
smallest app partition 0x3f0000 bytes
0x1a4110 bytes (42%) free
```

生成固件：`build/xiaozhi.bin`。

## 烧录后验收

1. 访问 `/api/health`，确认各健康字段存在。
2. 连续观察空闲堆，确认长时间运行没有持续下降。
3. 断开 DHT11 数据线，确认先显示缓存、30 秒后转为过期。
4. 自动模式下手动设置一个设备，确认只有该设备在覆盖期内不被自动规则修改。
5. 重新开启自动模式，确认覆盖剩余时间归零。
6. 移动到雷达左、中、右位置，确认 `radar_zone` 变化。
7. 使用手动环境场景制造突变，确认需要连续两个采样周期才报警。
8. 重启设备，确认自动/节能模式恢复，设备具体档位保持安全初始状态。
9. 小程序确认自动刷新、Token、诊断面板和历史相对时间。

## 仍然存在的边界

- MQ135 仍是演示级原始值和评分映射，没有专业 ppm 标定。
- 历史记录仍保存在 RAM，重启后清空；本阶段只增加了时间戳，没有高频写 Flash。
- 灯光仍为逻辑输出回调，没有绑定新的 GPIO，符合本阶段不改硬件的要求。
- Token 是编译期可选配置，不是完整的用户账号或云端设备证书体系。
- 雷达区域使用固定 X 坐标阈值，现场可根据摆放位置继续软件标定。
