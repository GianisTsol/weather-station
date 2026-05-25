# relay

The Raspberry Pi side of the weather station. Two processes, one responsibility each:

- **`relay`** (C++) — listens on the nRF24L01 and writes incoming packets directly to a local SQLite database
- **`sync`** (Python) — watches that database and uploads unsynced rows to the server API

Splitting them this way means readings are never lost if the network is down — the C++ process keeps writing locally, and sync catches up whenever connectivity is restored.

---

## relay (C++)

Reads the nRF24L01 at 250kbps on channel 108, address `NODE1`. Each packet is a 16-byte struct:

```cpp
struct SensorData {
    float temperature;
    float humidity;
    float pressure_kpa;
    float battery_voltage;
};
```

Writes every packet to `sensor.db` with `uploaded = 0`.

**Build:**
```bash
g++ relay.cpp -o relay -lrf24 -lsqlite3
```

**Run:**
```bash
./relay
```

---

## sync (Python)

Polls `sensor.db` for rows where `uploaded = 0`, POSTs them to the server API, and marks them done.

**Install:**
```bash
pip install -r requirements.txt
```

**Run:**
```bash
export WEATHER_API_URL="https://yoursite.com/api/data"
export WEATHER_API_KEY="your-secret-key"
python sync.py
```

| Variable | Default | Description |
|----------|---------|-------------|
| `SENSOR_DB` | `sensor.db` | Path to the SQLite database |
| `WEATHER_API_URL` | — | Full URL of the `/api/data` endpoint |
| `WEATHER_API_KEY` | — | Shared secret with the server |
| `POLL_INTERVAL` | `30` | Seconds between sync attempts |

---

## Running on boot

Both processes should run as systemd services. Example for `sync`:

```ini
# /etc/systemd/system/weather-sync.service
[Unit]
Description=Weather Station sync daemon
After=network-online.target
Wants=network-online.target

[Service]
ExecStart=/usr/bin/python3 /home/pi/weather-station/relay/sync.py
Environment=WEATHER_API_URL=https://yoursite.com/api/data
Environment=WEATHER_API_KEY=your-secret-key
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
```

```bash
sudo systemctl enable weather-sync
sudo systemctl start weather-sync
```

Do the same for `relay` with `ExecStart=/home/pi/weather-station/relay/relay`.