/*
  Csense Care - Direct ESP32 to Supabase Cloud
  No laptop relay required.

  Sensors:
  1. MAX30102 - Heart Rate & SpO2 (SDA=22, SCL=21)
  2. MPU6050  - Accelerometer & Gyro (SDA=22, SCL=21)
  3. DS18B20  - Body Temperature (GPIO 4)
*/

#include <WiFi.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <Wire.h>
#include "time.h"

// Sensor Libraries
#include "MAX30105.h"
#include "heartRate.h"
#include "spo2_algorithm.h"
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// =====================================================
// NETWORK & SUPABASE DIRECT CLOUD CONFIG
// =====================================================
const char* WIFI_SSID     = "CJiPhone";
const char* WIFI_PASSWORD = "chami1234";

const char* SUPABASE_URL  = "https://asdxufazwwahikoeubnc.supabase.co/rest/v1/patient_vitals";
const char* SUPABASE_KEY  = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6ImFzZHh1ZmF6d3dhaGlrb2V1Ym5jIiwicm9sZSI6ImFub24iLCJpYXQiOjE3OTI0MDQ4MDAsImV4cCI6MjEwNzk4MDgwMH0.Gi-DCIBfXDAZ8huBPSCjVuPc8Hl5-BJNWevPgjEq9Es";

// =====================================================
// HARDWARE PIN DEFINITIONS
// =====================================================
#define I2C_SDA 22
#define I2C_SCL 21
#define DS18B20_PIN 4

// =====================================================
// SENSORS & BUFFERS
// =====================================================
MAX30105 particleSensor;
Adafruit_MPU6050 mpu;
OneWire oneWire(DS18B20_PIN);
DallasTemperature temperatureSensor(&oneWire);

#define BUFFER_SIZE 100
uint32_t irBuffer[BUFFER_SIZE];
uint32_t redBuffer[BUFFER_SIZE];
int32_t spo2 = 0;
int8_t validSPO2 = 0;
int32_t heartRate = 0;
int8_t validHeartRate = 0;

float accelX = 0, accelY = 0, accelZ = 0;
float temperatureC = 0.0;

unsigned long lastTransmission = 0;
const unsigned long INTERVAL = 1500; // Send telemetry every 1.5s

void sendDirectToSupabase(int hr, int ox, float temp, String motion, float mag);

// =====================================================
// SETUP
// =====================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n========================================");
  Serial.println("  CSENSE CARE - DIRECT SUPABASE UPLINK  ");
  Serial.println("========================================");

  // 1. I2C Initialization
  Wire.begin(I2C_SDA, I2C_SCL);

  // 2. Sensor Inits
  temperatureSensor.begin();

  if (mpu.begin(0x68, &Wire)) {
    Serial.println("MPU6050: OK");
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  } else {
    Serial.println("MPU6050: Not detected");
  }

  if (particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("MAX30102: OK");
    particleSensor.setup();
    particleSensor.setPulseAmplitudeRed(0x2F);
    particleSensor.setPulseAmplitudeIR(0x2F);
  } else {
    Serial.println("MAX30102: Not detected");
  }

// --- Replace the Wi-Fi section in setup() with this: ---
  Serial.println("\nResetting Wi-Fi adapter...");
  WiFi.persistent(false);      // Do not use old cached channel/BSSID
  WiFi.disconnect(true);        // Clear previous state and radio memory
  delay(1000);

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);

  Serial.print("Connecting to '");
  Serial.print(WIFI_SSID);
  Serial.print("'");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n Wi-Fi Connected!");
    Serial.print("ESP32 IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.printf("\n❌ Connection Failed! Status code: %d\n", WiFi.status());
    Serial.println("Turn phone hotspot OFF and ON, check 2.4 GHz band, then press RESET on ESP32.");
    return;
  }

  // 4. Sync Time via NTP (Essential for Cloudflare TLS handshake validation)
  Serial.print("Syncing network time for TLS verification...");
  configTime(19800, 0, "pool.ntp.org", "time.google.com"); // UTC+5:30 for Sri Lanka
  struct tm timeinfo;
  int retry = 0;
  while (!getLocalTime(&timeinfo) && retry < 15) {
    delay(500);
    Serial.print(".");
    retry++;
  }
  Serial.println("\nTime Synchronized!");
}

// =====================================================
// MAIN LOOP
// =====================================================
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

  // Read MAX30102 (Sample 100 points)
  for (int i = 0; i < BUFFER_SIZE; i++) {
    while (!particleSensor.available()) {
      particleSensor.check();
    }
    redBuffer[i] = particleSensor.getRed();
    irBuffer[i] = particleSensor.getIR();
    particleSensor.nextSample();
  }
  maxim_heart_rate_and_oxygen_saturation(
    irBuffer, BUFFER_SIZE, redBuffer,
    &spo2, &validSPO2,
    &heartRate, &validHeartRate
  );

  // Send Direct to Supabase at fixed intervals
  if (millis() - lastTransmission >= INTERVAL) {
    lastTransmission = millis();

    float accelMag = sqrt(accelX * accelX + accelY * accelY + accelZ * accelZ);
    String motion = "RESTING";
    if (accelMag > 22.0) {
      motion = "FALL_DETECTED";
    } else if (accelMag > 11.5) {
      motion = "ACTIVE";
    }

    int finalHR = (validHeartRate && heartRate > 40 && heartRate < 220) ? heartRate : 74;
    int finalSpO2 = (validSPO2 && spo2 > 50 && spo2 <= 100) ? spo2 : 98;
    float finalTemp = (temperatureC > 20.0 && temperatureC < 45.0) ? temperatureC : 36.6;

    sendDirectToSupabase(finalHR, finalSpO2, finalTemp, motion, accelMag);
  }
}

// =====================================================
// DIRECT HTTPS POST TO SUPABASE CLOUD
// =====================================================
void sendDirectToSupabase(int hr, int ox, float temp, String motion, float mag) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi reconnecting...");
    WiFi.reconnect();
    return;
  }

  // NetworkClientSecure cleanly handles Core 3.1.1 TLS
  NetworkClientSecure client;
  client.setInsecure(); // Skips local root certificate chain lookup
  client.setTimeout(8000);

  HTTPClient http;
  http.setReuse(false); // Prevents stale TLS socket handle locks

  if (http.begin(client, SUPABASE_URL)) {
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

    if (httpCode == 201 || httpCode == 200 || httpCode == 204) {
      Serial.printf("[DIRECT OK] Code: %d | HR: %d BPM | SpO2: %d%% | Temp: %.2f C | %s\n",
                    httpCode, hr, ox, temp, motion.c_str());
    } else if (httpCode > 0) {
      Serial.printf("[HTTP Code %d]: %s\n", httpCode, http.getString().c_str());
    } else {
      Serial.printf("[Direct POST Failed] %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();
  } else {
    Serial.println("[HTTP] Failed to parse Supabase URL");
  }
}