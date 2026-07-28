# 2026-07-08 作品报告与答辩 PPT 重做版阶段交付

## 本阶段目标

按照用户确认的方案 A，重新生成一套面向物联网应用类作品的正式材料。主结构参照《中国大学生计算机设计大赛物联网应用类作品技术文档》，写作展开方式参考 `CICC1004237初赛作品报告.docx`，整体语言改为清楚、直接、少套话的表达。

## 输出文件

本阶段新增输出目录：

`docs/report_output_rebuild/`

最终文件：

- `docs/report_output_rebuild/final/喵伴空气管家作品设计报告_重做版_2026-07-08.docx`
- `docs/report_output_rebuild/final/喵伴空气管家作品设计报告_重做版_2026-07-08.pdf`
- `docs/report_output_rebuild/final/喵伴空气管家答辩PPT_重做版_2026-07-08.pptx`
- `docs/report_output_rebuild/final/视频拍摄与答辩讲述建议_重做版_2026-07-08.md`

生成脚本：

- `docs/report_output_rebuild/generate_rebuild_materials.py`
- `docs/report_output_rebuild/quality_check_rebuild.py`

规格与计划：

- `docs/superpowers/specs/2026-07-08-report-rebuild-design.md`
- `docs/superpowers/plans/2026-07-08-report-rebuild-materials.md`

## 内容决策

- 报告封面的作品编号、作品名称、作者、版本编号、填写日期按用户要求留空，方便后续自行填写。
- 报告采用物联网技术文档章节：摘要、目录、第 1 章作品概述、第 2 章需求分析、第 3 章技术方案、第 4 章方案实现、第 5 章测试报告、第 6 章应用前景、附录和参考文献。
- PPT 共 10 页，颜色控制在深墨绿、浅灰白、暖橙三种主色内。
- 配图全部重新生成，包含系统架构、数据流、硬件连接、串口屏页面、小程序流程、AI/MCP 桥接、自动/节能逻辑、测试流程、应用场景和三种交互入口。
- 当前工程事实以源码为准：新风部分写为 `GPIO21` 舵机带动扇叶，在 0 到 180 度范围内按档位往复摆动。
- 由于当前素材目录未找到真实小程序截图，本版使用“小程序交互流程图”和“小程序控制界面结构示意”表达小程序功能。若后续有真实截图，可替换报告和 PPT 中对应位置。

## 验证结果

已运行：

```powershell
& 'C:\Users\cj041\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' 'E:\espwork\xiaozhi-esp32\xiaozhi-esp32\docs\report_output_rebuild\generate_rebuild_materials.py'
```

结果：生成 DOCX、PDF、PPTX、视频建议文档和 10 张自绘配图。

已运行：

```powershell
& 'C:\Users\cj041\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' 'E:\espwork\xiaozhi-esp32\xiaozhi-esp32\docs\report_output_rebuild\quality_check_rebuild.py'
```

结果：

```text
ALL CHECKS PASSED
pdf_pages=29
ppt_slides=10
custom_figures=10
```

PDF 渲染检查：

- 已使用 Poppler 渲染第 1-8 页和第 18-24 页。
- 检查了封面、目录、正文图文页、实物图页、测试表格页。
- 中文显示正常，未发现明显裁切、重叠或图片变形。

PPT 检查：

- 使用 `python-pptx` 检查 10 页幻灯片，均包含文本和图片/图形元素。
- 检查形状边界，未发现越界元素。
- 使用 Python 标准库检查 PPTX 包结构：10 个 slide XML、10 个 slide 关系文件、11 个媒体资源，核心文件完整。

## 未完成或受限事项

- 本机没有可调用的 LibreOffice `soffice`，也没有 PowerPoint 命令行转换器，因此未能把 PPTX 渲染为逐页图片做最终视觉检查。
- `markitdown` 和 PPTX 技能自带 Office 验证脚本依赖的 `defusedxml` 当前未安装；本阶段没有安装外部包，改用 `python-pptx` 和标准库完成结构检查。
- 小程序真实截图未在当前素材目录中找到，本版用示意图代替。

## 建议下一步

1. 用户打开 DOCX，补齐封面信息。
2. 用户打开 PPTX，人工浏览 10 页，确认实际 PowerPoint 渲染中没有文字挤压。
3. 如果有真实小程序截图，把截图发给 Codex 或放入 `docs/report_output_rebuild/assets/`，可替换示意图。
4. 答辩前按视频建议文档重新过一遍 3 分钟视频讲述节奏。
