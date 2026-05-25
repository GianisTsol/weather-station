# weather-station

A self-hosted weather station built from scratch — an Arduino reads the sensors, a Raspberry Pi picks up the data over 2.4 GHz radio and forwards it to a VPS, which stores it and serves a live web dashboard.

```
[Arduino]  ──  nRF24L01 (2.4 GHz)  ──  [Raspberry Pi]  ──  HTTPS  ──  [VPS]  ──  [Browser]
 DHT11                                    relay                          Flask
 MPL115A2                                                                SQLite
```

---

## Hardware

| Part | Role |
|------|------|
| Arduino (any) | Reads sensors, transmits over radio |
| DHT11 | Temperature & humidity |
| MPL115A2 | Barometric pressure |
| nRF24L01 | 2.4 GHz wireless link |
| Raspberry Pi | Receives radio packets, relays to the API |

---

## Repository

```
weather-station/
├── station/     Arduino sketch
├── relay/       Raspberry Pi bridge (Python)
└── server/      Flask API + web dashboard
```

**`station/`** — C++ sketch for the Arduino. Samples the DHT11 and MPL115A2 and broadcasts readings as compact radio packets via nRF24L01.

**`relay/`** — lightweight script that runs on the Pi. Listens for incoming nRF24 packets and forwards them to the server API over HTTPS. Runs as a systemd service.

**`server/`** — Flask app running on a VPS. Exposes a POST endpoint for the relay and a GET endpoint for the frontend. Data is stored in SQLite. nginx sits in front and handles SSL.

---

## API

| Method | Endpoint | Auth | Description |
|--------|----------|------|-------------|
| `POST` | `/api/data` | API key | Ingest a reading from the relay |
| `GET` | `/api/latest` | — | Most recent reading |
| `GET` | `/api/history?limit=N` | — | Last N readings (max 1000) |

Auth is done via the `X-API-Key` request header.

---

## License

MIT — [GianisTsol](https://github.com/GianisTsol)