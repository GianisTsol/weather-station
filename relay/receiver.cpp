#include <RF24/RF24.h>
#include <sqlite3.h>

#include <iostream>
#include <ctime>
#include <cstring>

// ======================================================
// NRF24
// ======================================================

RF24 radio(17, 0); // CE GPIO22, CSN CE0

const byte address[6] = "NODE1";

// ======================================================
// DATA STRUCT
// MUST MATCH ARDUINO STRUCT EXACTLY
// ======================================================

struct SensorData {
    float temperature;
    float humidity;
    float pressure_kpa;
    float battery_voltage;
};

// ======================================================

sqlite3* db;

// ======================================================

bool initDatabase()
{
    int rc = sqlite3_open("sensor.db", &db);

    if (rc) {
        std::cerr << "Cannot open DB\n";
        return false;
    }

    const char* sql =
        "CREATE TABLE IF NOT EXISTS readings ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "timestamp INTEGER,"
        "temperature REAL,"
        "humidity REAL,"
        "pressure REAL,"
        "battery REAL,"
        "uploaded INTEGER DEFAULT 0"
        ");";

    char* errMsg = nullptr;

    rc = sqlite3_exec(db, sql, nullptr, nullptr, &errMsg);

    if (rc != SQLITE_OK) {
        std::cerr << "SQL error: " << errMsg << "\n";
        sqlite3_free(errMsg);
        return false;
    }

    return true;
}

// ======================================================

void saveReading(const SensorData& data)
{
    sqlite3_stmt* stmt;

    const char* sql =
        "INSERT INTO readings "
        "(timestamp, temperature, humidity, pressure, battery) "
        "VALUES (?, ?, ?, ?, ?);";

    sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);

    sqlite3_bind_int64(stmt, 1, std::time(nullptr));
    sqlite3_bind_double(stmt, 2, data.temperature);
    sqlite3_bind_double(stmt, 3, data.humidity);
    sqlite3_bind_double(stmt, 4, data.pressure_kpa);
    sqlite3_bind_double(stmt, 5, data.battery_voltage);

    sqlite3_step(stmt);

    sqlite3_finalize(stmt);

    std::cout << "Saved reading\n";
}

// ======================================================

int main()
{
    if (!initDatabase()) {
        return 1;
    }

    if (!radio.begin()) {
        std::cerr << "NRF24 init failed\n";
        return 1;
    }

    radio.setPALevel(RF24_PA_LOW);
    radio.setDataRate(RF24_250KBPS);
    radio.setChannel(108);

    radio.openReadingPipe(1, address);

    radio.startListening();

    std::cout << "Listening...\n";

    while (true)
    {
        if (radio.available())
        {
            SensorData data;

            radio.read(&data, sizeof(data));

            std::cout
                << "T=" << data.temperature
                << " H=" << data.humidity
                << " P=" << data.pressure_kpa
                << " B=" << data.battery_voltage
                << "\n";

            saveReading(data);
        }
    }

    sqlite3_close(db);

    return 0;
}