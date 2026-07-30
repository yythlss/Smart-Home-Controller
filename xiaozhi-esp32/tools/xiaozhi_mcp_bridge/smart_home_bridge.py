"""Xiaozhi MCP bridge for external information and optional ESP32 compatibility.

Run this file through the Xiaozhi mcp_pipe.py bridge from:
https://github.com/78/mcp-calculator
"""

from __future__ import annotations

import json
import logging
import os
import sys
from functools import wraps
from typing import Any
from urllib.error import HTTPError, URLError
from urllib.parse import urlencode
from urllib.request import Request, urlopen
from xml.etree import ElementTree

try:
    from mcp.server.fastmcp import FastMCP
except ImportError:  # Tests and static checks should not require MCP installed.
    FastMCP = None


class _MissingFastMcp:
    def __init__(self, name: str):
        self.name = name

    def tool(self):
        def decorator(func):
            return func

        return decorator

    def run(self, transport: str = "stdio"):
        raise RuntimeError("Python package 'mcp' is required. Run: pip install -r tools/xiaozhi_mcp_bridge/requirements.txt")


logger = logging.getLogger("xiaozhi_mcp_bridge")
if not logger.handlers:
    handler = logging.StreamHandler(sys.stderr)
    handler.setFormatter(logging.Formatter("%(asctime)s %(levelname)s %(message)s"))
    logger.addHandler(handler)
log_level = os.environ.get("XIAOZHI_MCP_LOG_LEVEL", "INFO").strip().upper()
logger.setLevel(getattr(logging, log_level, logging.INFO))


class BridgeError(RuntimeError):
    def __init__(self, code: str, message: str, details: dict[str, Any] | None = None):
        super().__init__(message)
        self.code = code
        self.message = message
        self.details = details or {}


def _bridge_mode() -> str:
    mode = os.environ.get("XIAOZHI_MCP_BRIDGE_MODE", "external").strip().lower()
    if mode not in {"external", "full"}:
        logger.warning("Unknown bridge mode %r; using external", mode)
        return "external"
    return mode


mcp = FastMCP("xiaozhi-external-services") if FastMCP else _MissingFastMcp("xiaozhi-external-services")
REGISTERED_TOOL_NAMES: list[str] = []


def _error_result(tool_name: str, error: BridgeError) -> dict[str, Any]:
    result: dict[str, Any] = {
        "ok": False,
        "tool": tool_name,
        "error": {
            "code": error.code,
            "message": error.message,
        },
    }
    if error.details:
        result["error"]["details"] = error.details
    return result


def _tool(*, compatibility: bool = False):
    def decorator(func):
        @wraps(func)
        def safe_tool(*args, **kwargs):
            logger.info("tool_call name=%s", func.__name__)
            try:
                return func(*args, **kwargs)
            except BridgeError as exc:
                logger.warning("tool_error name=%s code=%s message=%s",
                               func.__name__, exc.code, exc.message)
                return _error_result(func.__name__, exc)
            except Exception as exc:  # Keep stdio MCP alive after one failed dependency call.
                logger.exception("tool_error name=%s code=internal_error", func.__name__)
                return _error_result(func.__name__, BridgeError(
                    "internal_error", "Bridge tool failed", {"reason": str(exc)}))

        if not compatibility or _bridge_mode() == "full":
            REGISTERED_TOOL_NAMES.append(func.__name__)
            return mcp.tool()(safe_tool)
        return safe_tool

    return decorator


def _base_url() -> str:
    base_url = os.environ.get("ESP32_BASE_URL", "").strip()
    if not base_url:
        raise BridgeError("config_missing",
                          "ESP32_BASE_URL is required for indoor data compatibility")
    if not base_url.startswith(("http://", "https://")):
        base_url = f"http://{base_url}"
    return base_url.rstrip("/")


def _timeout_seconds() -> float:
    value = os.environ.get("ESP32_HTTP_TIMEOUT_SECONDS", "5").strip()
    try:
        timeout = float(value)
    except ValueError:
        timeout = 5.0
    return max(1.0, min(timeout, 30.0))


