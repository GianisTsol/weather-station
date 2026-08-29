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
API_URL      = os.environ.get("WEATHER_API_URL",  "https://yoursite.com/api")
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

def get_last_id(conn) -> list[dict]:
    cur = conn.execute(
        "SELECT id"
        "FROM readings ORDER BY id ASC LIMIT 1"
    )
    cols = [c[0] for c in cur.description]
    res = [dict(zip(cols, row)) for row in cur.fetchall()]

    if len(res) > 0:
        return res[0]["id"]
    else:
        return 0

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
            f"{API_URL}/data",
            json=payload,
            headers={"X-API-Key": API_KEY},
            timeout=10,
        )
        resp.raise_for_status()
        return True
    except requests.RequestException as e:
        print(f"upload failed (id={row['id']}): {e}")
        print(f"Payload: {[v for k, v in row.items()]}")
        return False

def verify_consistency():
    conn = sqlite3.connect(DB_PATH, timeout=10)
    
    latest = get_last_id(conn)
    
    resp = requests.get(f"{API_URL}/latest")
    resp.raise_for_status()

    remote_latest = resp.json()["id"]

    if remote_latest < latest:
        n = latest - remote_latest
        print("Found {n} not pushed rows.")
        conn.execute("UPDATE readings SET uploaded = 0 WHERE id > ?", (remote_latest["id"],))
        conn.commit()
    if remote_latest == latest:
        print("Databases in sync")
    if remote_latest > latest:
        print("How did we get here?")


def main():
    print(f"[sync] watching {DB_PATH}, polling every {POLL_INTERVAL}s")
    while True:
        try:
            conn = sqlite3.connect(DB_PATH, timeout=10)
            try:
                pending = get_pending(conn)
                if pending:
                    print(f"{len(pending)} pending row(s)")
                    for row in pending:
                        if upload(row):
                            mark_uploaded(conn, row["id"])
            finally:
                conn.close()
        except Exception as e:
            print(e)

        time.sleep(POLL_INTERVAL)

if __name__ == "__main__":
    try:
        verify_consistency()

        main()
    except KeyboardInterrupt:
        print("\n[sync] stopped")