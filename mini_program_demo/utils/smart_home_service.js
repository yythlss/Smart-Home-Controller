const presetNames = {
  GOOD: "舒适",
  HOT: "高温",
  DRY: "干燥",
  POLLUTED: "污染"
};

const sceneNames = {
  HOME: "回家",
  AWAY: "离家",
  SLEEP: "睡眠",
  VENTILATE: "通风",
  CLEAN: "强力净化"
};

const deviceNames = {
  purifier: "净化器",
  fresh_air: "新风",
  humidifier: "加湿器",
  light: "灯光"
};

let demoState = null;
let demoHistory = [];
let demoEvents = [];
let demoStart = 0;
let demoTick = 0;

function clamp(value, minimum, maximum) {
  return Math.max(minimum, Math.min(maximum, value));
}

function clone(value) {
  return JSON.parse(JSON.stringify(value));
}

function getConfig() {
  return {
    host: wx.getStorageSync("esp32_host") || "",
    apiToken: wx.getStorageSync("esp32_api_token") || "",
    demoMode: wx.getStorageSync("smart_home_demo_mode") === true
  };
}

function saveConfig(host, apiToken) {
  wx.setStorageSync("esp32_host", String(host || "").trim());
  wx.setStorageSync("esp32_api_token", String(apiToken || "").trim());
}

function setDemoMode(enabled) {
  wx.setStorageSync("smart_home_demo_mode", Boolean(enabled));
  if (enabled) initializeDemoData();
  else demoState = null;
}

function apiBase(host) {
  const value = String(host || "").trim();
  if (value.startsWith("http://") || value.startsWith("https://")) return value;
  return `http://${value}`;
}

function request(path, method = "GET", data = undefined) {
  const config = getConfig();
  if (config.demoMode) return Promise.resolve(demoRequest(path, method, data || {}));
  if (!config.host) return Promise.reject(new Error("尚未在操作后台配置 ESP32 地址"));
  return new Promise((resolve, reject) => {
    wx.request({
      url: `${apiBase(config.host)}${path}`,
      method,
      data,
      header: {
        "content-type": "application/json",
        ...(config.apiToken ? { "X-API-Key": config.apiToken } : {})
      },
      success: (response) => {
        if (response.statusCode >= 200 && response.statusCode < 300) resolve(response.data);
        else reject(new Error(`HTTP ${response.statusCode}`));
      },
      fail: reject
    });
  });
}

function initializeDemoData() {
  demoStart = Date.now();
  demoTick = 0;
  demoEvents = [{ timestamp_ms: 0, type: "system", source: "demo", message: "离线演示系统已启动" }];
  demoHistory = [];
  demoState = {
    purifier_level: 0,
    fresh_air_level: 0,
    humidifier_level: 0,
    auto_mode: false,
    eco_mode: false,
    active_scene: "custom",
    manual_environment_mode: false,
    occupancy_known: true,
    occupied: true,
    has_ambient_light: true,
    ambient_light_percent: 22,
    light_on: true,
    has_radar_data: true,
    radar_target_count: 1,
    has_radar_position: true,
    radar_nearest_x_mm: 0,
    radar_nearest_y_mm: 1800,
    radar_nearest_speed_mm_per_s: 120,
    radar_zone: "center",
    alarm_active: false,
    alarm_reason: "",
    environment_source: "demo",
    sample_time_ms: 0,
    cached_temperature_humidity: false,
    has_temperature: true,
    temperature_c: 26,
    has_humidity: true,
    humidity_percent: 55,
    has_mq135_raw: true,
    mq135_raw: 720,
    air_score: 88,
    air_state: "优",
    comfort: "舒适",
    advice: "环境舒适",
    purifier_override_remaining_seconds: 0,
    fresh_air_override_remaining_seconds: 0,
    humidifier_override_remaining_seconds: 0,
    light_override_remaining_seconds: 0,
    automation_rule: {
      enabled: false,
      active: false,
      air_score_below: 60,
      humidity_below: 35,
      temperature_above: 30,
      purifier_level: 3,
      fresh_air_level: 2,
      humidifier_level: 2
    }
  };
  for (let index = 12; index > 0; index -= 1) {
    demoHistory.push({
      sample_time_ms: -index * 5000,
      has_temperature: true,
      temperature_c: 25.5 + Math.sin(index / 3) * 0.8,
      has_humidity: true,
      humidity_percent: 54 + Math.cos(index / 4) * 3,
      has_mq135_raw: true,
      mq135_raw: 700,
      air_score: 84 + Math.round(Math.sin(index / 2) * 4),
      environment_source: "demo",
      comfort: "舒适",
      advice: "环境舒适"
    });
  }
}

