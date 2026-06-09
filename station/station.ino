#include <Wire.h>
#include <Adafruit_MPL115A2.h>
#include <DHT.h>
#include <SPI.h>
#include <RF24.h>

// ======================================================
// DEBUG
// ======================================================

#define DEBUG 0

#if DEBUG
  #define DBG_BEGIN(x) Serial.begin(x)
  #define DBG_PRINT(x) Serial.print(x)
  #define DBG_PRINTLN(x) Serial.println(x)
#else
  #define DBG_BEGIN(x)
  #define DBG_PRINT(x)
  #define DBG_PRINTLN(x)
#endif

// ======================================================
// PINS
// ======================================================

#define DHT_POWER_PIN 3
#define DHT_DATA_PIN  2
#define BATTERY_PIN   A0

// NRF24
#define NRF_CE_PIN   8
#define NRF_CSN_PIN 9

// ======================================================
// DHT
// ======================================================

#define DHTTYPE DHT11
DHT dht(DHT_DATA_PIN, DHTTYPE);

// ======================================================
// MPL115A2
// ======================================================

Adafruit_MPL115A2 mpl115a2;

// ======================================================
// NRF24
// ======================================================

RF24 radio(NRF_CE_PIN, NRF_CSN_PIN);

const byte address[6] = "NODE1";

// ======================================================
// DATA STRUCT
// ======================================================

struct SensorData {
  float temperature;
  float humidity;
  float pressure_kpa;
  float battery_voltage;
};

SensorData data;

// ======================================================
// BATTERY MEASUREMENT
// Divider:
// Battery -> 100k -> A0 -> 220k -> GND
// ======================================================

float readBatteryVoltage()
{
  int raw = analogRead(BATTERY_PIN);

  float adcVoltage = (raw / 1023.0) * 3.3;

  // Divider compensation
  float batteryVoltage = adcVoltage * ((100.0 + 220.0) / 220.0);

  return batteryVoltage;
}

// ======================================================

void setup()
{
  DBG_BEGIN(115200);

  DBG_PRINTLN("Boot");

  // DHT power
  pinMode(DHT_POWER_PIN, OUTPUT);
  digitalWrite(DHT_POWER_PIN, LOW);

  // MPL115A2
  if (!mpl115a2.begin()) {
    DBG_PRINTLN("MPL115A2 not found");
  } else {
    DBG_PRINTLN("MPL115A2 OK");
  }

  // NRF24
  if (!radio.begin()) {
    DBG_PRINTLN("NRF24 failed");
  } else {
    radio.openWritingPipe(address);
    radio.setPALevel(RF24_PA_LOW);
    radio.setDataRate(RF24_250KBPS);
    radio.setChannel(108);
    radio.stopListening();

    DBG_PRINTLN("NRF24 OK");
  }
}

// ======================================================

void loop()
{
  // ------------------------------------------
  // Power DHT
  // ------------------------------------------

  digitalWrite(DHT_POWER_PIN, HIGH);

  delay(2000);

  dht.begin();

  // ------------------------------------------
  // Read DHT
  // ------------------------------------------

  data.temperature = dht.readTemperature();
  data.humidity = dht.readHumidity();

  // ------------------------------------------
  // Power off DHT
  // ------------------------------------------

  digitalWrite(DHT_POWER_PIN, LOW);

  // ------------------------------------------
  // Read pressure
  // ------------------------------------------

  data.pressure_kpa = mpl115a2.getPressure();

  // ------------------------------------------
  // Read battery
  // ------------------------------------------

  data.battery_voltage = readBatteryVoltage();

  // ------------------------------------------
  // Debug output
  // ------------------------------------------

  DBG_PRINT("Temp: ");
  DBG_PRINT(data.temperature);
  DBG_PRINTLN(" C");

  DBG_PRINT("Humidity: ");
  DBG_PRINT(data.humidity);
  DBG_PRINTLN(" %");

  DBG_PRINT("Pressure: ");
  DBG_PRINT(data.pressure_kpa);
  DBG_PRINTLN(" kPa");

  DBG_PRINT("Battery: ");
  DBG_PRINT(data.battery_voltage);
  DBG_PRINTLN(" V");

  // ------------------------------------------
  // Send packet
  // ------------------------------------------

  bool ok = radio.write(&data, sizeof(data));

  if (ok) {
    DBG_PRINTLN("Send OK");
  } else {
    DBG_PRINTLN("Send failed");
  }

  DBG_PRINTLN("----------------");

  delay(60000);
}