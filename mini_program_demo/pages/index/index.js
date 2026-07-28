const service = require("../../utils/smart_home_service");

const deviceNames = {
  purifier: "净化器",
  fresh_air: "新风",
  humidifier: "加湿器"
};

const sceneNames = {
  HOME: "回家",
  AWAY: "离家",
  SLEEP: "睡眠",
  VENTILATE: "通风",
  CLEAN: "强力净化"
};

const sceneValueNames = {
  custom: "日常",
  home: "回家",
  away: "离家",
  sleep: "睡眠",
  ventilate: "通风",
  clean: "强力净化"
};

function clamp(value, minimum, maximum) {
  return Math.max(minimum, Math.min(maximum, value));
}

Page({
  data: {
    configured: false,
    demoMode: false,
    connected: false,
    loading: false,
    message: "正在连接家庭设备…",
    state: null,
    health: null,
    devices: [],
    samples: [],
    levels: [0, 1, 2, 3],
    radar: { visible: false, left: 50, top: 86, label: "当前无人" },
    scenes: [
      { key: "HOME", value: "home", name: "回家" },
      { key: "AWAY", value: "away", name: "离家" },
      { key: "SLEEP", value: "sleep", name: "睡眠" },
      { key: "VENTILATE", value: "ventilate", name: "通风" },
      { key: "CLEAN", value: "clean", name: "净化" }
    ]
  },

  onShow() {
    this.stopAutoRefresh();
    this.refreshAll();
    this.refreshTimer = setInterval(() => {
      if (!this.data.loading) this.refreshAll();
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

  decorateState(state, demoMode) {
    const activeScene = state.active_scene || "custom";
    let sourceText = "真实传感器";
    if (demoMode) sourceText = "离线演示";
    else if (state.manual_environment_mode) sourceText = "后台手动数据";
    else if (state.cached_temperature_humidity) sourceText = "传感器缓存";
    return {
      ...state,
      activeSceneText: sceneValueNames[activeScene] || "日常",
      sourceText
    };
  },

  buildDevices(state) {
    return [
      { key: "purifier", name: "净化", level: state.purifier_level, override: state.purifier_override_remaining_seconds || 0 },
      { key: "fresh_air", name: "新风", level: state.fresh_air_level, override: state.fresh_air_override_remaining_seconds || 0 },
      { key: "humidifier", name: "加湿", level: state.humidifier_level, override: state.humidifier_override_remaining_seconds || 0 }
    ];
  },

  buildRadarView(state) {
    if (!state.has_radar_position || !state.radar_target_count) {
      return { visible: false, left: 50, top: 86, label: state.occupancy_known && !state.occupied ? "当前无人" : "等待雷达数据" };
    }
    const x = Number(state.radar_nearest_x_mm || 0);
    const y = Number(state.radar_nearest_y_mm || 0);
    return {
      visible: true,
      left: clamp(((x + 3000) / 6000) * 100, 5, 95),
      top: clamp(100 - (y / 6000) * 100, 6, 92),
      label: `${state.radar_zone || "center"} · 距离 ${y} mm`
    };
  },

  formatSamples(history) {
    return (history.samples || []).slice(-30).map((sample, index) => ({
      ...sample,
      index
    }));
  },

  refreshAll() {
    const config = service.getConfig();
    const configured = Boolean(config.host || config.demoMode);
    this.setData({ configured, demoMode: config.demoMode });
    if (!configured) {
      this.setData({
        connected: false,
        state: null,
        health: null,
        samples: [],
        message: "尚未配置家庭设备，请进入页面底部的操作后台"
      });
      return;
    }
    this.setData({ loading: true });
    service.advanceDemoData();
    Promise.all([
      service.request("/api/state"),
      service.request("/api/history"),
      service.request("/api/health")
    ]).then(([state, history, health]) => {
      const viewState = this.decorateState(state, config.demoMode);
      this.setData({
        connected: true,
        state: viewState,
        health,
        devices: this.buildDevices(viewState),
        samples: this.formatSamples(history),
        radar: this.buildRadarView(viewState),
        message: ""
      }, () => this.drawTrendChart());
    }).catch((error) => {
      this.setData({ connected: false, message: `家庭设备暂时离线：${error.message}` });
    }).finally(() => {
      this.setData({ loading: false });
    });
  },

  applyState(state, message) {
    const config = service.getConfig();
    const viewState = this.decorateState(state, config.demoMode);
    this.setData({
      state: viewState,
      devices: this.buildDevices(viewState),
      radar: this.buildRadarView(viewState),
      message
    });
  },

  setDevice(event) {
    const { device, level } = event.currentTarget.dataset;
    const numericLevel = Number(level);
    this.setData({ loading: true });
    service.request("/api/device", "POST", { device, power: numericLevel > 0, level: numericLevel })
      .then((state) => this.applyState(state, `${deviceNames[device]}已切换到 ${numericLevel} 档`))
      .catch((error) => this.setData({ message: `控制失败：${error.message}` }))
      .finally(() => this.setData({ loading: false }));
  },

  toggleLight() {
    const power = !this.data.state.light_on;
    this.setData({ loading: true });
    service.request("/api/device", "POST", { device: "light", power, level: power ? 1 : 0 })
      .then((state) => this.applyState(state, power ? "灯光已开启" : "灯光已关闭"))
      .catch((error) => this.setData({ message: `灯光控制失败：${error.message}` }))
      .finally(() => this.setData({ loading: false }));
  },

  setMode(event) {
    const mode = event.currentTarget.dataset.mode;
    const power = !this.data.state[`${mode}_mode`];
    this.setData({ loading: true });
    service.request("/api/mode", "POST", { mode, power })
      .then((state) => this.applyState(state, `${mode === "auto" ? "自动" : "节能"}模式${power ? "已开启" : "已关闭"}`))
      .catch((error) => this.setData({ message: `模式切换失败：${error.message}` }))
      .finally(() => this.setData({ loading: false }));
  },

  setScene(event) {
    const scene = event.currentTarget.dataset.scene;
    this.setData({ loading: true });
    service.request("/api/scene", "POST", { scene })
      .then((state) => this.applyState(state, `${sceneNames[scene]}场景已生效`))
      .catch((error) => this.setData({ message: `场景切换失败：${error.message}` }))
      .finally(() => this.setData({ loading: false }));
  },

  acknowledgeAlarm() {
    service.request("/api/alarm/ack", "POST", {})
      .then((state) => this.applyState(state, "告警已确认"))
      .catch((error) => this.setData({ message: `确认告警失败：${error.message}` }));
  },

  openAdmin() {
    wx.navigateTo({ url: "/pages/admin/admin" });
  },

  drawTrendChart() {
    if (!this.data.samples.length) return;
    wx.createSelectorQuery().in(this).select("#trendCanvas").fields({ node: true, size: true }).exec((result) => {
      const item = result && result[0];
      if (!item || !item.node || !item.width || !item.height) return;
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
  }
});
