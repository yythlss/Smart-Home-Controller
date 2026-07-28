# 阶段交付：HMI 后台脚本探测

日期：2026-06-10

## 本阶段目标

用户要求先不继续尝试 GUI 点击方式，改为测试能否用后台脚本辅助 USART HMI 工程处理。本阶段只在 `.HMI` 副本上做验证，不修改原始 HMI 工程。

## 当前工程状态

| 项目 | 当前值 |
| --- | --- |
| ESP-IDF 工程根目录 | `E:/espwork/xiaozhi-esp32/xiaozhi-esp32` |
| 当前板型目录 | `main/boards/bread-compact-wifi` |
| 当前 HMI 原文件 | `D:/QQ/serial_warm_home .HMI` |
| USART HMI 编辑器 | `E:/USART_HMI/USART HMI.exe` |
| 原始 HMI SHA256 | `892173DB649541701A2F48470BAEEC5CC39CADA97602598FFFB23174A78439E6` |

注意：真实 HMI 文件名在 `.HMI` 前有一个空格，路径是 `D:/QQ/serial_warm_home .HMI`。旧文档中部分无空格写法已同步修正。

## 本阶段新增文件

| 文件 | 作用 |
| --- | --- |
| `scripts/hmi_background_probe.ps1` | 后台脚本：扫描 HMI 内嵌 PNG、生成轻量 page1 背景、可选写入 HMI 测试副本 |
| `main/boards/bread-compact-wifi/ui_assets/page1_air_detail_hmi_blank_light.png` | 为嵌入 HMI 旧资源槽位生成的轻量 page1 背景 |
| `tmp_hmi_probe_page1_bg_replace.HMI` | 由脚本生成的测试 HMI 副本，已替换 page1 背景资源槽位 |

`tmp_hmi_probe_page1_bg_replace.HMI` 是临时验证产物，不建议作为正式交付文件直接覆盖原工程。正式使用前必须用 USART HMI 编辑器打开副本、检查 page1 显示、保存/编译/下载验证。

## 脚本验证结果

