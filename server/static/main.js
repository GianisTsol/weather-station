const POLL_INTERVAL  = 30_000; // ms
const DATA_INTERVAL  = 60;     // seconds between readings from the station
const URL_LATEST  = window.APP_CONFIG.apiLatest;
const URL_HISTORY = window.APP_CONFIG.apiHistory;

// Range definitions: label → seconds of history
const RANGES = [
{ label: "1H",  seconds: 3600 },
{ label: "6H",  seconds: 21600 },
{ label: "1D",  seconds: 86400 },
{ label: "1W",  seconds: 604800 },
{ label: "1M",  seconds: 2592000 },
];
const DEFAULT_RANGE = "1H";

const $ = id => document.getElementById(id);
const status = $("status");

// ── Timestamp ─────────────────────────────────────────────
function parseTs(ts) {
const n = Number(ts);
if (!isNaN(n) && n > 1_000_000_000) return new Date(n * 1000);
return new Date(String(ts).includes("Z") ? ts : ts + "Z");
}

function fmtLabel(ts, rangeSecs) {
const d = parseTs(ts);
if (isNaN(d)) return "—";
// show date+time for ranges > 1 day, just time otherwise
if (rangeSecs > 86400) {
    return d.toLocaleDateString([], { month: "short", day: "numeric" })
        + " " + d.toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" });
}
return d.toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" });
}

function shortTime(ts) {
const d = parseTs(ts);
if (isNaN(d)) return "—";
return d.toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" });
}

// ── Charts ────────────────────────────────────────────────
function makeChart(id, color) {
return new Chart($(id), {
    type: "line",
    data: {
    labels: [],
    datasets: [{
        data: [],
        borderColor: color,
        borderWidth: 2,
        pointRadius: 2,
        pointBackgroundColor: color,
        fill: true,
        backgroundColor: color + "18",
        tension: 0.3
    }]
    },
    options: {
    animation: false,
    responsive: true,
    plugins: { legend: { display: false }, tooltip: { mode: "index" } },
    scales: {
        x: {
        ticks: { color: "#8b949e", font: { family: "IBM Plex Mono", size: 10 }, maxTicksLimit: 10 },
        grid: { color: "#21262d" }
        },
        y: {
        ticks: { color: "#8b949e", font: { family: "IBM Plex Mono", size: 10 } },
        grid: { color: "#21262d" },
        grace: "10%"
        }
    }
    }
});
}

const charts = {
temp: makeChart("chart-temp", "#ff7b72"),
hum:  makeChart("chart-hum",  "#58a6ff"),
bat:  makeChart("chart-bat",  "#d29922"),
};

const chartKeys = { temp: "temperature", hum: "humidity", bat: "bat_voltage" };

// active range per chart
const activeRange = { temp: DEFAULT_RANGE, hum: DEFAULT_RANGE, bat: DEFAULT_RANGE };

// ── Range buttons ─────────────────────────────────────────
document.querySelectorAll(".range-btns").forEach(container => {
const chartId = container.dataset.chart;
RANGES.forEach(r => {
    const btn = document.createElement("button");
    btn.textContent = r.label;
    if (r.label === DEFAULT_RANGE) btn.classList.add("active");
    btn.onclick = () => {
    activeRange[chartId] = r.label;
    container.querySelectorAll("button").forEach(b => b.classList.remove("active"));
    btn.classList.add("active");
    fetchHistory(chartId);
    };
    container.appendChild(btn);
});
});

// ── Helpers ───────────────────────────────────────────────
function fmt(val, digits = 1) {
return val != null ? Number(val).toFixed(digits) : "—";
}

function updateChart(chartId, rows) {
const rangeSecs = RANGES.find(r => r.label === activeRange[chartId]).seconds;
// downsample if too many points (keep max ~300 points for performance)
const step = Math.max(1, Math.floor(rows.length / 300));
const pts  = [...rows].reverse().filter((_, i) => i % step === 0);
const chart = charts[chartId];
chart.data.labels = pts.map(r => fmtLabel(r.timestamp, rangeSecs));
chart.data.datasets[0].data = pts.map(r => r[chartKeys[chartId]]);
chart.update();
}

// ── Fetch ─────────────────────────────────────────────────
function fetchLatest() {
fetch(URL_LATEST)
    .then(r => r.json())
    .then(d => {
    if (!d || !d.temperature) return;
    $("val-temp").innerHTML  = `${fmt(d.temperature)}<span class="unit">°C</span>`;
    $("val-hum").innerHTML   = `${fmt(d.humidity)}<span class="unit">%</span>`;
    $("val-press").innerHTML = d.pressure    != null ? `${fmt(d.pressure, 2)}<span class="unit">kPa</span>` : "—";
    $("val-bat").innerHTML   = d.bat_voltage != null ? `${fmt(d.bat_voltage, 2)}<span class="unit">V</span>` : "—";
    $("ts").textContent = "updated " + shortTime(d.timestamp);
    status.textContent = "live";
    status.className = "ok";
    })
    .catch(() => {
    status.textContent = "offline";
    status.className = "err";
    });
}

function fetchHistory(chartId = null) {
const targets = chartId ? [chartId] : Object.keys(charts);
targets.forEach(id => {
    const rangeSecs = RANGES.find(r => r.label === activeRange[id]).seconds;
    const since     = Math.floor(Date.now() / 1000) - rangeSecs;
    // limit as a safety cap: range / interval + 10% headroom
    const limit     = Math.ceil(rangeSecs / DATA_INTERVAL * 1.1);
    fetch(`${URL_HISTORY}?since=${since}&interval=${DATA_INTERVAL}`)
    .then(r => r.json())
    .then(rows => {
        if (!rows.length) { $("no-data").style.display = "block"; return; }
        $("no-data").style.display = "none";
        updateChart(id, rows);
    });
});
}

fetchLatest();
fetchHistory();
setInterval(fetchLatest,  POLL_INTERVAL);
setInterval(fetchHistory, POLL_INTERVAL * 2);
