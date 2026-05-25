"""
relay.py — Raspberry Pi relay
Listens for nRF24L01 packets from the Arduino station and forwards them to the API.

Packet format (matches Arduino struct):
  struct Payload {
    float temperature;
    float humidity;
    float pressure;
    float bat_voltage;
  };
"""

import struct
import time
import os
import requests
from RF24 import RF24, RF24_PA_LOW, RF24_250KBPS

# ── Config ────────────────────────────────────────────────────────────────────

API_URL  = os.environ.get("WEATHER_API_URL", "https://yoursite.com/api/data")
API_KEY  = os.environ.get("WEATHER_API_KEY", "changeme")

CE_PIN   = 17   # change to match your wiring
CSN_PIN  = 0    # SPI bus 0 → GPIO8; SPI bus 1 → GPIO7

CHANNEL  = 108   # must match the Arduino sketch
ADDRESS  = b"NODE1"  # must match the Arduino sketch

PAYLOAD_FORMAT = "<ffff"  # 4 little-endian floats: temp, humidity, pressure, bat_voltage
PAYLOAD_SIZE   = struct.calcsize(PAYLOAD_FORMAT)  # 16 bytes

# ── Radio setup ───────────────────────────────────────────────────────────────

radio = RF24(CE_PIN, CSN_PIN)

def init_radio():
    if not radio.begin():
        raise RuntimeError("nRF24L01 not responding — check wiring")

    radio.setChannel(CHANNEL)
    radio.setDataRate(RF24_250KBPS)
    radio.setPALevel(RF24_PA_LOW)
    radio.setPayloadSize(PAYLOAD_SIZE)
    radio.openReadingPipe(1, ADDRESS)
    radio.startListening()
    print(f"[radio] listening on channel {CHANNEL}, address {ADDRESS.decode()}")

# ── API ───────────────────────────────────────────────────────────────────────

def upload(data: dict):
    try:
        resp = requests.post(
            API_URL,
            json=data,
            headers={"X-API-Key": API_KEY},
            timeout=10,
        )
        resp.raise_for_status()
        print(f"[api] uploaded: {data}")
    except requests.RequestException as e:
        print(f"[api] upload failed: {e}")

# ── Main loop ─────────────────────────────────────────────────────────────────

def main():
    init_radio()

    while True:
        if radio.available():
            raw = radio.read(PAYLOAD_SIZE)

            try:
                temp, humidity, pressure, bat_voltage = struct.unpack(PAYLOAD_FORMAT, raw)
            except struct.error as e:
                print(f"[radio] bad packet: {e}")
                continue

            # Sanity checks — discard obviously corrupt readings
            if not (-40 <= temp <= 80):
                print(f"[radio] temperature out of range ({temp:.1f}°C), discarding")
                continue
            if not (0 <= humidity <= 100):
                print(f"[radio] humidity out of range ({humidity:.1f}%), discarding")
                continue

            upload({
                "temperature":  round(temp, 2),
                "humidity":     round(humidity, 2),
                "pressure":     round(pressure, 2),
                "bat_voltage":  round(bat_voltage, 3),
            })

        else:
            time.sleep(0.05)  # avoid busy-looping

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\n[relay] stopped")
    finally:
        radio.stopListening()
