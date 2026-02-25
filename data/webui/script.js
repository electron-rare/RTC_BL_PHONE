const SECTION_MAP = {
  dashboard: "dashboardSection",
  config: "configSection",
  network: "networkSection",
  control: "controlSection",
};
let realtimeSource = null;
let realtimeConnected = false;
let fallbackPollingTimer = null;

function showSection(section) {
  Object.values(SECTION_MAP).forEach((id) => {
    const el = document.getElementById(id);
    if (el) {
      el.classList.remove("active");
    }
  });
  const sectionEl = document.getElementById(SECTION_MAP[section]);
  if (sectionEl) {
    sectionEl.classList.add("active");
  }
}

function setJson(id, value) {
  const el = document.getElementById(id);
  if (!el) {
    return;
  }
  if (typeof value === "string") {
    el.textContent = value;
    return;
  }
  el.textContent = JSON.stringify(value, null, 2);
}

async function requestJson(url, options = {}) {
  const response = await fetch(url, options);
  const text = await response.text();
  let parsed = {};
  if (text) {
    try {
      parsed = JSON.parse(text);
    } catch (_) {
      parsed = { raw: text };
    }
  }
  if (!response.ok) {
    throw new Error(`HTTP ${response.status} ${text || ""}`.trim());
  }
  return parsed;
}

function jsonHeaders() {
  return { "Content-Type": "application/json" };
}

function parseJsonInput(id) {
  const raw = document.getElementById(id).value.trim();
  if (!raw) {
    return {};
  }
  return JSON.parse(raw);
}

function parsePayloadValue(rawPayload) {
  const trimmed = rawPayload.trim();
  if (trimmed.startsWith("{") || trimmed.startsWith("[")) {
    try {
      return JSON.parse(trimmed);
    } catch (_) {
      return rawPayload;
    }
  }
  return rawPayload;
}

function parseRealtimeData(raw) {
  try {
    return JSON.parse(raw);
  } catch (_) {
    return { raw };
  }
}

function applyStatusSnapshot(status) {
  const line = document.getElementById("statusLine");
  const telephonyState = status.telephony?.state || "n/a";
  const hook = status.telephony?.hook || "n/a";
  const wifiState = status.wifi?.state || "n/a";
  const mqttConnected = status.mqtt?.connected ? "on" : "off";
  const peers = status.espnow?.peer_count ?? 0;
  const liveState = realtimeConnected ? "live=on" : "live=off";
  line.textContent =
    `board=${status.board_profile || "n/a"} telephony=${telephonyState} hook=${hook} ` +
    `wifi=${wifiState} mqtt=${mqttConnected} espnow_peers=${peers} ${liveState}`;

  setJson("statusJson", status);
  if (status.wifi) {
    setJson("wifiJson", status.wifi);
  }
  if (status.mqtt) {
    setJson("mqttJson", status.mqtt);
  }
  if (status.espnow) {
    setJson("espnowJson", status.espnow);
    setJson("espnowPeersJson", { peers: status.espnow.peers || [] });
  }
}

function connectRealtime() {
  if (!window.EventSource) {
    return;
  }
  if (realtimeSource) {
    realtimeSource.close();
  }

  realtimeSource = new EventSource("/api/events");

  realtimeSource.onopen = () => {
    realtimeConnected = true;
  };

  realtimeSource.addEventListener("hello", () => {
    realtimeConnected = true;
  });

  realtimeSource.addEventListener("status", (event) => {
    realtimeConnected = true;
    const status = parseRealtimeData(event.data);
    applyStatusSnapshot(status);
  });

  realtimeSource.addEventListener("dispatch", (event) => {
    const payload = parseRealtimeData(event.data);
    const out = { stream: "dispatch" };
    if (payload && typeof payload === "object" && !Array.isArray(payload)) {
      Object.assign(out, payload);
    } else {
      out.payload = payload;
    }
    setJson("actionResult", out);
  });

  realtimeSource.addEventListener("effect", (event) => {
    const payload = parseRealtimeData(event.data);
    const out = { stream: "effect" };
    if (payload && typeof payload === "object" && !Array.isArray(payload)) {
      Object.assign(out, payload);
    } else {
      out.payload = payload;
    }
    setJson("actionResult", out);
  });

  realtimeSource.onerror = () => {
    realtimeConnected = false;
  };
}

function ensureFallbackPolling() {
  if (fallbackPollingTimer !== null) {
    return;
  }
  fallbackPollingTimer = window.setInterval(() => {
    if (!realtimeConnected) {
      refreshStatus().catch(() => {});
    }
  }, 2000);
}

async function refreshStatus() {
  try {
    const status = await requestJson("/api/status");
    applyStatusSnapshot(status);
  } catch (error) {
    const line = document.getElementById("statusLine");
    const liveState = realtimeConnected ? "live=on" : "live=off";
    line.textContent = `Erreur statut: ${error.message}`;
    line.textContent += ` ${liveState}`;
    setJson("statusJson", { error: error.message });
  }
}

