import os
import sqlite3
from datetime import datetime
from functools import wraps
from flask import Flask, request, jsonify, render_template, g
from werkzeug.middleware.proxy_fix import ProxyFix


app = Flask(__name__)
app.config["APPLICATION_ROOT"] = os.environ.get("APP_ROOT", "/")

app.wsgi_app = ProxyFix(app.wsgi_app, x_prefix=1)

DATABASE = "weather.db"
API_KEY  = os.environ.get("WEATHER_API_KEY", "changeme")


# ---------- DB helpers ----------

def get_db():
    db = getattr(g, "_database", None)
    if db is None:
        db = g._database = sqlite3.connect(DATABASE)
        db.row_factory = sqlite3.Row
    return db

@app.teardown_appcontext
def close_connection(exception):
    db = getattr(g, "_database", None)
    if db is not None:
        db.close()

def init_db():
    with app.app_context():
        db = get_db()
        with app.open_resource("schema.sql", mode="r") as f:
            db.cursor().executescript(f.read())
        db.commit()


# ---------- Auth ----------

def require_api_key(f):
    @wraps(f)
    def decorated(*args, **kwargs):
        key = request.headers.get("X-API-Key")
        if key != API_KEY:
            return jsonify({"error": "Unauthorized"}), 401
        return f(*args, **kwargs)
    return decorated


# ---------- Routes ----------

@app.route("/")
def index():
    return render_template("index.html")

@app.route("/api/data", methods=["POST"])
@require_api_key
def ingest():
    data = request.get_json(silent=True)
    if not data:
        return jsonify({"error": "Invalid JSON"}), 400

    required = {"temperature", "humidity"}
    if not required.issubset(data.keys()):
        return jsonify({"error": f"Missing fields: {required - data.keys()}"}), 400

    db = get_db()
    try:
        db.execute(
            "INSERT INTO readings (temperature, humidity, pressure, bat_voltage, timestamp) "
            "VALUES (?, ?, ?, ?, ?)",
            (
                data["temperature"],
                data["humidity"],
                data.get("pressure"),
                data.get("bat_voltage"),
                data.get("timestamp") or int(datetime.utcnow().timestamp()),
            ),
        )
    except Exception as e:
        print(f"Inserting {data}")
        print(f"Error while inserting record: {e}")
    db.commit()
    return jsonify({"status": "ok"}), 201

@app.route("/api/latest")
def latest():
    row = get_db().execute(
        "SELECT * FROM readings ORDER BY id DESC LIMIT 1"
    ).fetchone()
    if not row:
        return jsonify({}), 204
    return jsonify(dict(row))

@app.route("/api/history")
def history():
    since    = int(request.args.get("since",    0))
    interval = int(request.args.get("interval", 60))
    interval = max(60, min(interval, 86400))

    rows = get_db().execute(
        """
        SELECT
            (timestamp / :interval) * :interval AS timestamp,
            AVG(temperature)  AS temperature,
            AVG(humidity)     AS humidity,
            AVG(pressure)     AS pressure,
            AVG(bat_voltage)  AS bat_voltage
        FROM readings
        WHERE timestamp >= :since
        GROUP BY timestamp / :interval
        ORDER BY timestamp ASC
        """,
        {"interval": interval, "since": since}
    ).fetchall()

    return jsonify([dict(r) for r in rows])


# ---------- Entry point ----------

if __name__ == "__main__":
    if not os.path.exists(DATABASE):
        init_db()
    app.run(host="127.0.0.1", port=5000)