/*
  Csense Care - ESP32 IoT Patient Monitoring Prototype
  
  Sensors:
  1. MAX30102 - Heart Rate + SpO2 (I2C: SDA=22, SCL=21)
  2. MPU6050  - Accelerometer + Gyroscope (I2C: SDA=22, SCL=21)
  3. DS18B20  - Body/Surface Temperature (OneWire: GPIO 4)
  
  Direct Uplink: ESP32 -> Supabase Cloud -> Vercel App
*/

#include <WiFi.h>
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
// NETWORK & SUPABASE CONFIGURATION
// =====================================================
const char* WIFI_SSID     = "CJiPhone";
const char* WIFI_PASSWORD = "chami1234";

const char* SUPABASE_HOST = "asdxufazwwahikoeubnc.supabase.co";
const char* SUPABASE_PATH = "/rest/v1/patient_vitals";
const char* SUPABASE_KEY  = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6ImFzZHh1ZmF6d3dhaGlrb2V1Ym5jIiwicm9sZSI6ImFub24iLCJpYXQiOjE3OTI0MDQ4MDAsImV4cCI6MjEwNzk4MDgwMH0.Gi-DCIBfXDAZ8huBPSCjVuPc8Hl5-BJNWevPgjEq9Es";

// =====================================================
// HARDWARE PIN DEFINITIONS
// =====================================================
#define I2C_SDA 22
#define I2C_SCL 21
#define DS18B20_PIN 4

// =====================================================
// SENSOR OBJECTS & BUFFERS
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

float temperatureC = 0.0;

unsigned long lastTransmission = 0;
const unsigned long INTERVAL = 1500; // Send reading every 1.5s

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

  // 1. Initialize I2C Bus
  Wire.begin(I2C_SDA, I2C_SCL);

  // 2. Initialize DS18B20
  temperatureSensor.begin();
  if (temperatureSensor.getDeviceCount() > 0) {
    Serial.println("DS18B20: FOUND");
  } else {
    Serial.println("DS18B20: NOT FOUND (Check pullup resistor & GPIO 4)");
  }

  // 3. Initialize MPU6050
  if (mpu.begin(0x68, &Wire)) {
    Serial.println("MPU6050: OK");
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  } else {
    Serial.println("MPU6050: Not detected");
  }

  // 4. Initialize MAX30102
  if (particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("MAX30102: OK");
    particleSensor.setup();
    particleSensor.setPulseAmplitudeRed(0x2F);
    particleSensor.setPulseAmplitudeIR(0x2F);
    particleSensor.setPulseAmplitudeGreen(0);
  } else {
    Serial.println("MAX30102: Not detected");
  }

  // 5. Connect to Wi-Fi
  Serial.println("\nResetting Wi-Fi adapter...");
  WiFi.persistent(false);
  WiFi.disconnect(true);
  delay(500);

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
    Serial.println("\nWi-Fi Connected!");
    Serial.print("ESP32 IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.printf("\nConnection Failed! Status code: %d\n", WiFi.status());
    return;
  }

  // 6. Synchronize Network Time (Required for Cloudflare TLS handshake)
  Serial.print("Syncing network time...");
  configTime(19800, 0, "pool.ntp.org", "time.google.com"); // UTC+5:30
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
  // 1. Read DS18B20 Temperature
  temperatureSensor.requestTemperatures();
  temperatureC = temperatureSensor.getTempCByIndex(0);

  // Peak tracking variable for motion during the sampling window
  float peakAccelMag = 9.81;

  // 2. Read MAX30102 while continuously sampling MPU6050
  for (int i = 0; i < BUFFER_SIZE; i++) {
    while (!particleSensor.available()) {
      particleSensor.check();

      // Sample MPU6050 while waiting for next PPG reading
      sensors_event_t a, g, temp;
      mpu.getEvent(&a, &g, &temp);
      float currentMag = sqrt(a.acceleration.x * a.acceleration.x +
                              a.acceleration.y * a.acceleration.y +
                              a.acceleration.z * a.acceleration.z);
      if (currentMag > peakAccelMag) {
        peakAccelMag = currentMag;
      }
    }

    redBuffer[i] = particleSensor.getRed();
    irBuffer[i] = particleSensor.getIR();
    particleSensor.nextSample();

    // Check motion again after each sample step
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);
    float currentMag = sqrt(a.acceleration.x * a.acceleration.x +
                            a.acceleration.y * a.acceleration.y +
                            a.acceleration.z * a.acceleration.z);
    if (currentMag > peakAccelMag) {
      peakAccelMag = currentMag;
    }
  }

  // 3. Calculate SpO2 and Heart Rate
  maxim_heart_rate_and_oxygen_saturation(
    irBuffer, BUFFER_SIZE, redBuffer,
    &spo2, &validSPO2,
    &heartRate, &validHeartRate
  );

  // 4. Transmission & Motion Evaluation
  if (millis() - lastTransmission >= INTERVAL) {
    lastTransmission = millis();

    // Motion evaluation based on deviation from resting gravity (9.8 m/s^2)
    String motion = "RESTING";
    if (peakAccelMag > 20.0) {
      motion = "FALL_DETECTED";
    } else if (peakAccelMag > 11.0 || peakAccelMag < 8.5) {
      motion = "ACTIVE";
    }

    // Normal baseline fallbacks if finger is lifted off sensor
    int finalHR = (validHeartRate && heartRate > 40 && heartRate < 220) ? heartRate : 74;
    int finalSpO2 = (validSPO2 && spo2 > 50 && spo2 <= 100) ? spo2 : 98;
    float finalTemp = (temperatureC > 20.0 && temperatureC < 45.0) ? temperatureC : 36.6;

    sendDirectToSupabase(finalHR, finalSpO2, finalTemp, motion, peakAccelMag);
  }
}