function demoUptime() {
  return Math.max(0, Date.now() - demoStart);
}

function addDemoEvent(type, source, message) {
  demoEvents.push({ timestamp_ms: demoUptime(), type, source, message });
  demoEvents = demoEvents.slice(-32);
}

function applyDemoAutomation() {
  const state = demoState;
  const rule = state.automation_rule;
  const active = Boolean(rule.enabled && state.auto_mode && (
    state.air_score < rule.air_score_below ||
    state.humidity_percent < rule.humidity_below ||
    state.temperature_c > rule.temperature_above
  ));
  if (active && !rule.active) addDemoEvent("automation", "system", "环境达到阈值，自动化规则已执行");
  else if (!active && rule.active) addDemoEvent("automation", "system", "环境恢复，自动化规则已解除");
  rule.active = active;
  if (!active) return;
  if (state.air_score < rule.air_score_below) {
    state.purifier_level = Math.max(state.purifier_level, rule.purifier_level);
    state.fresh_air_level = Math.max(state.fresh_air_level, rule.fresh_air_level);
  }
  if (state.humidity_percent < rule.humidity_below) {
    state.humidifier_level = Math.max(state.humidifier_level, rule.humidifier_level);
  }
  if (state.temperature_c > rule.temperature_above) {
    state.fresh_air_level = Math.max(state.fresh_air_level, rule.fresh_air_level);
  }
}

function advanceDemoData() {
  if (!getConfig().demoMode) return;
  if (!demoState) initializeDemoData();
  demoTick += 1;
  const state = demoState;
  state.sample_time_ms = demoUptime();
  state.radar_nearest_x_mm = Math.round(Math.sin(demoTick / 2) * 2200);
  state.radar_nearest_y_mm = 1600 + Math.round((Math.cos(demoTick / 3) + 1) * 900);
  state.radar_nearest_speed_mm_per_s = Math.round(Math.cos(demoTick / 2) * 260);
  state.radar_zone = state.radar_nearest_x_mm <= -500 ? "left" : (state.radar_nearest_x_mm >= 500 ? "right" : "center");
  if (!state.manual_environment_mode) {
    state.temperature_c = Number((26 + Math.sin(demoTick / 3) * 1.2).toFixed(1));
    state.humidity_percent = Number((54 + Math.cos(demoTick / 4) * 4).toFixed(1));
    state.air_score = clamp(Math.round(state.air_score + (state.purifier_level > 0 ? 3 : Math.sin(demoTick) * 2)), 25, 95);
    state.mq135_raw = Math.round((100 - state.air_score) * 30);
  }
  applyDemoAutomation();
  demoHistory.push({
    sample_time_ms: state.sample_time_ms,
    has_temperature: true,
    temperature_c: state.temperature_c,
    has_humidity: true,
    humidity_percent: state.humidity_percent,
    has_mq135_raw: true,
    mq135_raw: state.mq135_raw,
    air_score: state.air_score,
    environment_source: state.environment_source,
    comfort: state.comfort,
    advice: state.advice
  });
  demoHistory = demoHistory.slice(-30);
}

function applyDemoScene(scene) {
  const settings = {
    HOME: { active_scene: "home", occupied: true, auto_mode: true, eco_mode: false, purifier_level: 1, fresh_air_level: 1, humidifier_level: 0, light_on: true },
    AWAY: { active_scene: "away", occupied: false, auto_mode: false, eco_mode: true, purifier_level: 0, fresh_air_level: 0, humidifier_level: 0, light_on: false },
    SLEEP: { active_scene: "sleep", occupied: true, auto_mode: false, eco_mode: true, purifier_level: 1, fresh_air_level: 0, humidifier_level: 1, light_on: false },
    VENTILATE: { active_scene: "ventilate", auto_mode: false, eco_mode: false, purifier_level: 1, fresh_air_level: 3, humidifier_level: 0 },
    CLEAN: { active_scene: "clean", auto_mode: false, eco_mode: false, purifier_level: 3, fresh_air_level: 2, humidifier_level: 0 }
  };
  Object.assign(demoState, settings[scene] || {});
  addDemoEvent("scene", "manual", `已切换到${sceneNames[scene] || scene}场景`);
}

