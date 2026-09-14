#include <Wire.h>
#include <Adafruit_MPL115A2.h>
#include <DHT.h>
#include <SPI.h>
#include <RF24.h>

#include <avr/sleep.h>
#include <avr/wdt.h>

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
#define BATTERY_GATE_PIN 4   // bottom leg of divider, was tied to GND

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
// WATCHDOG / SLEEP
// ======================================================

ISR(WDT_vect)
{
  // Watchdog interrupt wakes the MCU
}

void setupWatchdog()
{
  cli();

  MCUSR &= ~(1 << WDRF);

  WDTCSR |= (1 << WDCE) | (1 << WDE);

  // ~8 second watchdog interrupt
  WDTCSR =
    (1 << WDIE) |
    (1 << WDP3) |
    (1 << WDP0);

  sei();
}

void sleep8s()
{
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);

  sleep_enable();

  sleep_cpu();

  sleep_disable();
}

void sleep60s()
{
  // ~64 seconds asleep
  for (uint8_t i = 0; i < 7; i++) {
    sleep8s();
  }
}

// ======================================================
// BATTERY MEASUREMENT
// Divider:
// Battery -> 100k -> A0 -> 220k -> BATTERY_GATE_PIN (gated, was GND)
// 0.1uF cap on A0 for noise filtering -> slows settling, see delay below
// ======================================================

float readBatteryVoltage()
{
  pinMode(BATTERY_GATE_PIN, OUTPUT);
  digitalWrite(BATTERY_GATE_PIN, LOW);   // close the loop to ground

  delay(60); // let divider + 0.1uF cap settle (RC ~6.9ms, ~2x margin)

  analogRead(BATTERY_PIN);          // throwaway read, clears ADC S/H cap
  int raw = analogRead(BATTERY_PIN); // real reading

  pinMode(BATTERY_GATE_PIN, INPUT); // open the loop, stop the leak

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

  // DHT
  dht.begin();

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

    // Automatic retries / ACK
    radio.setRetries(5, 15);

    // 16-bit CRC
    radio.setCRCLength(RF24_CRC_16);

    radio.stopListening();

    // Radio off between transmissions
    radio.powerDown();

    DBG_PRINTLN("NRF24 OK");
  }

  // Watchdog
  setupWatchdog();
}

// ======================================================

void loop()
{
  // ------------------------------------------
  // Power DHT
  // ------------------------------------------

  digitalWrite(DHT_POWER_PIN, HIGH);

  delay(2000);

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

  radio.powerUp();

  // Allow NRF24 to wake up
  delayMicroseconds(1500);

  bool ok = radio.write(&data, sizeof(data));

  if (ok) {
    DBG_PRINTLN("Send OK");
  } else {
    DBG_PRINTLN("Send failed");
  }

  // Radio off until next transmission
  radio.powerDown();

  DBG_PRINTLN("----------------");

  // ------------------------------------------
  // Sleep
  // ------------------------------------------

  sleep60s();
}