async function refreshConfig() {
  try {
    const [pins, audio, mqtt] = await Promise.all([
      requestJson("/api/config/pins"),
      requestJson("/api/config/audio"),
      requestJson("/api/config/mqtt"),
    ]);
    setJson("configJson", { pins, audio, mqtt });
    const audioInput = document.getElementById("audioConfigInput");
    const mqttInput = document.getElementById("mqttConfigInput");
    const pinsInput = document.getElementById("pinsConfigInput");
    if (audioInput) {
      audioInput.value = JSON.stringify(audio, null, 2);
    }
    if (mqttInput) {
      mqttInput.value = JSON.stringify(mqtt.config || mqtt, null, 2);
    }
    if (pinsInput) {
      pinsInput.value = JSON.stringify(pins, null, 2);
    }
  } catch (error) {
    setJson("configJson", { error: error.message });
  }
}

async function refreshNetwork() {
  try {
    const [wifi, mqtt, espnow, peers] = await Promise.all([
      requestJson("/api/network/wifi"),
      requestJson("/api/network/mqtt"),
      requestJson("/api/network/espnow"),
      requestJson("/api/network/espnow/peer"),
    ]);
    setJson("wifiJson", wifi);
    setJson("mqttJson", mqtt);
    setJson("espnowJson", espnow);
    setJson("espnowPeersJson", peers);
  } catch (error) {
    const err = { error: error.message };
    setJson("wifiJson", err);
    setJson("mqttJson", err);
    setJson("espnowJson", err);
    setJson("espnowPeersJson", err);
  }
}

async function sendControl(action) {
  const result = await requestJson("/api/control", {
    method: "POST",
    headers: jsonHeaders(),
    body: JSON.stringify({ action }),
  });
  setJson("actionResult", result);
  await Promise.all([refreshStatus(), refreshNetwork()]);
  return result;
}