function demoRequest(path, method, data) {
  if (!demoState) initializeDemoData();
  const state = demoState;
  if (path === "/api/state") return clone(state);
  if (path === "/api/history") return { count: demoHistory.length, capacity: 30, samples: clone(demoHistory) };
  if (path === "/api/events") return { count: demoEvents.length, capacity: 32, events: clone(demoEvents) };
  if (path === "/api/health") {
    return {
      uptime_ms: demoUptime(), free_heap_bytes: 196608, firmware_version: "demo-2.0",
      network_connected: true, hmi_initialized: true, api_auth_enabled: false,
      has_wifi_rssi: true, wifi_rssi_dbm: -48, dht_current_ok: true, dht_stale: false,
      dht_consecutive_failures: 0, mq135_stale: false, mq135_consecutive_failures: 0,
      ambient_light_stale: false, ambient_light_consecutive_failures: 0,
      radar_stale: false, radar_valid_frames: 128 + demoTick
    };
  }
  if (method !== "POST") return clone(state);
  if (path === "/api/device") {
    const key = `${data.device}_level`;
    if (key in state) state[key] = data.power ? clamp(Number(data.level), 0, 3) : 0;
    if (data.device === "light") state.light_on = Boolean(data.power);
    state.active_scene = "custom";
    addDemoEvent("device", "manual", `${deviceNames[data.device] || data.device}切换到${Number(data.level) || 0}档`);
  } else if (path === "/api/mode") {
    state[`${data.mode}_mode`] = Boolean(data.power);
    if (data.power) state[`${data.mode === "auto" ? "eco" : "auto"}_mode`] = false;
    state.active_scene = "custom";
    addDemoEvent("mode", "manual", `${data.mode === "auto" ? "自动" : "节能"}模式${data.power ? "已开启" : "已关闭"}`);
    applyDemoAutomation();
  } else if (path === "/api/scene") {
    applyDemoScene(data.scene);
  } else if (path === "/api/automation") {
    state.automation_rule = { ...state.automation_rule, ...data, active: false };
    addDemoEvent("automation", "manual", data.enabled ? "自定义自动化规则已启用" : "自定义自动化规则已停用");
    applyDemoAutomation();
  } else if (path === "/api/environment") {
    if (!data.enabled) {
      state.manual_environment_mode = false;
      state.environment_source = "demo";
      addDemoEvent("environment", "manual", "已切换到传感器读取系统");
    } else {
      const presets = { GOOD: [26, 55, 88], HOT: [33, 58, 72], DRY: [25, 28, 76], POLLUTED: [27, 60, 28] };
      const values = data.preset ? presets[data.preset] : [data.temperature_c, data.humidity_percent, data.air_score];
      state.temperature_c = Number(values[0]);
      state.humidity_percent = Number(values[1]);
      state.air_score = Number(values[2]);
      state.mq135_raw = Math.round((100 - state.air_score) * 30);
      state.manual_environment_mode = true;
      state.environment_source = "manual";
      state.air_state = state.air_score >= 85 ? "优" : (state.air_score >= 65 ? "良" : (state.air_score >= 40 ? "一般" : "差"));
      state.comfort = state.air_score < 40 ? "空气较差" : (state.humidity_percent < 35 ? "偏干" : (state.temperature_c > 30 ? "偏热" : "舒适"));
      state.advice = state.air_score < 40 ? "开净化器和新风" : (state.humidity_percent < 35 ? "开加湿器" : (state.temperature_c > 30 ? "开新风降温" : "环境舒适"));
      addDemoEvent("environment", "manual", data.preset ? `${presetNames[data.preset]}环境已模拟` : "手动传感器数值已更新");
      applyDemoAutomation();
    }
  } else if (path === "/api/alarm/ack") {
    state.alarm_active = false;
    state.alarm_reason = "";
    addDemoEvent("alarm", "manual", "当前告警已确认");
  }
  return clone(state);
}

module.exports = {
  getConfig,
  saveConfig,
  setDemoMode,
  request,
  advanceDemoData
};