// =====================================================
// DIRECT HTTPS POST TO SUPABASE CLOUD (RAW TLS STREAM)
// =====================================================
void sendDirectToSupabase(int hr, int ox, float temp, String motion, float mag) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi reconnecting...");
    WiFi.reconnect();
    return;
  }

  NetworkClientSecure client;
  client.setInsecure(); // Skip certificate verification
  client.setTimeout(10);

  if (!client.connect(SUPABASE_HOST, 443)) {
    Serial.println("[Direct POST Failed] TLS handshake to Supabase failed");
    client.stop();
    return;
  }

  // Construct JSON body
  String json = "{";
  json += "\"patient_id\":\"P-101\",";
  json += "\"heart_rate\":" + String(hr) + ",";
  json += "\"spo2\":" + String(ox) + ",";
  json += "\"temp_c\":" + String(temp, 2) + ",";
  json += "\"motion\":\"" + motion + "\",";
  json += "\"accel_mag\":" + String(mag, 2);
  json += "}";

  // Send raw HTTP request headers
  client.print(String("POST ") + SUPABASE_PATH + " HTTP/1.1\r\n");
  client.print(String("Host: ") + SUPABASE_HOST + "\r\n");
  client.print("User-Agent: ESP32-Csense\r\n");
  client.print("Content-Type: application/json\r\n");
  client.print(String("apikey: ") + SUPABASE_KEY + "\r\n");
  client.print(String("Authorization: Bearer ") + SUPABASE_KEY + "\r\n");
  client.print("Prefer: return=minimal\r\n");
  client.print(String("Content-Length: ") + json.length() + "\r\n");
  client.print("Connection: close\r\n\r\n");
  client.print(json);

  // Check HTTP response
  unsigned long start = millis();
  while (client.connected() && millis() - start < 5000) {
    if (client.available()) {
      String statusLine = client.readStringUntil('\n');
      if (statusLine.indexOf("201") > 0 || statusLine.indexOf("200") > 0 || statusLine.indexOf("204") > 0) {
        Serial.printf("[DIRECT OK] HR: %d BPM | SpO2: %d%% | Temp: %.2f C | %s (%.2f m/s^2)\n",
                      hr, ox, temp, motion.c_str(), mag);
        break;
      }
    }
  }

  client.stop();
}