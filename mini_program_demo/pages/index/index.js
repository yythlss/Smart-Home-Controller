const deviceNames = {
  purifier: "净化",
  fresh_air: "新风",
  humidifier: "加湿",
  light: "灯光"
};

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

const sceneValueNames = {
  custom: "自定义",
  home: "回家",
  away: "离家",
  sleep: "睡眠",
  ventilate: "通风",
  clean: "强力净化"
};

function clamp(value, minimum, maximum) {
  return Math.max(minimum, Math.min(maximum, value));
}

function clone(value) {
  return JSON.parse(JSON.stringify(value));
}

Page({
  data: {
    host: "192.168.1.100:8080",
    apiToken: "",
    demoMode: false,
    state: null,
    health: null,
    devices: [],
    samples: [],
    events: [],
    radar: {
      visible: false,
      left: 50,
      top: 86,
      label: "等待目标"
    },
    message: "请先填写 ESP32 的局域网 IP",
    loading: false,
    levels: [0, 1, 2, 3],
    manualForm: {
      temperature_c: "30",
      humidity_percent: "35",
      air_score: "45"
    },
    ruleForm: {
      enabled: false,
      air_score_below: "60",
      humidity_below: "35",
      temperature_above: "30",
      purifier_level: "3",
      fresh_air_level: "2",
      humidifier_level: "2"
    },
    presets: [
      { key: "GOOD", name: "舒适" },
      { key: "HOT", name: "高温" },
      { key: "DRY", name: "干燥" },
      { key: "POLLUTED", name: "污染" }
    ],
    scenes: [
      { key: "HOME", value: "home", name: "回家" },
      { key: "AWAY", value: "away", name: "离家" },
      { key: "SLEEP", value: "sleep", name: "睡眠" },
      { key: "VENTILATE", value: "ventilate", name: "通风" },
      { key: "CLEAN", value: "clean", name: "净化" }
    ]
  },

  onLoad() {
    const savedHost = wx.getStorageSync("esp32_host");
    const savedToken = wx.getStorageSync("esp32_api_token");
    const demoMode = wx.getStorageSync("smart_home_demo_mode") === true;
    this.setData({
      host: savedHost || this.data.host,
      apiToken: savedToken || "",
      demoMode
    });
    if (demoMode) {
      this.initializeDemoData();
      this.refreshAll();
    } else if (savedHost) {
      this.refreshAll();
    }
  },

  onShow() {
    this.stopAutoRefresh();
    this.refreshTimer = setInterval(() => {
      if ((this.data.demoMode || this.data.host) && !this.data.loading) {
        this.refreshAll();
      }
    }, 10000);
  },

  onHide() {
    this.stopAutoRefresh();
  },

  onUnload() {
    this.stopAutoRefresh();
  },

  stopAutoRefresh() {
    if (this.refreshTimer) {
      clearInterval(this.refreshTimer);
      this.refreshTimer = null;
    }
  },

  onHostInput(event) {
    this.setData({ host: event.detail.value });
  },

  onTokenInput(event) {
    this.setData({ apiToken: event.detail.value });
  },

  toggleDemoMode(event) {
    const enabled = event.detail.value;
    wx.setStorageSync("smart_home_demo_mode", enabled);
    this.setData({ demoMode: enabled, message: enabled ? "离线演示模式已开启" : "已切回真实设备模式" });
    if (enabled) {
      this.initializeDemoData();
      this.refreshAll();
    } else {
      this.demoState = null;
      this.refreshAll();
    }
  },

  saveHost() {
    wx.setStorageSync("esp32_host", this.data.host);
    wx.setStorageSync("esp32_api_token", this.data.apiToken.trim());
    this.setData({ message: this.data.demoMode ? "离线演示无需连接 ESP32" : "地址已保存" });
    this.refreshAll();
  },

  apiBase() {
    const host = this.data.host.trim();
    if (host.startsWith("http://") || host.startsWith("https://")) {
      return host;
    }
    return `http://${host}`;
  },

  request(path, method = "GET", data = undefined) {
    if (this.data.demoMode) {
      return Promise.resolve(this.demoRequest(path, method, data));
    }
    this.setData({ loading: true });
    return new Promise((resolve, reject) => {
      wx.request({
        url: `${this.apiBase()}${path}`,
        method,
        data,
        header: {
          "content-type": "application/json",
          ...(this.data.apiToken.trim() ? { "X-API-Key": this.data.apiToken.trim() } : {})
        },
        success: (res) => {
          if (res.statusCode >= 200 && res.statusCode < 300) {
            resolve(res.data);
          } else {
            reject(new Error(`HTTP ${res.statusCode}`));
          }
        },
        fail: reject,
        complete: () => {
          this.setData({ loading: false });
        }
      });
    });
  },

  buildDevices(state) {
    return [
      { key: "purifier", name: "净化", level: state.purifier_level, override: state.purifier_override_remaining_seconds || 0 },
      { key: "fresh_air", name: "新风", level: state.fresh_air_level, override: state.fresh_air_override_remaining_seconds || 0 },
      { key: "humidifier", name: "加湿", level: state.humidifier_level, override: state.humidifier_override_remaining_seconds || 0 }
    ];
  },

  decorateState(state) {
    const automationRule = state.automation_rule || {
      enabled: false,
      active: false,
      air_score_below: 60,
      humidity_below: 35,
      temperature_above: 30,
      purifier_level: 3,
      fresh_air_level: 2,
      humidifier_level: 2
    };
    return {
      ...state,
      automation_rule: automationRule,
      activeSceneText: sceneValueNames[state.active_scene || "custom"] || state.active_scene || "自定义"
    };
  },

  buildRadarView(state) {
    if (!state || !state.has_radar_position || !state.radar_target_count) {
      return { visible: false, left: 50, top: 86, label: "等待目标" };
    }
    const x = Number(state.radar_nearest_x_mm || 0);
    const y = Number(state.radar_nearest_y_mm || 0);
    return {
      visible: true,
      left: clamp(((x + 3000) / 6000) * 100, 5, 95),
      top: clamp(100 - (y / 6000) * 100, 6, 92),
      label: `${state.radar_zone || "unknown"} · X ${x} / Y ${y} mm`
    };
  },

  buildRuleForm(state) {
    const rule = state.automation_rule || {};
    return {
      enabled: Boolean(rule.enabled),
      air_score_below: String(rule.air_score_below == null ? 60 : rule.air_score_below),
      humidity_below: String(rule.humidity_below == null ? 35 : rule.humidity_below),
      temperature_above: String(rule.temperature_above == null ? 30 : rule.temperature_above),
      purifier_level: String(rule.purifier_level == null ? 3 : rule.purifier_level),
      fresh_air_level: String(rule.fresh_air_level == null ? 2 : rule.fresh_air_level),
      humidifier_level: String(rule.humidifier_level == null ? 2 : rule.humidifier_level)
    };
  },

  formatSamples(history, health) {
    return (history.samples || []).slice(-30).map((sample, index) => ({
      ...sample,
      index: index + 1,
      ageText: sample.sample_time_ms && health.uptime_ms >= sample.sample_time_ms
        ? `${Math.round((health.uptime_ms - sample.sample_time_ms) / 1000)}秒前`
        : "--",
      airWidth: Math.max(4, Math.min(100, sample.air_score || 0))
    }));
  },

  formatEvents(payload, health) {
    return (payload.events || []).slice(-20).reverse().map((event, index) => ({
      ...event,
      key: `${event.timestamp_ms || 0}-${index}`,
      ageText: event.timestamp_ms && health.uptime_ms >= event.timestamp_ms
        ? `${Math.round((health.uptime_ms - event.timestamp_ms) / 1000)}秒前`
        : "刚刚",
      typeText: {
        alarm: "告警",
        automation: "自动",
        device: "设备",
        scene: "场景",
        presence: "雷达",
        environment: "环境",
        mode: "模式",
        system: "系统"
      }[event.type] || "信息"
    }));
  },

  applyState(state, message) {
    const viewState = this.decorateState(state);
    this.setData({
      state: viewState,
      devices: this.buildDevices(viewState),
      radar: this.buildRadarView(viewState),
      ruleForm: this.buildRuleForm(viewState),
      message
    });
  },

  refreshAll() {
    if (this.data.demoMode) {
      this.advanceDemoData();
    }
    Promise.all([
      this.request("/api/state"),
      this.request("/api/history"),
      this.request("/api/health"),
      this.request("/api/events").catch(() => ({ events: [] }))
    ]).then(([state, history, health, eventPayload]) => {
      const viewState = this.decorateState(state);
      const healthView = {
        ...health,
        uptimeText: `${Math.round((health.uptime_ms || 0) / 1000)} 秒`,
        wifiText: health.has_wifi_rssi ? `${health.wifi_rssi_dbm} dBm` : "--",
        dhtText: `${health.dht_stale ? "数据过期" : (health.dht_current_ok ? "正常" : "使用缓存")} / 失败 ${health.dht_consecutive_failures || 0}`,
        mq135Text: `${health.mq135_stale ? "数据过期" : "正常"} / 失败 ${health.mq135_consecutive_failures || 0}`,
        lightText: `${health.ambient_light_stale ? "数据过期" : "正常"} / 失败 ${health.ambient_light_consecutive_failures || 0}`,
        radarText: `${health.radar_stale ? "无新帧" : "正常"} / 有效 ${health.radar_valid_frames || 0}`
      };
      const samples = this.formatSamples(history, health);
      this.setData({
        samples,
        events: this.formatEvents(eventPayload, health),
        health: healthView,
        state: viewState,
        devices: this.buildDevices(viewState),
        radar: this.buildRadarView(viewState),
        ruleForm: this.buildRuleForm(viewState),
        message: this.data.demoMode ? "离线演示数据已刷新" : "数据已刷新"
      }, () => this.drawTrendChart());
    }).catch((error) => {
      this.setData({ message: `连接失败：${error.message}；可开启离线演示模式` });
    });
  },

  setDevice(event) {
    const { device, level } = event.currentTarget.dataset;
    const numericLevel = Number(level);
    this.request("/api/device", "POST", {
      device,
      power: numericLevel > 0,
      level: numericLevel
    }).then((state) => {
      this.applyState(state, `${deviceNames[device]}已切到 ${numericLevel} 档`);
      this.refreshEventsOnly();
    }).catch((error) => {
      this.setData({ message: `控制失败：${error.message}` });
    });
  },

  setMode(event) {
    const mode = event.currentTarget.dataset.mode;
    const power = !this.data.state || !this.data.state[`${mode}_mode`];
    this.request("/api/mode", "POST", { mode, power }).then((state) => {
      this.applyState(state, `${mode === "auto" ? "自动" : "节能"}模式${power ? "开启" : "关闭"}`);
      this.refreshEventsOnly();
    }).catch((error) => {
      this.setData({ message: `模式切换失败：${error.message}` });
    });
  },

  setScene(event) {
    const scene = event.currentTarget.dataset.scene;
    this.request("/api/scene", "POST", { scene }).then((state) => {
      this.applyState(state, `${sceneNames[scene]}场景已生效`);
      this.refreshEventsOnly();
    }).catch((error) => {
      this.setData({ message: `场景切换失败：${error.message}` });
    });
  },

  onRuleInput(event) {
    const field = event.currentTarget.dataset.field;
    this.setData({ [`ruleForm.${field}`]: event.detail.value });
  },

  onRuleToggle(event) {
    this.setData({ "ruleForm.enabled": event.detail.value });
  },

  saveAutomationRule() {
    const form = this.data.ruleForm;
    const payload = {
      enabled: form.enabled,
      air_score_below: Number(form.air_score_below),
      humidity_below: Number(form.humidity_below),
      temperature_above: Number(form.temperature_above),
      purifier_level: Number(form.purifier_level),
      fresh_air_level: Number(form.fresh_air_level),
      humidifier_level: Number(form.humidifier_level)
    };
    if (Object.keys(payload).some((key) => key !== "enabled" && !Number.isFinite(payload[key]))) {
      this.setData({ message: "自动化规则中存在无效数字" });
      return;
    }
    this.request("/api/automation", "POST", payload).then((state) => {
      this.applyState(state, "自动化规则已保存");
      this.refreshEventsOnly();
    }).catch((error) => {
      this.setData({ message: `规则保存失败：${error.message}` });
    });
  },

  onManualInput(event) {
    const field = event.currentTarget.dataset.field;
    this.setData({ [`manualForm.${field}`]: event.detail.value });
  },

  setManualEnvironment() {
    const form = this.data.manualForm;
    const temperature = Number(form.temperature_c);
    const humidity = Number(form.humidity_percent);
    const airScore = Number(form.air_score);
    if (!Number.isFinite(temperature) || !Number.isFinite(humidity) || !Number.isFinite(airScore)) {
      this.setData({ message: "请填写有效的温度、湿度和空气评分" });
      return;
    }
    this.request("/api/environment", "POST", {
      enabled: true,
      temperature_c: temperature,
      humidity_percent: humidity,
      air_score: airScore
    }).then((state) => {
      this.applyState(state, "手动输入数据已生效");
      this.refreshAll();
    }).catch((error) => {
      this.setData({ message: `手动环境设置失败：${error.message}` });
    });
  },

  setEnvironmentPreset(event) {
    const preset = event.currentTarget.dataset.preset;
    this.request("/api/environment", "POST", { enabled: true, preset }).then((state) => {
      this.applyState(state, `${presetNames[preset]}场景已生效`);
      this.refreshAll();
    }).catch((error) => {
      this.setData({ message: `场景设置失败：${error.message}` });
    });
  },

  disableManualEnvironment() {
    this.request("/api/environment", "POST", { enabled: false }).then((state) => {
      this.applyState(state, "已恢复真实传感器数据");
      this.refreshEventsOnly();
    }).catch((error) => {
      this.setData({ message: `退出手动模式失败：${error.message}` });
    });
  },

  acknowledgeAlarm() {
    this.request("/api/alarm/ack", "POST", {}).then((state) => {
      this.applyState(state, "告警已确认");
      this.refreshEventsOnly();
    }).catch((error) => {
      this.setData({ message: `确认告警失败：${error.message}` });
    });
  },

  refreshEventsOnly() {
    if (!this.data.health) {
      return;
    }
    this.request("/api/events").then((payload) => {
      this.setData({ events: this.formatEvents(payload, this.data.health) });
    }).catch(() => {});
  },

  drawTrendChart() {
    if (!this.data.samples.length) {
      return;
    }
    wx.createSelectorQuery().in(this).select("#trendCanvas").fields({ node: true, size: true }).exec((result) => {
      const item = result && result[0];
      if (!item || !item.node || !item.width || !item.height) {
        return;
      }
      const canvas = item.node;
      const context = canvas.getContext("2d");
      const dpr = wx.getWindowInfo ? wx.getWindowInfo().pixelRatio : wx.getSystemInfoSync().pixelRatio;
      canvas.width = item.width * dpr;
      canvas.height = item.height * dpr;
      context.scale(dpr, dpr);
      const width = item.width;
      const height = item.height;
      const padding = 18;
      context.clearRect(0, 0, width, height);
      context.strokeStyle = "#e5e7eb";
      context.lineWidth = 1;
      [0.25, 0.5, 0.75].forEach((ratio) => {
        context.beginPath();
        context.moveTo(padding, height * ratio);
        context.lineTo(width - padding, height * ratio);
        context.stroke();
      });
      const drawLine = (values, minimum, maximum, color) => {
        context.beginPath();
        context.strokeStyle = color;
        context.lineWidth = 2;
        values.forEach((value, index) => {
          const x = padding + (values.length === 1 ? 0 : index * (width - padding * 2) / (values.length - 1));
          const normalized = clamp((Number(value) - minimum) / (maximum - minimum), 0, 1);
          const y = height - padding - normalized * (height - padding * 2);
          if (index === 0) context.moveTo(x, y);
          else context.lineTo(x, y);
        });
        context.stroke();
      };
      drawLine(this.data.samples.map((sample) => sample.air_score || 0), 0, 100, "#14b8a6");
      drawLine(this.data.samples.map((sample) => sample.temperature_c || 0), 0, 50, "#f97316");
      drawLine(this.data.samples.map((sample) => sample.humidity_percent || 0), 0, 100, "#3b82f6");
    });
  },

  initializeDemoData() {
    const now = Date.now();
    this.demoStart = now;
    this.demoTick = 0;
    this.demoEvents = [{ timestamp_ms: 0, type: "system", source: "demo", message: "离线演示系统已启动" }];
    this.demoHistory = [];
    this.demoState = {
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
      this.demoHistory.push({
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
  },

  demoUptime() {
    return Math.max(0, Date.now() - this.demoStart);
  },

  addDemoEvent(type, source, message) {
    this.demoEvents.push({ timestamp_ms: this.demoUptime(), type, source, message });
    this.demoEvents = this.demoEvents.slice(-32);
  },

  advanceDemoData() {
    if (!this.demoState) this.initializeDemoData();
    this.demoTick += 1;
    const state = this.demoState;
    state.sample_time_ms = this.demoUptime();
    state.radar_nearest_x_mm = Math.round(Math.sin(this.demoTick / 2) * 2200);
    state.radar_nearest_y_mm = 1600 + Math.round((Math.cos(this.demoTick / 3) + 1) * 900);
    state.radar_nearest_speed_mm_per_s = Math.round(Math.cos(this.demoTick / 2) * 260);
    state.radar_zone = state.radar_nearest_x_mm <= -500 ? "left" : (state.radar_nearest_x_mm >= 500 ? "right" : "center");
    if (!state.manual_environment_mode) {
      state.temperature_c = Number((26 + Math.sin(this.demoTick / 3) * 1.2).toFixed(1));
      state.humidity_percent = Number((54 + Math.cos(this.demoTick / 4) * 4).toFixed(1));
      state.air_score = clamp(Math.round(state.air_score + (state.purifier_level > 0 ? 3 : Math.sin(this.demoTick) * 2)), 25, 95);
      state.mq135_raw = Math.round((100 - state.air_score) * 30);
    }
    this.applyDemoAutomation();
    this.demoHistory.push({
      sample_time_ms: state.sample_time_ms,
      has_temperature: true,
      temperature_c: state.temperature_c,
      has_humidity: true,
      humidity_percent: state.humidity_percent,
      has_mq135_raw: true,
      mq135_raw: state.mq135_raw,
      air_score: state.air_score,
      environment_source: "demo",
      comfort: state.comfort,
      advice: state.advice
    });
    this.demoHistory = this.demoHistory.slice(-30);
  },

  applyDemoAutomation() {
    const state = this.demoState;
    const rule = state.automation_rule;
    const active = Boolean(rule.enabled && state.auto_mode && (
      state.air_score < rule.air_score_below ||
      state.humidity_percent < rule.humidity_below ||
      state.temperature_c > rule.temperature_above
    ));
    if (active && !rule.active) {
      this.addDemoEvent("automation", "system", "环境达到阈值，自动化规则已执行");
    } else if (!active && rule.active) {
      this.addDemoEvent("automation", "system", "环境恢复，自动化规则已解除");
    }
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
  },

  demoRequest(path, method, data = {}) {
    if (!this.demoState) this.initializeDemoData();
    const state = this.demoState;
    if (path === "/api/state") return clone(state);
    if (path === "/api/history") return { count: this.demoHistory.length, capacity: 30, samples: clone(this.demoHistory) };
    if (path === "/api/events") return { count: this.demoEvents.length, capacity: 32, events: clone(this.demoEvents) };
    if (path === "/api/health") {
      return {
        uptime_ms: this.demoUptime(), free_heap_bytes: 196608, firmware_version: "demo-2.0",
        network_connected: true, hmi_initialized: true, api_auth_enabled: false,
        has_wifi_rssi: true, wifi_rssi_dbm: -48, dht_current_ok: true, dht_stale: false,
        dht_consecutive_failures: 0, mq135_stale: false, mq135_consecutive_failures: 0,
        ambient_light_stale: false, ambient_light_consecutive_failures: 0,
        radar_stale: false, radar_valid_frames: 128 + this.demoTick
      };
    }
    if (method !== "POST") return clone(state);
    if (path === "/api/device") {
      const key = `${data.device}_level`;
      if (key in state) state[key] = data.power ? clamp(Number(data.level), 0, 3) : 0;
      if (data.device === "light") state.light_on = Boolean(data.power);
      state.active_scene = "custom";
      this.addDemoEvent("device", "manual", `${deviceNames[data.device] || data.device}切换到${Number(data.level) || 0}档`);
    } else if (path === "/api/mode") {
      state[`${data.mode}_mode`] = Boolean(data.power);
      if (data.power) state[`${data.mode === "auto" ? "eco" : "auto"}_mode`] = false;
      state.active_scene = "custom";
      this.addDemoEvent("mode", "manual", `${data.mode === "auto" ? "自动" : "节能"}模式${data.power ? "已开启" : "已关闭"}`);
      this.applyDemoAutomation();
    } else if (path === "/api/scene") {
      this.applyDemoScene(data.scene);
    } else if (path === "/api/automation") {
      state.automation_rule = { ...state.automation_rule, ...data, active: false };
      this.addDemoEvent("automation", "manual", data.enabled ? "自定义自动化规则已启用" : "自定义自动化规则已停用");
      this.applyDemoAutomation();
    } else if (path === "/api/environment") {
      if (!data.enabled) {
        state.manual_environment_mode = false;
        state.environment_source = "demo";
        this.addDemoEvent("environment", "manual", "已恢复模拟传感器数据");
      } else {
        const presets = {
          GOOD: [26, 55, 88], HOT: [33, 58, 72], DRY: [25, 28, 76], POLLUTED: [27, 60, 28]
        };
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
        this.addDemoEvent("environment", "manual", data.preset ? `${presetNames[data.preset]}环境已模拟` : "手动环境数据已更新");
        this.applyDemoAutomation();
      }
    } else if (path === "/api/alarm/ack") {
      state.alarm_active = false;
      state.alarm_reason = "";
      this.addDemoEvent("alarm", "manual", "当前告警已确认");
    }
    return clone(state);
  },

  applyDemoScene(scene) {
    const state = this.demoState;
    const settings = {
      HOME: { active_scene: "home", occupied: true, auto_mode: true, eco_mode: false, purifier_level: 1, fresh_air_level: 1, humidifier_level: 0, light_on: true },
      AWAY: { active_scene: "away", occupied: false, auto_mode: false, eco_mode: true, purifier_level: 0, fresh_air_level: 0, humidifier_level: 0, light_on: false },
      SLEEP: { active_scene: "sleep", occupied: true, auto_mode: false, eco_mode: true, purifier_level: 1, fresh_air_level: 0, humidifier_level: 1, light_on: false },
      VENTILATE: { active_scene: "ventilate", auto_mode: false, eco_mode: false, purifier_level: 1, fresh_air_level: 3, humidifier_level: 0 },
      CLEAN: { active_scene: "clean", auto_mode: false, eco_mode: false, purifier_level: 3, fresh_air_level: 2, humidifier_level: 0 }
    };
    Object.assign(state, settings[scene] || {});
    this.addDemoEvent("scene", "manual", `已切换到${sceneNames[scene] || scene}场景`);
  }
});
