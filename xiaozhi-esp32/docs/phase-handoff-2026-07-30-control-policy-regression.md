# 2026-07-30 控制策略与回归测试阶段交付

## 当前工程状态

- 工作目录：`E:\espwork\Smart-Home-Controller`
- 开发分支：`codex/logic-ai-optimization`
- 本阶段将自动/节能控制判断从控制器中拆分到纯 C++ 策略模块，策略输出是三类设备的完整目标档位。
- 原有 180 度舵机往复驱动逻辑没有改动。

## 本阶段改动

- 新增 `main/boards/bread-compact-wifi/smart_home_policy.h/.cc`。
- 新增 `tests/native/smart_home_policy_test.cpp`，覆盖空气质量阈值、回差、手动覆盖、无人关闭、无效样本和节能策略。
- 更新 `tests/test_bread_compact_wifi_regressions.py`，不再依赖已经删除的控制器内联判断。
- 新增 `tests/test_smart_home_policy_native.py`，本机存在 C++ 编译器时自动编译并运行策略测试，否则明确跳过。
- 更新 `.github/workflows/quality.yml`，Ubuntu CI 使用 `g++` 强制编译并运行策略测试。

## 验证结果

- `python -m unittest discover -s tests -v`：36 项通过，1 项因本机没有 `g++/clang++` 而跳过。
- GitHub CI 中不会跳过原生策略测试。
- 尚未执行 ESP-IDF 全量编译，待其余阶段完成后统一验证。

## 遗留事项

- 串口屏仍存在重复曲线重放和刷新命令可能交错的问题。
- 真实传感器样本、手动样本、MCP 接口和 PC 桥仍按后续阶段继续优化。
