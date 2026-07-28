const service = require("../../utils/smart_home_service");

const presetNames = {
  GOOD: "舒适",
  HOT: "高温",
  DRY: "干燥",
  POLLUTED: "污染"
};

Page({
  data: {
    host: "",
    apiToken: "",
    demoMode: false,
    loading: false,
    message: "",
    state: null,
    health: null,
    events: [],
    sourceSystem: "sensor",
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
    ]
  },

  onLoad() {
    this.loadConfig();
  },

  onShow() {
    this.loadConfig();
    this.refreshAll();
  },

  loadConfig() {
    const config = service.getConfig();
    this.setData({
      host: config.host || "192.168.1.100:8080",
      apiToken: config.apiToken,
      demoMode: config.demoMode
    });
  },

  onHostInput(event) {
    this.setData({ host: event.detail.value });
  },

  onTokenInput(event) {
    this.setData({ apiToken: event.detail.value });
  },

  saveConnection() {
    service.saveConfig(this.data.host, this.data.apiToken);
    this.setData({ message: "连接配置已保存" });
    this.refreshAll();
  },

  toggleDemoMode(event) {
    const enabled = event.detail.value;
    service.setDemoMode(enabled);
    this.setData({ demoMode: enabled, message: enabled ? "离线演示系统已启用" : "已切换回真实 ESP32" });
    this.refreshAll();
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

  formatHealth(health) {
    return {
      ...health,
      uptimeText: `${Math.round((health.uptime_ms || 0) / 1000)} 秒`,
      wifiText: health.has_wifi_rssi ? `${health.wifi_rssi_dbm} dBm` : "--",
      dhtText: `${health.dht_stale ? "数据过期" : (health.dht_current_ok ? "正常" : "使用缓存")} / 失败 ${health.dht_consecutive_failures || 0}`,
      mq135Text: `${health.mq135_stale ? "数据过期" : "正常"} / 失败 ${health.mq135_consecutive_failures || 0}`,
      lightText: `${health.ambient_light_stale ? "数据过期" : "正常"} / 失败 ${health.ambient_light_consecutive_failures || 0}`,
      radarText: `${health.radar_stale ? "无新帧" : "正常"} / 有效 ${health.radar_valid_frames || 0}`
    };
  },

  formatEvents(payload, health) {
    return (payload.events || []).slice(-24).reverse().map((event, index) => ({
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
        environment: "数据源",
        mode: "模式",
        system: "系统"
      }[event.type] || "信息"
    }));
  },

  refreshAll() {
    const config = service.getConfig();
    if (!config.host && !config.demoMode) {
      this.setData({ state: null, health: null, events: [], message: "请先保存 ESP32 地址，或开启离线演示" });
      return;
    }
    this.setData({ loading: true });
    service.advanceDemoData();
    Promise.all([
      service.request("/api/state"),
      service.request("/api/health"),
      service.request("/api/events").catch(() => ({ events: [] }))
    ]).then(([state, health, eventPayload]) => {
      this.setData({
        state,
        health: this.formatHealth(health),
        events: this.formatEvents(eventPayload, health),
        sourceSystem: state.manual_environment_mode ? "manual" : "sensor",
        ruleForm: this.buildRuleForm(state),
        message: config.demoMode ? "正在使用离线演示系统" : "后台数据已刷新"
      });
    }).catch((error) => {
      this.setData({ message: `后台连接失败：${error.message}` });
    }).finally(() => {
      this.setData({ loading: false });
    });
  },

  switchToSensorSystem() {
    this.setData({ loading: true });
    service.request("/api/environment", "POST", { enabled: false })
      .then((state) => {
        this.setData({ state, sourceSystem: "sensor", message: "已切换到真实传感器系统" });
        this.refreshAll();
      })
      .catch((error) => this.setData({ message: `切换失败：${error.message}` }))
      .finally(() => this.setData({ loading: false }));
  },

  switchToManualSystem() {
    this.submitManualEnvironment("已切换到手动传感器系统");
  },

  onManualInput(event) {
    const field = event.currentTarget.dataset.field;
    this.setData({ [`manualForm.${field}`]: event.detail.value });
  },

  applyManualEnvironment() {
    this.submitManualEnvironment("手动传感器数值已更新");
  },

  submitManualEnvironment(successMessage) {
    const form = this.data.manualForm;
    const temperature = Number(form.temperature_c);
    const humidity = Number(form.humidity_percent);
    const airScore = Number(form.air_score);
    if (![temperature, humidity, airScore].every(Number.isFinite)) {
      this.setData({ message: "请填写有效的温度、湿度和空气评分" });
      return;
    }
    this.setData({ loading: true });
    service.request("/api/environment", "POST", {
      enabled: true,
      temperature_c: temperature,
      humidity_percent: humidity,
      air_score: airScore
    }).then((state) => {
      this.setData({ state, sourceSystem: "manual", message: successMessage });
      this.refreshAll();
    }).catch((error) => {
      this.setData({ message: `手动数据设置失败：${error.message}` });
    }).finally(() => {
      this.setData({ loading: false });
    });
  },

  setEnvironmentPreset(event) {
    const preset = event.currentTarget.dataset.preset;
    this.setData({ loading: true });
    service.request("/api/environment", "POST", { enabled: true, preset })
      .then((state) => {
        this.setData({ state, sourceSystem: "manual", message: `${presetNames[preset]}模拟数据已启用` });
        this.refreshAll();
      })
      .catch((error) => this.setData({ message: `预设设置失败：${error.message}` }))
      .finally(() => this.setData({ loading: false }));
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
    this.setData({ loading: true });
    service.request("/api/automation", "POST", payload)
      .then((state) => {
        this.setData({ state, ruleForm: this.buildRuleForm(state), message: "自动化规则已保存" });
        this.refreshAll();
      })
      .catch((error) => this.setData({ message: `规则保存失败：${error.message}` }))
      .finally(() => this.setData({ loading: false }));
  },

  acknowledgeAlarm() {
    service.request("/api/alarm/ack", "POST", {})
      .then(() => {
        this.setData({ message: "告警已确认" });
        this.refreshAll();
      })
      .catch((error) => this.setData({ message: `确认告警失败：${error.message}` }));
  }
});