function bindEvents() {
  document.querySelectorAll("nav button[data-section]").forEach((button) => {
    button.addEventListener("click", () => showSection(button.dataset.section));
  });

  document.getElementById("refreshAllBtn").addEventListener("click", async () => {
    await Promise.all([refreshStatus(), refreshConfig(), refreshNetwork()]);
  });
  document.getElementById("refreshConfigBtn").addEventListener("click", refreshConfig);
  document.getElementById("espnowRefreshBtn").addEventListener("click", refreshNetwork);

  document.getElementById("applyAudioConfigBtn").addEventListener("click", async () => {
    try {
      const payload = parseJsonInput("audioConfigInput");
      const result = await requestJson("/api/config/audio", {
        method: "POST",
        headers: jsonHeaders(),
        body: JSON.stringify(payload),
      });
      setJson("actionResult", result);
      await Promise.all([refreshConfig(), refreshStatus()]);
    } catch (error) {
      setJson("actionResult", { error: error.message });
    }
  });

  document.getElementById("applyMqttConfigBtn").addEventListener("click", async () => {
    try {
      const payload = parseJsonInput("mqttConfigInput");
      const result = await requestJson("/api/config/mqtt", {
        method: "POST",
        headers: jsonHeaders(),
        body: JSON.stringify(payload),
      });
      setJson("actionResult", result);
      await Promise.all([refreshConfig(), refreshNetwork(), refreshStatus()]);
    } catch (error) {
      setJson("actionResult", { error: error.message });
    }
  });

  document.getElementById("applyPinsConfigBtn").addEventListener("click", async () => {
    try {
      const payload = parseJsonInput("pinsConfigInput");
      const result = await requestJson("/api/config/pins", {
        method: "POST",
        headers: jsonHeaders(),
        body: JSON.stringify(payload),
      });
      setJson("actionResult", result);
      await Promise.all([refreshConfig(), refreshStatus()]);
    } catch (error) {
      setJson("actionResult", { error: error.message });
    }
  });

  document.getElementById("wifiConnectForm").addEventListener("submit", async (event) => {
    event.preventDefault();
    const ssid = document.getElementById("wifiSsid").value.trim();
    const pass = document.getElementById("wifiPass").value;
    try {
      const result = await requestJson("/api/network/wifi/connect", {
        method: "POST",
        headers: jsonHeaders(),
        body: JSON.stringify({ ssid, pass }),
      });
      setJson("actionResult", result);
      await Promise.all([refreshStatus(), refreshNetwork()]);
    } catch (error) {
      setJson("actionResult", { error: error.message });
    }
  });

  document.getElementById("wifiDisconnectBtn").addEventListener("click", async () => {
    try {
      const result = await requestJson("/api/network/wifi/disconnect", {
        method: "POST",
        headers: jsonHeaders(),
        body: "{}",
      });
      setJson("actionResult", result);
      await Promise.all([refreshStatus(), refreshNetwork()]);
    } catch (error) {
      setJson("actionResult", { error: error.message });
    }
  });

  document.getElementById("wifiReconnectBtn").addEventListener("click", async () => {
    try {
      const result = await requestJson("/api/network/wifi/reconnect", {
        method: "POST",
        headers: jsonHeaders(),
        body: "{}",
      });
      setJson("actionResult", result);
      await Promise.all([refreshStatus(), refreshNetwork()]);
    } catch (error) {
      setJson("actionResult", { error: error.message });
    }
  });

  document.getElementById("wifiScanBtn").addEventListener("click", async () => {
    try {
      const result = await requestJson("/api/network/wifi/scan", {
        method: "POST",
        headers: jsonHeaders(),
        body: "{}",
      });
      setJson("actionResult", result);
      await refreshNetwork();
    } catch (error) {
      setJson("actionResult", { error: error.message });
    }
  });

  document.getElementById("mqttConnectBtn").addEventListener("click", async () => {
    try {
      const result = await requestJson("/api/network/mqtt/connect", {
        method: "POST",
        headers: jsonHeaders(),
        body: "{}",
      });
      setJson("actionResult", result);
      await Promise.all([refreshStatus(), refreshNetwork()]);
    } catch (error) {
      setJson("actionResult", { error: error.message });
    }
  });

  document.getElementById("mqttDisconnectBtn").addEventListener("click", async () => {
    try {
      const result = await requestJson("/api/network/mqtt/disconnect", {
        method: "POST",
        headers: jsonHeaders(),
        body: "{}",
      });
      setJson("actionResult", result);
      await Promise.all([refreshStatus(), refreshNetwork()]);
    } catch (error) {
      setJson("actionResult", { error: error.message });
    }
  });

  document.getElementById("mqttPublishForm").addEventListener("submit", async (event) => {
    event.preventDefault();
    const topic = document.getElementById("mqttTopic").value.trim();
    const payload = document.getElementById("mqttPayload").value;
    try {
      const result = await requestJson("/api/network/mqtt/publish", {
        method: "POST",
        headers: jsonHeaders(),
        body: JSON.stringify({ topic, payload }),
      });
      setJson("actionResult", result);
      await Promise.all([refreshStatus(), refreshNetwork()]);
    } catch (error) {
      setJson("actionResult", { error: error.message });
    }
  });

  document.getElementById("espnowOnBtn").addEventListener("click", async () => {
    try {
      const result = await requestJson("/api/network/espnow/on", {
        method: "POST",
        headers: jsonHeaders(),
        body: "{}",
      });
      setJson("actionResult", result);
      await Promise.all([refreshStatus(), refreshNetwork()]);
    } catch (error) {
      setJson("actionResult", { error: error.message });
    }
  });

  document.getElementById("espnowOffBtn").addEventListener("click", async () => {
    try {
      const result = await requestJson("/api/network/espnow/off", {
        method: "POST",
        headers: jsonHeaders(),
        body: "{}",
      });
      setJson("actionResult", result);
      await Promise.all([refreshStatus(), refreshNetwork()]);
    } catch (error) {
      setJson("actionResult", { error: error.message });
    }
  });

  document.getElementById("espnowPeerForm").addEventListener("submit", async (event) => {
    event.preventDefault();
    const mac = document.getElementById("espnowMac").value.trim();
    try {
      const result = await requestJson("/api/network/espnow/peer", {
        method: "POST",
        headers: jsonHeaders(),
        body: JSON.stringify({ mac }),
      });
      setJson("actionResult", result);
      await refreshNetwork();
    } catch (error) {
      setJson("actionResult", { error: error.message });
    }
  });

  document.getElementById("espnowDelBtn").addEventListener("click", async () => {
    const mac = document.getElementById("espnowMac").value.trim();
    try {
      const result = await requestJson("/api/network/espnow/peer", {
        method: "DELETE",
        headers: jsonHeaders(),
        body: JSON.stringify({ mac }),
      });
      setJson("actionResult", result);
      await refreshNetwork();
    } catch (error) {
      setJson("actionResult", { error: error.message });
    }
  });

  document.getElementById("espnowSendForm").addEventListener("submit", async (event) => {
    event.preventDefault();
    const payloadRaw = document.getElementById("espnowPayload").value;
    const payload = parsePayloadValue(payloadRaw);
    try {
      const result = await requestJson("/api/network/espnow/send", {
        method: "POST",
        headers: jsonHeaders(),
        body: JSON.stringify({ payload }),
      });
      setJson("actionResult", result);
      await refreshNetwork();
    } catch (error) {
      setJson("actionResult", { error: error.message });
    }
  });

  document.querySelectorAll("#controlSection button[data-action]").forEach((button) => {
    button.addEventListener("click", async () => {
      try {
        await sendControl(button.dataset.action);
      } catch (error) {
        setJson("actionResult", { error: error.message });
      }
    });
  });

  document.getElementById("rawCommandForm").addEventListener("submit", async (event) => {
    event.preventDefault();
    const action = document.getElementById("rawCommandInput").value.trim();
    try {
      await sendControl(action);
    } catch (error) {
      setJson("actionResult", { error: error.message });
    }
  });
}

document.addEventListener("DOMContentLoaded", async () => {
  bindEvents();
  connectRealtime();
  ensureFallbackPolling();
  await Promise.all([refreshStatus(), refreshConfig(), refreshNetwork()]);
  showSection("dashboard");
});
