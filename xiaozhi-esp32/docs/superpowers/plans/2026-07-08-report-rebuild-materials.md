# Report Rebuild Materials Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Generate a rebuilt IoT technical report, PDF, defense PPT, diagrams, and video guidance for the current ESP32S3 indoor air management project.

**Architecture:** Build a local Python generation pipeline under `docs/report_output_rebuild/`. The pipeline creates purpose-specific diagrams, writes a DOCX report following the IoT technical document structure, exports a readable PDF, creates a restrained 10-slide PPT, and runs text/asset QA checks.

**Tech Stack:** Python, python-docx, reportlab, python-pptx, Pillow, pypdf, Poppler `pdftoppm`, existing local photos/screenshots.

---

### Task 1: Prepare Output Workspace And Asset Inventory

**Files:**
- Create: `E:/espwork/xiaozhi-esp32/xiaozhi-esp32/docs/report_output_rebuild/`
- Create: `E:/espwork/xiaozhi-esp32/xiaozhi-esp32/docs/report_output_rebuild/assets/`
- Create: `E:/espwork/xiaozhi-esp32/xiaozhi-esp32/docs/report_output_rebuild/final/`
- Create: `E:/espwork/xiaozhi-esp32/xiaozhi-esp32/docs/report_output_rebuild/tmp/`
- Read: `E:/espwork/xiaozhi-esp32/xiaozhi-esp32/docs/report_output/assets/*`

- [ ] **Step 1: Create directories**

Run:

```powershell
New-Item -ItemType Directory -Force -Path `
  'E:\espwork\xiaozhi-esp32\xiaozhi-esp32\docs\report_output_rebuild\assets',`
  'E:\espwork\xiaozhi-esp32\xiaozhi-esp32\docs\report_output_rebuild\final',`
  'E:\espwork\xiaozhi-esp32\xiaozhi-esp32\docs\report_output_rebuild\tmp'
