import importlib
import json
import os
import unittest
from unittest.mock import patch


class FakeResponse:
    def __init__(self, payload):
        self.payload = payload

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc, tb):
        return False

    def read(self):
        return json.dumps(self.payload).encode("utf-8")


class XiaozhiMcpBridgeTest(unittest.TestCase):
    def setUp(self):
        os.environ["ESP32_BASE_URL"] = "http://192.168.1.23:8080/"
        self.bridge = importlib.import_module("tools.xiaozhi_mcp_bridge.smart_home_bridge")

    def tearDown(self):
        os.environ.pop("ESP32_BASE_URL", None)

    def capture_request(self, response_payload=None):
        calls = []

        def fake_urlopen(request, timeout=0):
            body = request.data.decode("utf-8") if request.data else None
            calls.append({
                "url": request.full_url,
                "method": request.get_method(),
                "body": json.loads(body) if body else None,
                "timeout": timeout,
                "content_type": request.headers.get("Content-type"),
            })
            return FakeResponse(response_payload or {"ok": True})

        return calls, fake_urlopen

    def test_home_get_state_reads_esp32_state_endpoint(self):
        calls, fake_urlopen = self.capture_request({"purifier_level": 0})

        with patch.object(self.bridge, "urlopen", fake_urlopen):
            result = self.bridge.home_get_state()

        self.assertEqual({"purifier_level": 0}, result)
        self.assertEqual("http://192.168.1.23:8080/api/state", calls[0]["url"])
        self.assertEqual("GET", calls[0]["method"])
        self.assertIsNone(calls[0]["body"])

    def test_home_set_fresh_air_posts_device_payload(self):
        calls, fake_urlopen = self.capture_request({"fresh_air_level": 2})

        with patch.object(self.bridge, "urlopen", fake_urlopen):
            result = self.bridge.home_set_fresh_air(True, 2)

        self.assertEqual({"fresh_air_level": 2}, result)
        self.assertEqual("http://192.168.1.23:8080/api/device", calls[0]["url"])
        self.assertEqual("POST", calls[0]["method"])
        self.assertEqual({"device": "fresh_air", "power": True, "level": 2}, calls[0]["body"])
        self.assertEqual("application/json", calls[0]["content_type"])

    def test_home_set_auto_posts_mode_payload(self):
        calls, fake_urlopen = self.capture_request({"auto_mode": True})

        with patch.object(self.bridge, "urlopen", fake_urlopen):
            result = self.bridge.home_set_auto(True)

        self.assertEqual({"auto_mode": True}, result)
        self.assertEqual("http://192.168.1.23:8080/api/mode", calls[0]["url"])
        self.assertEqual({"mode": "auto", "power": True}, calls[0]["body"])

    def test_home_set_environment_preset_posts_environment_payload(self):
        calls, fake_urlopen = self.capture_request({"manual_environment_mode": True})

        with patch.object(self.bridge, "urlopen", fake_urlopen):
            result = self.bridge.home_set_environment_preset("POLLUTED")

        self.assertEqual({"manual_environment_mode": True}, result)
        self.assertEqual("http://192.168.1.23:8080/api/environment", calls[0]["url"])
        self.assertEqual({"enabled": True, "preset": "POLLUTED"}, calls[0]["body"])

    def test_missing_esp32_base_url_returns_clear_error(self):
        os.environ.pop("ESP32_BASE_URL", None)

        with self.assertRaisesRegex(RuntimeError, "ESP32_BASE_URL"):
            self.bridge.home_get_state()

    def test_home_set_light_posts_device_payload(self):
        calls, fake_urlopen = self.capture_request({"light_on": True})

        with patch.object(self.bridge, "urlopen", fake_urlopen):
            result = self.bridge.home_set_light(True)

        self.assertEqual({"light_on": True}, result)
        self.assertEqual("http://192.168.1.23:8080/api/device", calls[0]["url"])
        self.assertEqual({"device": "light", "power": True, "level": 1}, calls[0]["body"])

    def test_home_update_context_posts_presence_and_light(self):
        calls, fake_urlopen = self.capture_request({"occupied": True})

        with patch.object(self.bridge, "urlopen", fake_urlopen):
            result = self.bridge.home_update_context(True, 18.0)

        self.assertEqual({"occupied": True}, result)
        self.assertEqual("http://192.168.1.23:8080/api/context", calls[0]["url"])
        self.assertEqual({"occupied": True, "ambient_light_percent": 18.0}, calls[0]["body"])

    def test_home_get_weather_returns_current_and_daily_summary(self):
        geocoding = {
            "results": [{"name": "杭州", "latitude": 30.27, "longitude": 120.15, "country": "中国"}]
        }
        forecast = {
            "current": {"temperature_2m": 31.2, "weather_code": 3, "wind_speed_10m": 8.0},
            "daily": {"temperature_2m_max": [34.0], "temperature_2m_min": [26.0], "precipitation_probability_max": [70]},
        }

        with patch.object(self.bridge, "_request_external_json", side_effect=[geocoding, forecast]):
            result = self.bridge.home_get_weather("杭州")

        self.assertEqual("杭州", result["city"])
        self.assertEqual(31.2, result["temperature_c"])
        self.assertEqual(70, result["precipitation_probability_percent"])
        self.assertIn("多云", result["weather"])

    def test_home_get_news_parses_configured_rss(self):
        os.environ["NEWS_RSS_URL"] = "https://example.com/news.xml"
        rss = """<?xml version="1.0" encoding="UTF-8"?>
        <rss><channel><item><title>新闻一</title><link>https://example.com/1</link></item>
        <item><title>新闻二</title><link>https://example.com/2</link></item></channel></rss>"""

        try:
            with patch.object(self.bridge, "_request_external_text", return_value=rss):
                result = self.bridge.home_get_news(1)
        finally:
            os.environ.pop("NEWS_RSS_URL", None)

        self.assertEqual(1, result["count"])
        self.assertEqual("新闻一", result["items"][0]["title"])

    def test_home_get_combined_advice_uses_indoor_and_weather_data(self):
        indoor = {"temperature_c": 32.0, "humidity_percent": 30.0, "air_score": 35, "occupied": True}
        weather = {"temperature_c": 35.0, "precipitation_probability_percent": 80, "weather": "阵雨"}

        with patch.object(self.bridge, "home_get_state", return_value=indoor), patch.object(
            self.bridge, "home_get_weather", return_value=weather
        ):
            result = self.bridge.home_get_combined_advice("杭州")

        self.assertIn("净化", result["advice"])
        self.assertIn("加湿", result["advice"])
        self.assertIn("降温", result["advice"])
        self.assertIn("带伞", result["advice"])


if __name__ == "__main__":
    unittest.main()
