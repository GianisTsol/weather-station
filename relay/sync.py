"""
sync.py — upload daemon
Watches the local SQLite database written by the C++ relay and uploads
any unsynced readings to the server API, then marks them as uploaded.
"""

import sqlite3
import time
import os
import requests

# ── Config ────────────────────────────────────────────────────────────────────

DB_PATH      = os.environ.get("SENSOR_DB",       "sensor.db")
API_URL      = os.environ.get("WEATHER_API_URL",  "https://yoursite.com/api/data")
API_KEY      = os.environ.get("WEATHER_API_KEY",  "changeme")
POLL_INTERVAL = int(os.environ.get("POLL_INTERVAL", 30))   # seconds

# ── DB ────────────────────────────────────────────────────────────────────────

def get_pending(conn) -> list[dict]:
    cur = conn.execute(
        "SELECT id, timestamp, temperature, humidity, pressure, battery "
        "FROM readings WHERE uploaded = 0 ORDER BY id ASC"
    )
    cols = [c[0] for c in cur.description]
    return [dict(zip(cols, row)) for row in cur.fetchall()]

def mark_uploaded(conn, row_id: int):
    conn.execute("UPDATE readings SET uploaded = 1 WHERE id = ?", (row_id,))
    conn.commit()

# ── API ───────────────────────────────────────────────────────────────────────

def upload(row: dict) -> bool:
    payload = {
        "temperature": row["temperature"],
        "humidity":    row["humidity"],
        "pressure":    row["pressure"],
        "bat_voltage": row["battery"],
        "timestamp":   row["timestamp"],
    }
    try:
        resp = requests.post(
            API_URL,
            json=payload,
            headers={"X-API-Key": API_KEY},
            timeout=10,
        )
        resp.raise_for_status()
        return True
    except requests.RequestException as e:
        print(f"[sync] upload failed (id={row['id']}): {e}")
        return False

# ── Main loop ─────────────────────────────────────────────────────────────────

def main():
    print(f"[sync] watching {DB_PATH}, polling every {POLL_INTERVAL}s")
    conn = sqlite3.connect(DB_PATH, check_same_thread=False)

    while True:
        pending = get_pending(conn)

        if pending:
            print(f"[sync] {len(pending)} pending row(s)")
            for row in pending:
                if upload(row):
                    mark_uploaded(conn, row["id"])

        time.sleep(POLL_INTERVAL)

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\n[sync] stopped")