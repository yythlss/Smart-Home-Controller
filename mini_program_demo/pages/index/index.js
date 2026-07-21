const deviceNames = {
  purifier: "净化",
  fresh_air: "新风",
  humidifier: "加湿"
};

const presetNames = {
  GOOD: "舒适",
  HOT: "高温",
  DRY: "干燥",
  POLLUTED: "污染"
};

Page({
  data: {
    host: "192.168.1.100:8080",
    state: null,
    devices: [],
    samples: [],
    message: "请先填写 ESP32 的局域网 IP",
    loading: false,
    levels: [0, 1, 2, 3],
    manualForm: {
      temperature_c: "30",
      humidity_percent: "35",
      air_score: "45"
    },
    presets: [
      { key: "GOOD", name: "舒适" },
      { key: "HOT", name: "高温" },
      { key: "DRY", name: "干燥" },
      { key: "POLLUTED", name: "污染" }
    ]
  },

  onLoad() {
    const savedHost = wx.getStorageSync("esp32_host");
    if (savedHost) {
      this.setData({ host: savedHost });
      this.refreshAll();
    }
  },

  onHostInput(event) {
    this.setData({ host: event.detail.value });
  },

  saveHost() {
    wx.setStorageSync("esp32_host", this.data.host);
    this.setData({ message: "地址已保存" });
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
    this.setData({ loading: true });
    return new Promise((resolve, reject) => {
      wx.request({
        url: `${this.apiBase()}${path}`,
        method,
        data,
        header: {
          "content-type": "application/json"
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
      { key: "purifier", name: "净化", level: state.purifier_level },
      { key: "fresh_air", name: "新风", level: state.fresh_air_level },
      { key: "humidifier", name: "加湿", level: state.humidifier_level }
    ];
  },

  applyState(state, message) {
    this.setData({
      state,
      devices: this.buildDevices(state),
      message
    });
  },

  refreshAll() {
    Promise.all([
      this.request("/api/state"),
      this.request("/api/history")
    ]).then(([state, history]) => {
      const samples = (history.samples || []).slice(-30).map((sample, index) => ({
        ...sample,
        index: index + 1,
        airWidth: Math.max(4, Math.min(100, sample.air_score || 0))
      }));
      this.setData({
        samples,
        state,
        devices: this.buildDevices(state),
        message: "数据已刷新"
      });
    }).catch((error) => {
      this.setData({ message: `连接失败：${error.message}` });
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
    }).catch((error) => {
      this.setData({ message: `控制失败：${error.message}` });
    });
  },

  setMode(event) {
    const mode = event.currentTarget.dataset.mode;
    const power = !this.data.state || !this.data.state[`${mode}_mode`];
    this.request("/api/mode", "POST", { mode, power }).then((state) => {
      this.applyState(state, `${mode === "auto" ? "自动" : "节能"}模式${power ? "开启" : "关闭"}`);
    }).catch((error) => {
      this.setData({ message: `模式切换失败：${error.message}` });
    });
  },

  onManualInput(event) {
    const field = event.currentTarget.dataset.field;
    this.setData({
      [`manualForm.${field}`]: event.detail.value
    });
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
    }).catch((error) => {
      this.setData({ message: `手动环境设置失败：${error.message}` });
    });
  },

  setEnvironmentPreset(event) {
    const preset = event.currentTarget.dataset.preset;
    this.request("/api/environment", "POST", {
      enabled: true,
      preset
    }).then((state) => {
      this.applyState(state, `${presetNames[preset]}场景已生效`);
    }).catch((error) => {
      this.setData({ message: `场景设置失败：${error.message}` });
    });
  },

  disableManualEnvironment() {
    this.request("/api/environment", "POST", {
      enabled: false
    }).then((state) => {
      this.applyState(state, "已恢复真实传感器数据");
    }).catch((error) => {
      this.setData({ message: `退出手动模式失败：${error.message}` });
    });
  }
});
