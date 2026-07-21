# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Development

All build commands run from `xiaozhi-esp32/xiaozhi-esp32/` (the inner directory). Requires ESP-IDF 5.4+.

- `idf.py build` — compile the firmware
- `idf.py -p <PORT> flash monitor` — flash and open serial monitor
- `idf.py clean` — clean build artifacts
- `idf.py menuconfig` — configure Kconfig (board type, language, audio, network, display, etc.)
- `clean.bat` — Windows batch clean script

**Code style**: Google C++ style, enforced via `.clang-format` at the repo root. Run `clang-format -i <files>` before committing.

**Tests**: Python pytest tests in `tests/` (e.g., `test_bread_compact_wifi_regressions.py`). Run with `pytest tests/` from the inner `xiaozhi-esp32/` directory.

## Project Structure

The firmware source lives in `xiaozhi-esp32/xiaozhi-esp32/main/`. All source files are compiled as a single ESP-IDF component via `main/CMakeLists.txt`. Key subdirectories:

- `protocols/` — WebSocket and MQTT+UDP transport implementations
- `audio/` — I2S audio pipeline, Opus codecs, VAD/AEC, OGG demuxer, wake words
- `display/` — OLED, LCD, LVGL9 rich UI, emote display backends
- `boards/` — per-hardware board definitions (~100 boards) and `common/` shared abstractions
- `led/` — LED strip, GPIO LED, circular strip drivers
- `assets/` — OGG audio per language, locale JSON, fonts, emoji collections

Supporting files at the inner `xiaozhi-esp32/` root:
- `main/idf_component.yml` — managed component dependencies
- `partitions/v2/16m.csv` — partition table (v2, incompatible with v1 OTA)
- `sdkconfig.defaults.*` — chip-specific baseline configs (esp32, esp32s3, esp32c3, esp32c5, esp32c6, esp32p4)
- `scripts/` — `build_default_assets.py`, `gen_lang.py`, `release.py`, `mp3_to_ogg.sh`, etc.

## Architecture

### Event Loop

`main.cc` → `Application::Initialize()` then `Application::Run()` (never returns). `Application` is a singleton. Work from any FreeRTOS task is dispatched via `xEventGroupSetBits`, processed synchronously in `Run()`. For deferred callbacks from ISRs or other tasks, use `Application::Schedule(std::function<void()>)`.

`TaskPriorityReset` (in `application.h`) is an RAII helper that temporarily raises task priority.

### State Machine

`DeviceStateMachine` drives transitions: `kDeviceStateUnknown` → `kDeviceStateStarting` → `kDeviceStateWifiConfiguring` → `kDeviceStateConnecting` → `kDeviceStateIdle` → `kDeviceStateListening` → `kDeviceStateSpeaking` → back to idle. Boards call `Application::SetDeviceState()`; `Application::OnStateChanged` is the callback.

### Protocol (Transport)

`protocols/protocol.h` defines a callback-based `Protocol` interface. Two implementations:

- `websocket_protocol.cc` — full duplex over WebSocket
- `mqtt_protocol.cc` — control/JSON over MQTT, audio streaming over UDP

The interface exposes callbacks: `OnIncomingAudio`, `OnIncomingJson`, `OnAudioChannelOpened/Closed`, `OnConnected/Disconnected`, `OnNetworkError`. Listening modes: `kListeningModeAutoStop`, `kListeningModeManualStop`, `kListeningModeRealtime` (requires AEC).

### Audio Pipeline

`audio/audio_service.cc` manages I2S lifecycle, Opus encode/decode, VAD, and AEC. `audio/audio_processor.cc` selects between `afe_audio_processor` (ESP-SR AFE on S3/P4 with PSRAM) and `no_audio_processor` (fallback). Wake words are board-target-specific: AFE-based on S3/P4, ESP wakenet on C3/C5/C6, custom multinet on S3/P4. An audio debugger (`USE_AUDIO_DEBUGGER`) can stream raw audio over UDP to a host.

### Display

Four backends: `oled_display` (SSD1306, SH1106 monochrome), `lcd_display` (raw framebuffer LCDs), `lvgl_display` (LVGL9 with themes, emoji collections, GIF, JPEG), and `emote_display` (expression/emoji abstraction). Fonts and emoji collections are selected per-board in `CMakeLists.txt`.

### Board Abstraction

Each board lives in `boards/<name>/` with a `config.h` (GPIO pins, sample rates, etc.) and optional `config.json`, plus board-specific `.cc/.h` files. The `DECLARE_BOARD(BOARD_CLASS_NAME)` macro registers the factory. Common abstractions in `boards/common/` include `wifi_board`, `ml307_board`, `nt26_board`, `dual_network_board`, `button`, `knob`, `backlight`, `power_save_timer`, `sleep_timer`, `esp_video`, `esp32_camera`, etc.

Board selection is entirely compile-time via `CONFIG_BOARD_TYPE` in menuconfig. `main/CMakeLists.txt` maps each `CONFIG_BOARD_TYPE_*` to a board directory name and globs its `.cc/.c` files. Adding a new board requires: (1) adding a `CONFIG_BOARD_TYPE_*` entry in `Kconfig.projbuild`, (2) creating `boards/<name>/config.h` (and optionally other files), (3) adding an `elseif` branch in `CMakeLists.txt`.

### MCP Server

`mcp_server.h/.cc` implements a device-side MCP server. Tools are registered via `AddTool(name, description, properties, callback)`. The `user_only` flag hides tools from the LLM (visible only to the user UI). The LLM invokes tools through JSON-RPC messages parsed from the protocol. `Application::SendMcpMessage` sends messages to the cloud; `RegisterMcpBroadcastCallback` receives broadcasts (e.g., for state updates).

### OTA

`ota.cc/.h` handles firmware upgrades via ESP-IDF `app_update`. The default OTA URL is `https://api.tenclass.net/xiaozhi/ota/`. v1 and v2 firmware are partition-table-incompatible; OTA upgrade from v1 to v2 is not possible.

### Assets & Localization

`scripts/build_default_assets.py` generates `assets.bin` at build time, embedding fonts, emoji collections, and ESP-SR wake-word models. Language-specific OGG audio files are compiled in via `EMBED_FILES`. Language is selected at compile time via `CONFIG_LANGUAGE_*` Kconfig; missing files fall back to `en-US`. The build system auto-generates `assets/lang_config.h` via `scripts/gen_lang.py`.

### WiFi Provisioning

Three methods, selectable in menuconfig:
- Hotspot (default) — device creates an AP for WiFi credential input
- Acoustic — audio signal transmits WiFi config data
- Blufi — ESP BLE Blufi protocol

### Supported Chips

ESP32, ESP32-C3, ESP32-C5, ESP32-C6, ESP32-S3, ESP32-P4. AFE/AEC/ESP-SR features require ESP32-S3 or ESP32-P4 with PSRAM.
