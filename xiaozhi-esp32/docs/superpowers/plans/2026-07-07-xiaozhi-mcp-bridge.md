# Xiaozhi MCP Bridge Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a local Python MCP bridge that lets Xiaozhi AI call smart-home tools and forwards those tool calls to the ESP32 HTTP API.

**Architecture:** Keep ESP32 firmware unchanged. Add a standalone Python FastMCP stdio service under `tools/xiaozhi_mcp_bridge`; it reads `ESP32_BASE_URL` from the environment and calls `/api/state`, `/api/device`, `/api/mode`, and `/api/environment`. The Xiaozhi WebSocket endpoint stays outside source control and is supplied through `MCP_ENDPOINT` when running the upstream `mcp_pipe.py`.

**Tech Stack:** Python 3, stdlib `urllib`, optional `mcp.server.fastmcp.FastMCP`, unittest.

---

### Task 1: Bridge Tests

**Files:**
- Create: `tests/test_xiaozhi_mcp_bridge.py`

- [ ] **Step 1: Write failing tests**

Write tests that import `tools.xiaozhi_mcp_bridge.smart_home_bridge`, patch `urlopen`, and verify:
- `home_get_state()` calls `GET /api/state`.
- `home_set_fresh_air(True, 2)` calls `POST /api/device` with `{"device":"fresh_air","power":true,"level":2}`.
- `home_set_auto(True)` calls `POST /api/mode`.
- `home_set_environment_preset("POLLUTED")` calls `POST /api/environment`.

- [ ] **Step 2: Run tests and verify red**

Run:

```powershell
python -m unittest tests.test_xiaozhi_mcp_bridge -v
```

Expected: fail because `tools.xiaozhi_mcp_bridge.smart_home_bridge` does not exist.

### Task 2: Bridge Implementation

**Files:**
- Create: `tools/xiaozhi_mcp_bridge/__init__.py`
- Create: `tools/xiaozhi_mcp_bridge/smart_home_bridge.py`
- Create: `tools/xiaozhi_mcp_bridge/requirements.txt`

- [ ] **Step 1: Implement HTTP helper**

Add `_request_json(path, method="GET", payload=None)` using `ESP32_BASE_URL`, JSON serialization, and a timeout.

- [ ] **Step 2: Implement MCP tool functions**

Expose:
- `home_get_state`
- `home_set_purifier`
- `home_set_fresh_air`
- `home_set_humidifier`
- `home_set_auto`
- `home_set_eco`
- `home_set_environment_preset`
- `home_set_manual_environment`
- `home_disable_manual_environment`
- `home_get_advice`

- [ ] **Step 3: Verify green**

Run:

```powershell
python -m unittest tests.test_xiaozhi_mcp_bridge -v
```

Expected: pass.

### Task 3: Documentation

**Files:**
- Create: `tools/xiaozhi_mcp_bridge/README.md`
- Create: `docs/phase-handoff-2026-07-07-xiaozhi-mcp-bridge.md`

- [ ] **Step 1: Document safe startup**

Explain:
- Do not commit the Xiaozhi token.
- Set `MCP_ENDPOINT` and `ESP32_BASE_URL` through environment variables.
- Start the bridge with the upstream `mcp_pipe.py`.

- [ ] **Step 2: Final verification**

Run:

```powershell
python -m unittest discover -s tests -v
```

Expected: all current tests pass.
