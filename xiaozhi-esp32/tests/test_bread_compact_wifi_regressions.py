import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
BOARD_DIR = ROOT / "main" / "boards" / "bread-compact-wifi"


class BreadCompactWifiRegressionTest(unittest.TestCase):
    def test_dht11_driver_rejects_implausible_frames(self):
        source = (BOARD_DIR / "dht11_sensor.cc").read_text(encoding="utf-8")

        self.assertIn("ValidateReading", source)
        self.assertIn("humidity > 100.0f", source)
        self.assertIn("temperature > 60.0f", source)
        self.assertIn("Range error", source)

    def test_dht11_driver_consumes_response_high_before_data_bits(self):
        source = (BOARD_DIR / "dht11_sensor.cc").read_text(encoding="utf-8")

        response_high = source.find("if (!WaitForLevel(1, 200))")
        data_read = source.find("portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;")
        response_high_end = source.find("if (!WaitForLevel(0, 200))", response_high)

        self.assertGreater(response_high, -1)
        self.assertGreater(data_read, response_high)
        self.assertGreater(response_high_end, response_high)
        self.assertLess(response_high_end, data_read)
        self.assertIn("Response high timeout", source[response_high_end:data_read])

    def test_sensor_task_keeps_last_valid_dht11_reading_on_transient_failure(self):
        source = (BOARD_DIR / "compact_wifi_board.cc").read_text(encoding="utf-8")

        self.assertIn("has_last_dht_reading", source)
        self.assertIn("display_dht_ok", source)
        self.assertIn("Use cached DHT11 reading", source)
        self.assertNotIn("hmi_data.has_temperature = dht_ok;", source)
        self.assertNotIn("hmi_data.has_humidity = dht_ok;", source)

    def test_serial_hmi_debounces_page_refresh_to_reduce_flicker(self):
        source = (BOARD_DIR / "serial_hmi.cc").read_text(encoding="utf-8")
        header = (BOARD_DIR / "serial_hmi.h").read_text(encoding="utf-8")

        self.assertIn("if (page_id == current_page_id_)", source)
        self.assertIn("vTaskDelay(pdMS_TO_TICKS(80))", source)
        self.assertIn("Page already active", source)
        self.assertIn("kPageSwitchDebounceMs", source)
        self.assertIn("xTaskGetTickCount", source)
        self.assertIn("Ignore rapid page switch", source)
        self.assertIn("last_page_switch_ticks_", header)
        self.assertIn('SendCommand("ref_stop")', source)
        self.assertIn('SendCommand("ref_star")', source)
        self.assertIn("BeginBatchRefresh", source)
        self.assertIn("EndBatchRefresh", source)

    def test_critical_cplusplus_statements_are_not_swallowed_by_comments(self):
        checks = {
            "compact_wifi_board.cc": [
                "Display* display_ = nullptr;",
                "SerialHmi serial_hmi_;",
                "serial_hmi_.UpdateAirQuality(hmi_data);",
            ],
            "dht11_sensor.cc": [
                "gpio_config_t io_conf = {",
                "gpio_set_level(pin_, 0);",
                "if (!WaitForLevel(0, 200)) {",
                "portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;",
            ],
            "serial_hmi.cc": [
                'if (TokenEquals(page_name, "HOME") ||',
                "last_air_quality_data_ = data;",
                "switch (current_page_id_) {",
            ],
            "smart_home_controller.cc": [
                "const char* ComfortDescription(const EnvironmentSample& sample) {",
                "if (sample.temperature_c >= 22.0f && sample.temperature_c <= 28.0f &&",
                "if (sample.humidity_percent < 35.0f) {",
            ],
        }

        for filename, needles in checks.items():
            lines = (BOARD_DIR / filename).read_text(encoding="utf-8").splitlines()
            for needle in needles:
                matches = [line for line in lines if needle in line]
                self.assertTrue(matches, f"{needle!r} not found in {filename}")
                for line in matches:
                    prefix = line.split(needle, 1)[0]
                    self.assertNotIn("//", prefix, f"{needle!r} is commented out in {filename}: {line}")

    def test_smart_home_controller_owns_actuators_and_gpio_mapping(self):
        config = (BOARD_DIR / "config.h").read_text(encoding="utf-8")
        header = (BOARD_DIR / "smart_home_controller.h").read_text(encoding="utf-8")
        source = (BOARD_DIR / "smart_home_controller.cc").read_text(encoding="utf-8")

        self.assertRegex(config, r"#define\s+SMART_HOME_PURIFIER_LED_GPIO\s+GPIO_NUM_13")
        self.assertRegex(config, r"#define\s+SMART_HOME_HUMIDIFIER_LED_GPIO\s+GPIO_NUM_14")
        self.assertRegex(config, r"#define\s+SMART_HOME_FRESH_AIR_SERVO_GPIO\s+GPIO_NUM_21")
        self.assertIn("class SmartHomeController", header)
        self.assertIn("ApplyPurifier", source)
        self.assertIn("ApplyHumidifier", source)
        self.assertIn("SetServoAngle", source)
        self.assertIn("SetServoProfileForLevel", source)
        self.assertIn("ServoTaskLoop", source)
        self.assertIn("LEDC_TIMER_13_BIT", source)
        self.assertIn("SMART_HOME_SERVO_PWM_HZ", source)

    def test_smart_home_screen_events_and_mcp_tools_are_registered(self):
        board = (BOARD_DIR / "compact_wifi_board.cc").read_text(encoding="utf-8")
        source = (BOARD_DIR / "smart_home_controller.cc").read_text(encoding="utf-8")
        header = (BOARD_DIR / "smart_home_controller.h").read_text(encoding="utf-8")

        self.assertIn("SmartHomeController smart_home_", board)
        self.assertIn("smart_home_.HandleDeviceAction", board)
        self.assertIn("smart_home_.HandleModeAction", board)
        self.assertIn("smart_home_.UpdateEnvironment", board)
        self.assertIn("NextToggleLevel", header)
        self.assertIn("const int next_level = NextToggleLevel", source)
        self.assertIn("SetPurifier(next_level != 0, next_level)", source)
        self.assertIn("SetFreshAir(next_level != 0, next_level)", source)
        self.assertIn("SetHumidifier(next_level != 0, next_level)", source)
        self.assertIn("Device action applied", source)
        self.assertIn("Apply purifier output", source)
        self.assertIn("Apply humidifier output", source)
        self.assertIn("Apply fresh air fan output", source)

        for tool in [
            "self.home.get_state",
            "self.home.set_purifier",
            "self.home.set_fresh_air",
            "self.home.set_humidifier",
            "self.home.set_auto",
            "self.home.set_eco",
        ]:
            self.assertIn(tool, source)

    def test_smart_home_auto_eco_and_180_degree_servo_fan_rules(self):
        source = (BOARD_DIR / "smart_home_controller.cc").read_text(encoding="utf-8")
        header = (BOARD_DIR / "smart_home_controller.h").read_text(encoding="utf-8")
        design = (ROOT / "docs" / "superpowers" / "specs" / "2026-07-05-smart-home-control-design.md").read_text(encoding="utf-8")

        self.assertIn("SetEcoMode", source)
        self.assertIn("SetAutoMode", source)
        self.assertIn("state_.auto_mode = false", source)
        self.assertIn("state_.eco_mode = false", source)
        self.assertIn("EvaluateAutoMode", source)
        self.assertIn("humidity_percent < 40.0f", source)
        self.assertIn("mq135_raw >= 2000", source)
        self.assertIn("SetServoAngle", source)
        self.assertIn("SetServoProfileForLevel", source)
        self.assertIn("ServoTaskLoop", source)
        self.assertIn("servo_target_min_angle_", header)
        self.assertIn("servo_target_max_angle_", header)
        self.assertIn("servo_step_degrees_", header)
        self.assertIn("servo_step_delay_ms_", header)
        self.assertIn("xTaskCreate", source)
        self.assertIn("fresh_air_servo", source)
        self.assertIn("0", source)
        self.assertIn("180", source)
        self.assertNotIn("SetContinuousServoPulseUs", source)
        self.assertNotIn("FreshAirPulseUsForLevel", source)
        self.assertIn("180°角度舵机扇叶", design)
        self.assertIn("往复摆动", design)

    def test_presence_light_alarm_and_adaptive_eco_contract(self):
        header = (BOARD_DIR / "smart_home_controller.h").read_text(encoding="utf-8")
        source = (BOARD_DIR / "smart_home_controller.cc").read_text(encoding="utf-8")
        http_source = (BOARD_DIR / "smart_home_http_server.cc").read_text(encoding="utf-8")
        board = (BOARD_DIR / "compact_wifi_board.cc").read_text(encoding="utf-8")

        for symbol in [
            "UpdatePresence",
            "UpdateAmbientLight",
            "SetLight",
            "AcknowledgeAlarm",
            "EvaluateEcoMode",
            "EvaluateLighting",
            "EvaluateEnvironmentAlarm",
        ]:
            self.assertIn(symbol, header)
            self.assertIn(symbol, source)

        for state_field in [
            "occupancy_known",
            "occupied",
            "has_ambient_light",
            "ambient_light_percent",
            "light_on",
            "alarm_active",
        ]:
            self.assertIn(state_field, header)
            self.assertIn(f'"{state_field}"', source)

        eco_body = source.split("void SmartHomeController::SetEcoMode", 1)[1].split(
            "void SmartHomeController::SetManualEnvironmentMode", 1
        )[0]
        self.assertIn("EvaluateEcoMode", eco_body)
        self.assertNotIn("state_.purifier_level = 0", eco_body)
        self.assertIn("ShutdownForNoOccupancy", source)
        self.assertIn("kDarkThresholdPercent", source)
        self.assertIn('"self.home.set_light"', source)
        self.assertIn('"self.home.update_context"', source)
        self.assertIn('"self.home.acknowledge_alarm"', source)
        self.assertIn('"/api/context"', http_source)
        self.assertIn('"/api/alarm/ack"', http_source)
        self.assertIn('TokenEquals(device, "light")', http_source)
        self.assertIn("SetPresenceDetectedCallback", board)
        self.assertIn("Application::GetInstance().StartListening()", board)
        self.assertIn("SetAlarmOutputCallback", board)
        self.assertIn("app.Alert", board)

    def test_air_curve_id_is_confirmed_and_temperature_humidity_curves_remain_gated(self):
        header = (BOARD_DIR / "smart_home_controller.h").read_text(encoding="utf-8")
        source = (BOARD_DIR / "smart_home_controller.cc").read_text(encoding="utf-8")
        widgets = (BOARD_DIR / "serial_hmi_widgets.json").read_text(encoding="utf-8")

        self.assertIn("EnvironmentSample history_[30]", header)
        self.assertIn("history_write_index_", header)
        self.assertIn("RecordEnvironmentSample", source)
        self.assertIn("kAirCurveId = 12", source)
        self.assertIn("kCurveIdUnavailable", source)
        self.assertIn("MaybeSendCurvePoint", source)
        self.assertIn("curve_id < 0", source)
        self.assertIn("add %d,%d,%d", source)
        self.assertIn('"name": "c_air"', widgets)
        self.assertIn('"numeric_id": 12', widgets)

    def test_serial_hmi_replays_air_curve_history_on_air_detail_page(self):
        header = (BOARD_DIR / "serial_hmi.h").read_text(encoding="utf-8")
        source = (BOARD_DIR / "serial_hmi.cc").read_text(encoding="utf-8")

        self.assertIn("kAirCurveId = 12", source)
        self.assertIn("kAirCurveHistorySize", header)
        self.assertIn("air_curve_scores_", header)
        self.assertIn("RecordAirCurveScore", source)
        self.assertIn("ReplayAirCurveHistory", source)
        self.assertIn("ClearCurve", source)
        self.assertIn('"cle %d,%d"', source)
        self.assertIn('"add %d,%d,%d"', source)
        self.assertIn("ReplayAirCurveHistory()", source)
        self.assertIn("current_page_id_ == kAirDetailPageId", source)

    def test_environment_comfort_advice_manual_mode_and_ai_tool_contract(self):
        header = (BOARD_DIR / "smart_home_controller.h").read_text(encoding="utf-8")
        source = (BOARD_DIR / "smart_home_controller.cc").read_text(encoding="utf-8")

        self.assertIn("manual_environment_mode", header)
        self.assertIn("SetManualEnvironment", header)
        self.assertIn("SetEnvironmentPreset", header)
        self.assertIn("ComfortDescription", source)
        self.assertIn("AdviceForEnvironment", source)
        self.assertIn('"comfort"', source)
        self.assertIn('"advice"', source)
        self.assertIn('"manual_environment_mode"', source)
        self.assertIn('"environment_source"', source)
        self.assertIn("开空调", source)
        self.assertIn("开加湿器", source)
        self.assertIn("开净化器", source)
        self.assertIn("self.home.set_manual_environment", source)
        self.assertIn("self.home.set_environment_preset", source)
        self.assertIn("self.home.get_advice", source)
        self.assertIn("手动模拟", source)

    def test_serial_hmi_accepts_manual_environment_touch_events(self):
        header = (BOARD_DIR / "serial_hmi.h").read_text(encoding="utf-8")
        source = (BOARD_DIR / "serial_hmi.cc").read_text(encoding="utf-8")
        board = (BOARD_DIR / "compact_wifi_board.cc").read_text(encoding="utf-8")
        widgets = (BOARD_DIR / "serial_hmi_widgets.json").read_text(encoding="utf-8")

        self.assertIn("kEnvironmentAction", header)
        self.assertIn('StartsWith(line, "BTN,ENV,")', source)
        self.assertIn("smart_home_.HandleEnvironmentAction", board)
        for event in [
            "BTN,ENV,MANUAL,TOGGLE",
            "BTN,ENV,SCENE,GOOD",
            "BTN,ENV,SCENE,HOT",
            "BTN,ENV,SCENE,DRY",
            "BTN,ENV,SCENE,POLLUTED",
        ]:
            self.assertIn(event, widgets)

    def test_http_api_supports_manual_environment_endpoint(self):
        header = (BOARD_DIR / "smart_home_http_server.h").read_text(encoding="utf-8")
        source = (BOARD_DIR / "smart_home_http_server.cc").read_text(encoding="utf-8")

        self.assertIn("EnvironmentHandler", header)
        self.assertIn("HandleEnvironment", header)
        self.assertIn('"/api/environment"', source)
        self.assertIn("SetManualEnvironment", source)
        self.assertIn("SetManualEnvironmentMode", source)
        self.assertIn("JsonFloat", source)
        self.assertIn("GET,POST,OPTIONS", source)

    def test_mini_program_exposes_manual_environment_input(self):
        mini_dir = ROOT.parent / "mini_program_demo" / "pages" / "index"
        js = (mini_dir / "index.js").read_text(encoding="utf-8")
        wxml = (mini_dir / "index.wxml").read_text(encoding="utf-8")
        wxss = (mini_dir / "index.wxss").read_text(encoding="utf-8")

        self.assertIn("manualForm", js)
        self.assertIn("setManualEnvironment", js)
        self.assertIn("disableManualEnvironment", js)
        self.assertIn('"/api/environment"', js)
        self.assertIn("舒适度", wxml)
        self.assertIn("环境建议", wxml)
        self.assertIn("手动输入数据", wxml)
        self.assertIn("bindinput=\"onManualInput\"", wxml)
        self.assertIn("bindtap=\"setManualEnvironment\"", wxml)
        self.assertIn("bindtap=\"disableManualEnvironment\"", wxml)
        self.assertIn(".manual-grid", wxss)

    def test_smart_home_local_http_api_is_registered_for_mini_program(self):
        self.assertTrue((BOARD_DIR / "smart_home_http_server.h").exists())
        self.assertTrue((BOARD_DIR / "smart_home_http_server.cc").exists())

        board = (BOARD_DIR / "compact_wifi_board.cc").read_text(encoding="utf-8")
        header = (BOARD_DIR / "smart_home_http_server.h").read_text(encoding="utf-8")
        source = (BOARD_DIR / "smart_home_http_server.cc").read_text(encoding="utf-8")
        controller_header = (BOARD_DIR / "smart_home_controller.h").read_text(encoding="utf-8")
        cmake = (ROOT / "main" / "CMakeLists.txt").read_text(encoding="utf-8")

        self.assertIn("class SmartHomeHttpServer", header)
        self.assertIn("#include <esp_http_server.h>", header)
        self.assertIn("SmartHomeHttpServer smart_home_http_", board)
        self.assertIn("smart_home_http_(&smart_home_)", board)
        self.assertIn("smart_home_http_.Start()", board)
        self.assertIn("SetNetworkEventCallback(NetworkEventCallback callback)", board)
        self.assertIn("NetworkEvent::Connected", board)
        constructor_body = board.split("CompactWifiBoard()", 1)[1].split("virtual void SetNetworkEventCallback", 1)[0]
        self.assertNotIn("smart_home_http_.Start()", constructor_body)
        self.assertIn("esp_http_server", cmake)

        self.assertIn("BuildStateJson", controller_header)
        self.assertIn("BuildHistoryJson", controller_header)
        self.assertIn("GetLastSample", controller_header)

        for route in [
            '"/api/state"',
            '"/api/history"',
            '"/api/device"',
            '"/api/mode"',
        ]:
            self.assertIn(route, source)

        self.assertIn("httpd_start", source)
        self.assertIn("kSmartHomeHttpPort = 8080", source)
        self.assertIn("config.server_port = kSmartHomeHttpPort", source)
        self.assertIn("httpd_register_uri_handler", source)
        self.assertIn("Access-Control-Allow-Origin", source)
        self.assertIn("cJSON_ParseWithLength", source)
        self.assertIn("SetPurifier", source)
        self.assertIn("SetFreshAir", source)
        self.assertIn("SetHumidifier", source)
        self.assertIn("SetAutoMode", source)
        self.assertIn("SetEcoMode", source)

    def test_ld2450_and_ambient_light_hardware_contract(self):
        config = (BOARD_DIR / "config.h").read_text(encoding="utf-8")
        board = (BOARD_DIR / "compact_wifi_board.cc").read_text(encoding="utf-8")
        controller = (BOARD_DIR / "smart_home_controller.cc").read_text(encoding="utf-8")

        self.assertRegex(config, r"#define\s+AMBIENT_LIGHT_ADC_CHANNEL\s+ADC_CHANNEL_1")
        self.assertRegex(config, r"#define\s+LD2450_UART_PORT\s+UART_NUM_1")
        self.assertRegex(config, r"#define\s+LD2450_UART_RX_PIN\s+GPIO_NUM_11")
        self.assertRegex(config, r"#define\s+LD2450_UART_TX_PIN\s+GPIO_NUM_12")
        self.assertTrue((BOARD_DIR / "ambient_light_filter.h").exists())
        self.assertTrue((BOARD_DIR / "ambient_light_sensor.h").exists())
        self.assertTrue((BOARD_DIR / "ld2450_protocol.h").exists())
        self.assertTrue((BOARD_DIR / "ld2450_sensor.h").exists())
        self.assertIn("AmbientLightSensor", board)
        self.assertIn("Ld2450Sensor", board)
        self.assertIn("RadarTask", board)
        self.assertIn("UpdateRadarObservation", controller)

    def test_ambient_light_default_calibration_treats_higher_ao_as_darker(self):
        config = (BOARD_DIR / "config.h").read_text(encoding="utf-8")

        dark_match = re.search(r"#define\s+AMBIENT_LIGHT_DARK_RAW\s+(\d+)", config)
        bright_match = re.search(r"#define\s+AMBIENT_LIGHT_BRIGHT_RAW\s+(\d+)", config)

        self.assertIsNotNone(dark_match)
        self.assertIsNotNone(bright_match)
        self.assertGreater(int(dark_match.group(1)), int(bright_match.group(1)))

    def test_radar_status_and_light_hysteresis_contract(self):
        header = (BOARD_DIR / "smart_home_controller.h").read_text(encoding="utf-8")
        source = (BOARD_DIR / "smart_home_controller.cc").read_text(encoding="utf-8")

        for state_field in ["has_radar_data", "radar_target_count", "radar_zone", "radar_nearest_x_mm"]:
            self.assertIn(state_field, header)
            self.assertIn(f'"{state_field}"', source)

        self.assertIn("void UpdateRadarObservation(int target_count)", header)
        self.assertIn("void SmartHomeController::UpdateRadarObservation(int target_count)", source)
        self.assertIn("kLightOnThresholdPercent", header)
        self.assertIn("kLightOffThresholdPercent", header)
        self.assertIn("state_.ambient_light_percent >= kLightOffThresholdPercent", source)
        self.assertIn("kRadarVacancyTimeoutMs", header)
        self.assertIn("radar_clear_since_ms_", header)
        self.assertIn("UpdatePresence(false)", source)

    def test_controller_health_thread_safety_and_manual_override_contract(self):
        header = (BOARD_DIR / "smart_home_controller.h").read_text(encoding="utf-8")
        source = (BOARD_DIR / "smart_home_controller.cc").read_text(encoding="utf-8")
        board = (BOARD_DIR / "compact_wifi_board.cc").read_text(encoding="utf-8")
        http = (BOARD_DIR / "smart_home_http_server.cc").read_text(encoding="utf-8")

        for symbol in [
            "StateGuard", "xSemaphoreCreateRecursiveMutex", "BuildHealthJson",
            "UpdateSensorHealth", "UpdateRadarHealth", "kManualOverrideDurationMs",
            "kAlarmConfirmationSamples", "sample_time_ms",
        ]:
            self.assertIn(symbol, header + source)
        self.assertIn("DHT11_CACHE_MAX_AGE_MS", board)
        self.assertIn('"/api/health"', http)
        self.assertIn("X-API-Key", http)
        self.assertIn("SMART_HOME_API_TOKEN", http)

    def test_ld2450_sensor_exposes_wiring_diagnostics(self):
        header = (BOARD_DIR / "ld2450_sensor.h").read_text(encoding="utf-8")
        source = (BOARD_DIR / "ld2450_sensor.cc").read_text(encoding="utf-8")
        board = (BOARD_DIR / "compact_wifi_board.cc").read_text(encoding="utf-8")

        for symbol in ["GetReceivedByteCount", "GetValidFrameCount", "GetRejectedFrameCount"]:
            self.assertIn(symbol, header)
        self.assertIn("received_byte_count_", source)
        self.assertIn("valid_frame_count_", source)
        self.assertIn("rejected_frame_count_", source)
        self.assertIn("LD2450 stats", board)

    def test_demo_enhancements_expose_rules_scenes_events_radar_and_offline_mode(self):
        header = (BOARD_DIR / "smart_home_controller.h").read_text(encoding="utf-8")
        source = (BOARD_DIR / "smart_home_controller.cc").read_text(encoding="utf-8")
        http = (BOARD_DIR / "smart_home_http_server.cc").read_text(encoding="utf-8")
        mini_dir = ROOT.parent / "mini_program_demo" / "pages" / "index"
        js = (mini_dir / "index.js").read_text(encoding="utf-8")
        wxml = (mini_dir / "index.wxml").read_text(encoding="utf-8")
        wxss = (mini_dir / "index.wxss").read_text(encoding="utf-8")

        for symbol in [
            "AutomationRuleConfig", "SmartHomeEvent", "SetAutomationRule",
            "ApplyScene", "BuildEventsJson", "EvaluateAutomationRule",
        ]:
            self.assertIn(symbol, header + source)
        for endpoint in ['"/api/events"', '"/api/automation"', '"/api/scene"']:
            self.assertIn(endpoint, http)
        for symbol in [
            "toggleDemoMode", "drawTrendChart", "setScene", "saveAutomationRule",
            "initializeDemoData", '"/api/events"',
        ]:
            self.assertIn(symbol, js)
        for binding in [
            'bindchange="toggleDemoMode"', 'bindtap="setScene"',
            'bindtap="saveAutomationRule"', 'id="trendCanvas"',
        ]:
            self.assertIn(binding, wxml)
        for style in [".radar-map", ".trend-canvas", ".scene-grid", ".event-row"]:
            self.assertIn(style, wxss)

    def test_ld2450_initialization_does_not_stop_target_reporting(self):
        source = (BOARD_DIR / "ld2450_sensor.cc").read_text(encoding="utf-8")
        initialize_body = source.split("bool Ld2450Sensor::Initialize()", 1)[1].split(
            "bool Ld2450Sensor::Poll", 1
        )[0]

        self.assertNotIn("enable_cmd", initialize_body)
        self.assertNotIn("0xFF, 0x00", initialize_body)
        self.assertNotIn("uart_write_bytes", initialize_body)


if __name__ == "__main__":
    unittest.main()