运行命令：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File E:/espwork/xiaozhi-esp32/xiaozhi-esp32/scripts/hmi_background_probe.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File E:/espwork/xiaozhi-esp32/xiaozhi-esp32/scripts/hmi_background_probe.ps1 -PatchCopy
```

检测到的 HMI 内嵌 PNG 资源：

| Index | Offset | Length | Size |
| --- | --- | --- | --- |
| 0 | `0x007A855D` | `5148` | `480x272` |
| 1 | `0x007B2835` | `4335` | `480x272` |
| 2 | `0x007C0F8B` | `981` | `96x96` |
| 3 | `0x007C25BF` | `998` | `96x96` |
| 4 | `0x007C3C04` | `978` | `96x96` |
| 5 | `0x007C5235` | `1026` | `96x96` |
| 6 | `0x007C6896` | `9566` | `480x272` |
| 7 | `0x007D3D6B` | `7145` | `480x272` |
| 8 | `0x007DF6D4` | `8714` | `480x272` |
| 9 | `0x007EBF25` | `5860` | `480x272` |

本阶段把 `Index 7` 作为 page1 旧背景槽位进行副本替换验证：

| 项目 | 结果 |
| --- | --- |
| 目标槽位 | `Index 7` / `0x007D3D6B` |
| 槽位长度 | `7145` 字节 |
| 轻量 PNG | `page1_air_detail_hmi_blank_light.png` |
| 轻量 PNG 尺寸 | `480x272` |
| 轻量 PNG 长度 | `5319` 字节 |
| 轻量 PNG SHA256 | `B60AED76F8B85F9E196D95D3EF3108BEB325EF390CD177D2809D3AD2A5AB0862` |
| 测试副本 | `tmp_hmi_probe_page1_bg_replace.HMI` |
| 测试副本 SHA256 | `C6050895CDFE0FAFD1E690CD92A071A04118422EE7B8A78886F56A71E8DDE988` |
| 替换字节校验 | 通过，槽位起始字节与轻量 PNG 完全一致 |
| 剩余槽位填充 | 通过，PNG 后到旧槽位末尾已补 `0x00` |
| 原 HMI 哈希 | 保持 `892173DB649541701A2F48470BAEEC5CC39CADA97602598FFFB23174A78439E6` |

## 可行结论

后台脚本当前可用于：

- 只读扫描 `.HMI` 中的内嵌 PNG 资源。
- 提取或定位页面背景、图标等图片资源。
- 生成更小的同分辨率 PNG。
- 在测试副本中把更小或等长的 PNG 写入原资源槽位。
- 校验原始 HMI 是否未被修改。

这类操作适合用来快速验证“背景图替换”思路，尤其当新图片大小小于旧资源槽位时，风险相对较低。

## 不可安全自动化的部分

当前脚本不能安全完成以下工作：

- 新增文本控件、进度条、曲线控件或触摸热区。
- 把旧控件名改成长短不同的新控件名。
- 写入或修改 `事件 -> 弹起事件(0)` 里的脚本。
- 自动生成页面滑动动画。
- 自动记录曲线/波形控件的数字对象 ID。

原因是这些内容存储在 HMI 的 `.pa` 页面对象结构中，涉及对象表、长度字段、引用关系和编辑器内部 ID。当前只逆向到资源级别，不能保证直接改二进制页面对象后仍可被编辑器正常打开和编译。

## 后续建议

根据后续判断，脚本副本方式不作为当前正式交付路径。正式路径回退为 USART HMI 编辑器手动编辑原工程，并先另存一份人工编辑副本。

1. 如果要继续验证脚本副本，先用 USART HMI 编辑器打开：

```text
E:/espwork/xiaozhi-esp32/xiaozhi-esp32/tmp_hmi_probe_page1_bg_replace.HMI
```

2. 只检查副本是否能正常打开、page1 背景是否替换成功，不要直接覆盖 `D:/QQ/serial_warm_home .HMI`。
3. 若副本能正常打开，再另存为一个新的人工测试工程，例如 `D:/QQ/serial_warm_home_script_test.HMI`。
4. 控件、触摸热区和事件仍按 `../文档/串口屏手动事件配置手册.md` 手动创建。
5. 如果要让脚本进一步自动化控件创建，需要先获得官方导出格式、可用命令行接口，或完整逆向 `.pa` 页面对象结构；否则不建议继续直接写二进制对象表。

## 仍需人工完成

- 用 USART HMI 编辑器打开 `tmp_hmi_probe_page1_bg_replace.HMI`，确认副本是否可正常打开。
- 观察 page1 背景是否是新的轻量空气评分页。
- 若显示正常，再按文档创建 `t_air_score`、`j_air_detail`、`t_air_state`、`t_air_raw`、`t_temp_d`、`t_humi_d`、`t_comfort`、`hs_back`。
- 保存、编译并下载到串口屏。
- 烧录 ESP32 固件并打开 monitor，确认 page1 控件刷新和返回首页事件。

## 下一阶段目标

- 优先完成 HMI 副本打开验证。
- 如果副本可用，决定是否采用脚本替换背景作为正式流程。
- 如果副本不可用，回退到 USART HMI 编辑器手动导入 `page1_air_detail_hmi_blank.png`。
- 后续继续完善控件、热区、滑动切页和实机验证。

## 实际执行建议更新

当前建议不要再投入时间验证脚本副本。直接按 `../文档/串口屏手动事件配置手册.md` 的完全手动流程操作：

1. 打开 `D:/QQ/serial_warm_home .HMI`。
2. 另存为 `D:/QQ/serial_warm_home_manual_edit.HMI`。
3. 手动导入 `ui_assets/page*.png` 背景。
4. 手动创建文本、进度条、触摸热区。
5. 在右侧 `事件 -> 弹起事件(0)` 填写 `prints` / `printh`。
6. 保存、编译、下载到串口屏。
7. 用 ESP32 monitor 验证事件和控件刷新。