```

Expected: directories exist.

- [ ] **Step 2: Copy user-provided screenshots and photos into rebuild assets**

Run:

```powershell
Copy-Item -Force 'E:\espwork\xiaozhi-esp32\xiaozhi-esp32\docs\report_output\assets\hardware_overview_1.jpg' 'E:\espwork\xiaozhi-esp32\xiaozhi-esp32\docs\report_output_rebuild\assets\hardware_overview_1.jpg'
Copy-Item -Force 'E:\espwork\xiaozhi-esp32\xiaozhi-esp32\docs\report_output\assets\hardware_overview_2.jpg' 'E:\espwork\xiaozhi-esp32\xiaozhi-esp32\docs\report_output_rebuild\assets\hardware_overview_2.jpg'
Copy-Item -Force 'E:\espwork\xiaozhi-esp32\xiaozhi-esp32\docs\report_output\assets\serial_page_home.png' 'E:\espwork\xiaozhi-esp32\xiaozhi-esp32\docs\report_output_rebuild\assets\serial_page_home.png'
Copy-Item -Force 'E:\espwork\xiaozhi-esp32\xiaozhi-esp32\docs\report_output\assets\serial_page_air_score.png' 'E:\espwork\xiaozhi-esp32\xiaozhi-esp32\docs\report_output_rebuild\assets\serial_page_air_score.png'
Copy-Item -Force 'E:\espwork\xiaozhi-esp32\xiaozhi-esp32\docs\report_output\assets\serial_page_ai_settings.png' 'E:\espwork\xiaozhi-esp32\xiaozhi-esp32\docs\report_output_rebuild\assets\serial_page_ai_settings.png'
Copy-Item -Force 'E:\espwork\xiaozhi-esp32\xiaozhi-esp32\docs\report_output\assets\upload_requirements.png' 'E:\espwork\xiaozhi-esp32\xiaozhi-esp32\docs\report_output_rebuild\assets\upload_requirements.png'
```

Expected: six copied files in rebuild assets.

### Task 2: Build Rebuild Generator Script

**Files:**
- Create: `E:/espwork/xiaozhi-esp32/xiaozhi-esp32/docs/report_output_rebuild/generate_rebuild_materials.py`
- Output: diagrams, report DOCX/PDF, PPTX

- [ ] **Step 1: Add generator script**

Create a Python script with these responsibilities:

- Define project facts and report text in structured sections.
- Draw each diagram with a separate Pillow function.
- Generate DOCX with IoT report chapter structure and Chinese formatting.
- Generate PDF with reportlab, readable Chinese font, page numbers, captions, tables.
- Generate PPTX with 10 slides and the three-color visual system.
- Write video shooting suggestions into the report appendix and PPT notes content file if needed.

- [ ] **Step 2: Run generator**

Run:

```powershell
& 'C:\Users\cj041\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' 'E:\espwork\xiaozhi-esp32\xiaozhi-esp32\docs\report_output_rebuild\generate_rebuild_materials.py'
```

Expected:

- `docs/report_output_rebuild/final/喵伴空气管家作品设计报告_重做版_2026-07-08.docx`
- `docs/report_output_rebuild/final/喵伴空气管家作品设计报告_重做版_2026-07-08.pdf`
- `docs/report_output_rebuild/final/喵伴空气管家答辩PPT_重做版_2026-07-08.pptx`
- at least 9 custom diagrams in `docs/report_output_rebuild/assets/`

### Task 3: Add Quality Check Script

**Files:**
- Create: `E:/espwork/xiaozhi-esp32/xiaozhi-esp32/docs/report_output_rebuild/quality_check_rebuild.py`

- [ ] **Step 1: Add checks**

The script checks:

- Final DOCX exists and can be opened by `python-docx`.
- Final PDF exists and can be opened by `pypdf`.
- Final PPTX exists and can be opened by `python-pptx`.
- Report has at least 20 pages in PDF.
- PPT has 8 to 10 slides.
- Banned weak phrases are absent from final DOCX/PDF/PPT text.
- Required chapters exist in report text.
- Required functions exist in report text: 串口屏, 小程序, AI/MCP, 自动模式, 节能模式, GPIO13, GPIO14, GPIO21, DHT11, MQ135.

- [ ] **Step 2: Run checks**

Run:

```powershell
& 'C:\Users\cj041\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' 'E:\espwork\xiaozhi-esp32\xiaozhi-esp32\docs\report_output_rebuild\quality_check_rebuild.py'
```

Expected: `ALL CHECKS PASSED`.

### Task 4: Render PDF Pages And Inspect

**Files:**
- Read: `docs/report_output_rebuild/final/喵伴空气管家作品设计报告_重做版_2026-07-08.pdf`
- Output: `docs/report_output_rebuild/tmp/pdf_pages/*.png`

- [ ] **Step 1: Render key report pages**

Run:

```powershell
New-Item -ItemType Directory -Force -Path 'E:\espwork\xiaozhi-esp32\xiaozhi-esp32\docs\report_output_rebuild\tmp\pdf_pages'
& 'C:\Users\cj041\.cache\codex-runtimes\codex-primary-runtime\dependencies\native\poppler\Library\bin\pdftoppm.exe' -png -r 130 -f 1 -l 8 'E:\espwork\xiaozhi-esp32\xiaozhi-esp32\docs\report_output_rebuild\final\喵伴空气管家作品设计报告_重做版_2026-07-08.pdf' 'E:\espwork\xiaozhi-esp32\xiaozhi-esp32\docs\report_output_rebuild\tmp\pdf_pages\report'
& 'C:\Users\cj041\.cache\codex-runtimes\codex-primary-runtime\dependencies\native\poppler\Library\bin\pdftoppm.exe' -png -r 130 -f 18 -l 24 'E:\espwork\xiaozhi-esp32\xiaozhi-esp32\docs\report_output_rebuild\final\喵伴空气管家作品设计报告_重做版_2026-07-08.pdf' 'E:\espwork\xiaozhi-esp32\xiaozhi-esp32\docs\report_output_rebuild\tmp\pdf_pages\report_mid'
```

Expected: PNG pages render without Poppler error.

- [ ] **Step 2: Visually inspect representative PNG pages**

Use image viewer on cover, TOC, architecture page, implementation page, testing page. Check no clipped text, no unreadable glyphs, no overlapping captions, and no stretched screenshots.

### Task 5: Render PPT And Inspect

**Files:**
- Read: `docs/report_output_rebuild/final/喵伴空气管家答辩PPT_重做版_2026-07-08.pptx`
- Output: `docs/report_output_rebuild/tmp/pptx_pages/*.png` if conversion is available.

- [ ] **Step 1: Extract PPT text**

Run:

```powershell
& 'C:\Users\cj041\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' -m markitdown 'E:\espwork\xiaozhi-esp32\xiaozhi-esp32\docs\report_output_rebuild\final\喵伴空气管家答辩PPT_重做版_2026-07-08.pptx'
```

Expected: slide text appears in correct order and no placeholder text appears.

- [ ] **Step 2: Convert PPT to PDF or inspect slide XML when rendering is unavailable**

Try LibreOffice conversion if available. If not available, use `python-pptx` shape inspection and rely on direct PPTX openability check.

Expected: either rendered images are available for visual QA, or the limitation is recorded in handoff notes.

### Task 6: Write Phase Handoff

**Files:**
- Create: `E:/espwork/xiaozhi-esp32/xiaozhi-esp32/docs/phase-handoff-2026-07-08-report-ppt-rebuild.md`
- Modify: `E:/espwork/xiaozhi-esp32/xiaozhi-esp32/docs/current-project-handoff.md` if adding a short pointer is useful and does not overwrite user changes.

- [ ] **Step 1: Write handoff**

The handoff records:

- Output files.
- Reference standards used.
- Key content decisions.
- Verification commands and results.
- Known limitations, especially PPT visual rendering if no converter is available.
- Next manual steps for the user.

- [ ] **Step 2: Check git status**

Run:

```powershell
& 'C:\Users\cj041\.cache\codex-runtimes\codex-primary-runtime\dependencies\native\git\cmd\git.exe' -C 'E:\espwork\xiaozhi-esp32\xiaozhi-esp32' status --short -- docs/report_output_rebuild docs/phase-handoff-2026-07-08-report-ppt-rebuild.md docs/superpowers/plans/2026-07-08-report-rebuild-materials.md
```

Expected: only intended report rebuild files are listed.

## Self-Review

- Spec coverage: tasks cover output workspace, diagrams, DOCX, PDF, PPTX, QA, visual checks, and handoff.
- Placeholder scan: no TBD/TODO/fill-in-later language is used as an implementation instruction.
- Type consistency: file paths and final filenames are consistent across generation, QA, rendering, and handoff.
- Scope check: the work is a single deliverable set, not separate firmware development.
