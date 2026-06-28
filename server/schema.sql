CREATE TABLE IF NOT EXISTS readings (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,

    timestamp   INTEGER    NOT NULL,

    temperature REAL    NOT NULL,
    humidity    REAL    NOT NULL,
    pressure    REAL,
    bat_voltage REAL    NOT NULL

);
