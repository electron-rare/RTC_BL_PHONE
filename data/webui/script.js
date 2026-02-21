const SECTION_MAP = {
  dashboard: "dashboardSection",
  config: "configSection",
  network: "networkSection",
  control: "controlSection",
};

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

async function refreshStatus() {
  const line = document.getElementById("statusLine");
  try {
    const status = await requestJson("/api/status");
    const telephonyState = status.telephony?.state || "n/a";
    const hook = status.telephony?.hook || "n/a";
    const wifiState = status.wifi?.state || "n/a";
    const mqttConnected = status.mqtt?.connected ? "on" : "off";
    const peers = status.espnow?.peer_count ?? 0;
    line.textContent =
      `board=${status.board_profile || "n/a"} telephony=${telephonyState} hook=${hook} ` +
      `wifi=${wifiState} mqtt=${mqttConnected} espnow_peers=${peers}`;
    setJson("statusJson", status);
  } catch (error) {
    line.textContent = `Erreur statut: ${error.message}`;
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

  document.getElementById("espnowOnBtn").addEventListener("click", async () => {
    try {
      await sendControl("ESPNOW_ON");
    } catch (error) {
      setJson("actionResult", { error: error.message });
    }
  });

  document.getElementById("espnowOffBtn").addEventListener("click", async () => {
    try {
      await sendControl("ESPNOW_OFF");
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
    const target = document.getElementById("espnowTarget").value.trim();
    const payloadRaw = document.getElementById("espnowPayload").value;
    const payload = parsePayloadValue(payloadRaw);
    try {
      const result = await requestJson("/api/network/espnow/send", {
        method: "POST",
        headers: jsonHeaders(),
        body: JSON.stringify({ mac: target, payload }),
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
  await Promise.all([refreshStatus(), refreshConfig(), refreshNetwork()]);
  showSection("dashboard");
});
