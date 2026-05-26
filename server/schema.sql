CREATE TABLE IF NOT EXISTS readings (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    temperature REAL    NOT NULL,
    humidity    REAL    NOT NULL,
    pressure    REAL,
    bat_voltage REAL    NOT NULL,

    timestamp   TEXT    NOT NULL
);