def _request_json(path: str, method: str = "GET", payload: dict[str, Any] | None = None) -> dict[str, Any]:
    url = f"{_base_url()}{path}"
    body = None
    headers = {"Accept": "application/json"}
    api_token = os.environ.get("ESP32_API_TOKEN", "").strip()
    if api_token:
        headers["X-API-Key"] = api_token
    if payload is not None:
        body = json.dumps(payload, ensure_ascii=False).encode("utf-8")
        headers["Content-Type"] = "application/json"

    request = Request(url, data=body, headers=headers, method=method)
    try:
        with urlopen(request, timeout=_timeout_seconds()) as response:
            raw = response.read()
    except HTTPError as exc:
        detail = exc.read().decode("utf-8", errors="replace")
        raise BridgeError("esp32_http_error", f"ESP32 HTTP request failed with {exc.code}", {
            "status": exc.code,
            "path": path,
            "detail": detail[:512],
        }) from exc
    except URLError as exc:
        raise BridgeError("esp32_unreachable", "Cannot reach ESP32 HTTP API", {
            "path": path,
            "reason": str(exc.reason),
        }) from exc

    if not raw:
        return {}
    try:
        return json.loads(raw.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise BridgeError("esp32_invalid_response", "ESP32 returned invalid JSON", {
            "path": path,
        }) from exc


def _request_external_text(url: str) -> str:
    request = Request(url, headers={
        "Accept": "application/json, application/rss+xml, application/xml, text/xml",
        "User-Agent": "xiaozhi-smart-home/1.0",
    })
    try:
        with urlopen(request, timeout=10.0) as response:
            return response.read().decode("utf-8", errors="replace")
    except HTTPError as exc:
        raise BridgeError("external_http_error",
                          f"External service returned HTTP {exc.code}", {
                              "status": exc.code,
                              "url": url,
                          }) from exc
    except URLError as exc:
        raise BridgeError("external_unreachable", "Cannot reach external service", {
            "url": url,
            "reason": str(exc.reason),
        }) from exc


def _request_external_json(url: str) -> dict[str, Any]:
    try:
        return json.loads(_request_external_text(url))
    except json.JSONDecodeError as exc:
        raise BridgeError("external_invalid_response",
                          "External service returned invalid JSON", {"url": url}) from exc


def _weather_code_text(code: int) -> str:
    if code == 0:
        return "晴"
    if code in (1, 2, 3):
        return "多云"
    if code in (45, 48):
        return "雾"
    if code in (51, 53, 55, 56, 57):
        return "毛毛雨"
    if code in (61, 63, 65, 66, 67, 80, 81, 82):
        return "阵雨"
    if code in (71, 73, 75, 77, 85, 86):
        return "雪"
    if code in (95, 96, 99):
        return "雷雨"
    return "天气代码未知"


def _first(values: Any, default: Any = None) -> Any:
    return values[0] if isinstance(values, list) and values else default


def _require_tool_data(result: dict[str, Any]) -> dict[str, Any]:
    if result.get("ok") is not False or not isinstance(result.get("error"), dict):
        return result
    error = result["error"]
    raise BridgeError(
        f"dependency_{error.get('code', 'error')}",
        str(error.get("message", "Dependent bridge tool failed")),
        {"dependency": result.get("tool", "unknown")},
    )


def _level_value(power: bool, level: int) -> int:
    if not power:
        return 0
    level = max(0, min(3, int(level)))
    return 1 if level == 0 else level


def _set_device(device: str, power: bool, level: int = 1) -> dict[str, Any]:
    return _request_json("/api/device", "POST", {
        "device": device,
        "power": bool(power),
        "level": _level_value(bool(power), level),
    })


def _set_mode(mode: str, power: bool) -> dict[str, Any]:
    return _request_json("/api/mode", "POST", {
        "mode": mode,
        "power": bool(power),
    })


@_tool(compatibility=True)
def home_get_state() -> dict[str, Any]:
    """获取当前空气状态和智能家居设备状态。"""

    return _request_json("/api/state")


@_tool(compatibility=True)
def home_get_health() -> dict[str, Any]:
    """获取固件版本、运行时间、内存、Wi-Fi 和传感器健康状态。"""

    return _request_json("/api/health")


@_tool(compatibility=True)
def home_set_purifier(power: bool, level: int = 1) -> dict[str, Any]:
    """控制净化器红色 LED。level 为 0-3，0 表示关闭。"""

    return _set_device("purifier", power, level)


@_tool(compatibility=True)
def home_set_fresh_air(power: bool, level: int = 1) -> dict[str, Any]:
    """控制新风/风扇舵机。level 为 0-3，0 表示关闭。"""

    return _set_device("fresh_air", power, level)


@_tool(compatibility=True)
def home_set_humidifier(power: bool, level: int = 1) -> dict[str, Any]:
    """控制加湿器蓝色 LED。level 为 0-3，0 表示关闭。"""

    return _set_device("humidifier", power, level)


@_tool(compatibility=True)
def home_set_light(power: bool) -> dict[str, Any]:
    """控制照明灯开关。"""

    return _set_device("light", power, 1)


@_tool(compatibility=True)
def home_set_auto(power: bool) -> dict[str, Any]:
    """开启或关闭自动模式。开启自动模式会退出节能模式。"""

    return _set_mode("auto", power)


@_tool(compatibility=True)
def home_set_eco(power: bool) -> dict[str, Any]:
    """开启或关闭节能模式。节能模式按环境使用较低档位，无人时关闭设备。"""

    return _set_mode("eco", power)


@_tool(compatibility=True)
def home_update_context(occupied: bool, ambient_light_percent: float) -> dict[str, Any]:
    """更新雷达占用状态和环境亮度，用于新硬件接入前联调。"""

    return _request_json("/api/context", "POST", {
        "occupied": bool(occupied),
        "ambient_light_percent": max(0.0, min(100.0, float(ambient_light_percent))),
    })


@_tool(compatibility=True)
def home_acknowledge_alarm() -> dict[str, Any]:
    """确认并清除环境突变报警。"""

    return _request_json("/api/alarm/ack", "POST", {})


@_tool(compatibility=True)
def home_set_environment_preset(preset: str) -> dict[str, Any]:
    """设置手动环境预设。preset 可用 GOOD、HOT、DRY、POLLUTED。"""

    return _request_json("/api/environment", "POST", {
        "enabled": True,
        "preset": preset,
    })


@_tool(compatibility=True)
def home_set_manual_environment(temperature_c: float, humidity_percent: float, air_score: int) -> dict[str, Any]:
    """手动输入环境数据，用于测试自动模式和环境建议。"""

    return _request_json("/api/environment", "POST", {
        "enabled": True,
        "temperature_c": float(temperature_c),
        "humidity_percent": float(humidity_percent),
        "air_score": int(air_score),
    })


@_tool(compatibility=True)
def home_disable_manual_environment() -> dict[str, Any]:
    """退出手动环境模式，恢复真实传感器数据。"""

    return _request_json("/api/environment", "POST", {
        "enabled": False,
    })


@_tool(compatibility=True)
def home_get_advice() -> dict[str, Any]:
    """获取当前舒适度和环境建议。"""

    state = _require_tool_data(home_get_state())
    return {
        "air_state": state.get("air_state"),
        "comfort": state.get("comfort"),
        "advice": state.get("advice"),
        "temperature_c": state.get("temperature_c"),
        "humidity_percent": state.get("humidity_percent"),
        "air_score": state.get("air_score"),
        "manual_environment_mode": state.get("manual_environment_mode"),
        "environment_source": state.get("environment_source"),
    }


@_tool()
def home_get_weather(city: str) -> dict[str, Any]:
    """查询指定城市当前天气和今日温度、降雨概率，不需要天气 API 密钥。"""

    city = city.strip()
    if not city:
        raise BridgeError("invalid_argument", "city is required")

    geocoding_url = "https://geocoding-api.open-meteo.com/v1/search?" + urlencode({
        "name": city,
        "count": 1,
        "language": "zh",
        "format": "json",
    })
    geocoding = _request_external_json(geocoding_url)
    results = geocoding.get("results") or []
    if not results:
        raise BridgeError("not_found", f"Weather city not found: {city}")

    location = results[0]
    forecast_url = "https://api.open-meteo.com/v1/forecast?" + urlencode({
        "latitude": location["latitude"],
        "longitude": location["longitude"],
        "current": "temperature_2m,weather_code,wind_speed_10m",
        "daily": "temperature_2m_max,temperature_2m_min,precipitation_probability_max",
        "timezone": "auto",
        "forecast_days": 1,
    })
    forecast = _request_external_json(forecast_url)
    current = forecast.get("current") or {}
    daily = forecast.get("daily") or {}
    weather_code = int(current.get("weather_code", -1))

    return {
        "city": location.get("name", city),
        "country": location.get("country", ""),
        "weather": _weather_code_text(weather_code),
        "weather_code": weather_code,
        "temperature_c": current.get("temperature_2m"),
        "wind_speed_kmh": current.get("wind_speed_10m"),
        "temperature_max_c": _first(daily.get("temperature_2m_max")),
        "temperature_min_c": _first(daily.get("temperature_2m_min")),
        "precipitation_probability_percent": _first(daily.get("precipitation_probability_max"), 0),
        "source": "Open-Meteo",
    }


@_tool()
def home_get_news(limit: int = 5) -> dict[str, Any]:
    """读取 NEWS_RSS_URL 配置的新闻 RSS 标题，供 AI 简短播报。"""

    rss_url = os.environ.get("NEWS_RSS_URL", "").strip()
    if not rss_url:
        raise BridgeError("config_missing",
                          "NEWS_RSS_URL is required; set it to a trusted RSS feed URL")
    limit = max(1, min(10, int(limit)))

    try:
        root = ElementTree.fromstring(_request_external_text(rss_url))
    except ElementTree.ParseError as exc:
        raise BridgeError("external_invalid_response", "News service returned invalid XML", {
            "url": rss_url,
        }) from exc
    items: list[dict[str, str]] = []
    for item in root.findall(".//item"):
        title = (item.findtext("title") or "").strip()
        link = (item.findtext("link") or "").strip()
        if title:
            items.append({"title": title, "link": link})
        if len(items) >= limit:
            break

    if not items:
        atom_ns = {"atom": "http://www.w3.org/2005/Atom"}
        for entry in root.findall(".//atom:entry", atom_ns):
            title = (entry.findtext("atom:title", default="", namespaces=atom_ns) or "").strip()
            link_node = entry.find("atom:link", atom_ns)
            link = link_node.get("href", "") if link_node is not None else ""
            if title:
                items.append({"title": title, "link": link})
            if len(items) >= limit:
                break

    return {"count": len(items), "items": items, "source": rss_url}


@_tool()
def home_get_combined_advice(city: str) -> dict[str, Any]:
    """结合室内环境和指定城市天气生成可直接语音播报的生活建议。"""

    indoor = _require_tool_data(home_get_state())
    weather = _require_tool_data(home_get_weather(city))
    advice: list[str] = []

    air_score = indoor.get("air_score")
    humidity = indoor.get("humidity_percent")
    indoor_temperature = indoor.get("temperature_c")
    rain_probability = weather.get("precipitation_probability_percent") or 0

    if isinstance(air_score, (int, float)) and air_score < 40:
        advice.append("室内空气较差，建议开启净化和新风")
    elif isinstance(air_score, (int, float)) and air_score < 65:
        advice.append("室内空气一般，建议保持通风")
    if isinstance(humidity, (int, float)) and humidity < 35:
        advice.append("室内偏干，建议开启加湿")
    elif isinstance(humidity, (int, float)) and humidity > 75:
        advice.append("室内偏湿，建议适当除湿")
    if isinstance(indoor_temperature, (int, float)) and indoor_temperature > 30:
        advice.append("室内偏热，建议开启空调降温")
    elif isinstance(indoor_temperature, (int, float)) and indoor_temperature < 18:
        advice.append("室内偏冷，建议注意保暖")
    if isinstance(rain_probability, (int, float)) and rain_probability >= 60:
        advice.append("今天降雨概率较高，外出记得带伞")
    if not advice:
        advice.append("室内外环境总体平稳，可保持当前设置")

    return {
        "indoor": indoor,
        "weather": weather,
        "advice": "；".join(advice) + "。",
    }


if __name__ == "__main__":
    mcp.run(transport="stdio")
