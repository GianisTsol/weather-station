CREATE TABLE IF NOT EXISTS readings (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    temperature REAL    NOT NULL,
    humidity    REAL    NOT NULL,
    pressure    REAL,
    timestamp   TEXT    NOT NULL
);
