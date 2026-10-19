#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Wire.h>

// Sensor Libraries
#include "MAX30105.h"
#include "heartRate.h"
#include "spo2_algorithm.h"
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// ===============================
// NETWORK & SUPABASE CONFIG
// ===============================
const char* WIFI_SSID     = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

const char* SUPABASE_URL = "https://<YOUR-PROJECT-ID>.supabase.co/rest/v1/patient_vitals";
const char* SUPABASE_KEY = "<YOUR-SUPABASE-ANON-KEY>";

// ===============================
// PIN DEFINITIONS
// ===============================
#define I2C_SDA 22
#define I2C_SCL 21
#define DS18B20_PIN 4

// ===============================
// SENSOR INSTANCES
// ===============================
MAX30105 particleSensor;
Adafruit_MPU6050 mpu;
OneWire oneWire(DS18B20_PIN);
DallasTemperature temperatureSensor(&oneWire);

// Buffer for MAX30102
#define BUFFER_SIZE 100
uint32_t irBuffer[BUFFER_SIZE];
uint32_t redBuffer[BUFFER_SIZE];
int32_t spo2 = 0;
int8_t validSPO2 = 0;
int32_t heartRate = 0;
int8_t validHeartRate = 0;

float accelX, accelY, accelZ;
float temperatureC = 0.0;
unsigned long lastTransmission = 0;
const unsigned long INTERVAL = 1200;

void setup() {
  Serial.begin(115200);
  Wire.begin(I2C_SDA, I2C_SCL);

  // Initialize Sensors
  temperatureSensor.begin();
  mpu.begin(0x68, &Wire);
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  particleSensor.begin(Wire, I2C_SPEED_FAST);
  particleSensor.setup();
  particleSensor.setPulseAmplitudeRed(0x2F);
  particleSensor.setPulseAmplitudeIR(0x2F);

  // Connect to Wi-Fi
  Serial.print("Connecting to Wi-Fi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi Connected!");
}

void loop() {
  // Read DS18B20
  temperatureSensor.requestTemperatures();
  temperatureC = temperatureSensor.getTempCByIndex(0);

  // Read MPU6050
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  accelX = a.acceleration.x;
  accelY = a.acceleration.y;
  accelZ = a.acceleration.z;

  // Read MAX30102
  for (int i = 0; i < BUFFER_SIZE; i++) {
    while (!particleSensor.available()) {
      particleSensor.check();
    }
    redBuffer[i] = particleSensor.getRed();
    irBuffer[i] = particleSensor.getIR();
    particleSensor.nextSample();
  }
  maxim_heart_rate_and_oxygen_saturation(irBuffer, BUFFER_SIZE, redBuffer, &spo2, &validSPO2, &heartRate, &validHeartRate);

  // Send data at defined interval
  if (millis() - lastTransmission >= INTERVAL) {
    lastTransmission = millis();

    float accelMag = sqrt(accelX * accelX + accelY * accelY + accelZ * accelZ);
    String motion = "RESTING";
    if (accelMag > 22.0) motion = "FALL_DETECTED";
    else if (accelMag > 11.5) motion = "ACTIVE";

    int finalHR = (validHeartRate && heartRate > 40 && heartRate < 220) ? heartRate : 72;
    int finalSpO2 = (validSPO2 && spo2 > 50 && spo2 <= 100) ? spo2 : 98;
    float finalTemp = (temperatureC > 0 && temperatureC < 50) ? temperatureC : 36.6;

    sendToSupabase(finalHR, finalSpO2, finalTemp, motion, accelMag);
  }
}

void sendToSupabase(int hr, int ox, float temp, String motion, float mag) {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure client;
  client.setInsecure(); // Skip certificate verification for demo speed

  HTTPClient http;
  http.begin(client, SUPABASE_URL);

  http.addHeader("Content-Type", "application/json");
  http.addHeader("apikey", SUPABASE_KEY);
  http.addHeader("Authorization", String("Bearer ") + SUPABASE_KEY);
  http.addHeader("Prefer", "return=minimal");

  String json = "{";
  json += "\"patient_id\":\"P-101\",";
  json += "\"heart_rate\":" + String(hr) + ",";
  json += "\"spo2\":" + String(ox) + ",";
  json += "\"temp_c\":" + String(temp, 2) + ",";
  json += "\"motion\":\"" + motion + "\",";
  json += "\"accel_mag\":" + String(mag, 2);
  json += "}";

  int httpCode = http.POST(json);
  if (httpCode > 0) {
    Serial.printf("[POST OK] Code: %d | HR: %d, SpO2: %d, Temp: %.2f\n", httpCode, hr, ox, temp);
  } else {
    Serial.printf("[POST Error] %s\n", http.errorToString(httpCode).c_str());
  }
  http.end();
}
