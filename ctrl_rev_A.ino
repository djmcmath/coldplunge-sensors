#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <ESPping.h>
//Write to the SPIFFS with:
// -- .\mkspiffs -c .\\data -b 4096 -p 256 -s 0x160000 spiffs.bin
// -- .\esptool --chip esp32 --port COM5 --baud 921600 write_flash -z 0x290000 spiffs.bin
#include <Adafruit_MAX31855.h>
#include "time.h"

SET_LOOP_TASK_STACK_SIZE(16 * 1024);  // 16KB

// WiFi credentials -- Set to your own values.
const char* ssid = "YOURSSID";
const char* password = "YOURPASS";

// For setting the time
const char* ntpServer = "pool.ntp.org";  // Use a public NTP server

// MAX31855 Thermocouple Pins
#define MAX31855_CLK 22 //Clock
#define MAX31855_CS 19  //Cable Select
#define MAX31855_DO 4   //Data
Adafruit_MAX31855 thermocouple(MAX31855_CLK, MAX31855_CS, MAX31855_DO);

// Flow Meter Pin
#define FLOW_SENSOR_PIN 21  //Flow sensor gets 5VDC, Ground, and a single signal pin
volatile int pulseCount = 0;
float flowRate = 0.0;

DynamicJsonDocument jsonDoc(8192);
String response;

// Circular buffer for storing data
struct DataPoint {
  unsigned long timestamp;
  float temperature;
  float flowRate;
};
const int BUFFER_SIZE = (24 * 60 * 60) / 15;  // 72 hours of data at 15s intervals
DataPoint dataBuffer[BUFFER_SIZE];
int bufferIndex = 0;

// Web server setup
AsyncWebServer server(80);

// Interrupt handler for flow sensor
void IRAM_ATTR pulseCounter() {
  pulseCount++;
}

// Function to compute flow rate
// Calibrate flow by removing any obstructions in the system (any filters or nozzles) then assume that you're probably getting close to nominal pump flow rate
// Then just adjust this number to make the indicated flow rate match expected
// No, it's not perfect, but it's close enough to give you a good indication if you're not getting enough flow.
void calculateFlowRate() {
  flowRate = (pulseCount / 15.0);
  pulseCount = 0;
}

void listSPIFFSFiles() {
    Serial.println("Listing SPIFFS files...");
    File root = SPIFFS.open("/");
    
    if (!root) {
        Serial.println("Failed to open root directory");
        return;
    }

    File file = root.openNextFile();
    while (file) {
        Serial.print("FILE: ");
        Serial.println(file.name());
        file = root.openNextFile();
    }
}

void CheckWiFi() {
  const IPAddress remote_ip(192,168,1,1);
  bool ping_success = false;
  if (Ping.ping(remote_ip, 4)) {
    Serial.println("Ping success");
    ping_success = true;
  } else {
    Serial.println("Ping fail");
    ping_success = false;
  }

  if ( (WiFi.status() != WL_CONNECTED) || (ping_success == false) ) {
    Serial.println("Wifi dropped; attempting to reconnect");
    StartWiFi();
  }
}

void StartWiFi() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
}

void restartServer() {
    Serial.println("Restarting server...");
    server.end();
    delay(1000);
    
    // Re-register API endpoints
    setupServerRoutes();

    server.begin();
    Serial.println("Server restarted.");
}

// Function to set up all server routes
void setupServerRoutes() {
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/index.html", "text/html");
    });

    server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (ESP.getFreeHeap() < 50000) {
            Serial.println("Low memory detected! Restarting ESP...");
            ESP.restart();
            return;
        }

        jsonDoc.clear();
        JsonArray data = jsonDoc.createNestedArray("readings");

        int maxEntries = 100; // Only return the last 100 readings
        int count = min(maxEntries, bufferIndex);

        for (int i = 0; i < count; i++) {
            int index = (bufferIndex - count + i + BUFFER_SIZE) % BUFFER_SIZE;

            if (dataBuffer[index].timestamp > 0) {
                JsonObject point = data.createNestedObject();
                point["timestamp"] = dataBuffer[index].timestamp;
                point["temperature"] = dataBuffer[index].temperature;
                point["flowRate"] = dataBuffer[index].flowRate;
            }
        }
        
        if (serializeJson(jsonDoc, response) == 0) {
            Serial.println("JSON serialization failed!");
            request->send(500, "text/plain", "JSON serialization failed");
            return;
        }

        request->send(200, "application/json", response);
        jsonDoc.clear();
        Serial.printf("Total Heap: %d | Free Heap: %d | Max Allocatable: %d\n", ESP.getHeapSize(), ESP.getFreeHeap(), ESP.getMaxAllocHeap());
    });

    server.begin();
    Serial.println("Endpoints registered.");
}

void setup() {
  Serial.begin(115200);

  // Initialize WiFi
  StartWiFi();

  // Initialize SPIFFS for local storage
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS Mount Failed");
    return;
  } else { Serial.println("SPIFFS Mount Successful"); }

  if (SPIFFS.exists("/index.html")) {
      Serial.println("index.html found in SPIFFS!");
  } else {
      Serial.println("ERROR: index.html NOT FOUND in SPIFFS!");
      listSPIFFSFiles();
  }

  // Initialize MAX31855
  if (!thermocouple.begin()) {
    Serial.println("Failed to initialize thermocouple!");
    return;
  }

  // Initialize flow sensor
  pinMode(FLOW_SENSOR_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN), pulseCounter, FALLING);

  setupServerRoutes();

  server.begin();

  // Sync time using NTP
  configTime(0, 0, ntpServer);
  Serial.println("Time synchronized.");
}

int WiFiCheckCount = 0;
int WiFiLoopCheck = 10; //Check every ~2.5min, or 10 loops

void loop() {

  // Read temperature
  float temperature = thermocouple.readCelsius();
  if (isnan(temperature)) {
    Serial.println("Error reading temperature");
    temperature = -1;
  }
  // Calculate flow rate
  calculateFlowRate();

  // Store data in circular buffer
  time_t now;
  time(&now);
  dataBuffer[bufferIndex] = { now, temperature, flowRate };
  bufferIndex = (bufferIndex + 1) % BUFFER_SIZE;

  //Serial.printf("Time: %lu, Temp: %.2fC, Flow: %.2f L/min\n", millis(), temperature, flowRate);
  Serial.print(".");

  WiFiCheckCount++;
  if (WiFiCheckCount >= WiFiLoopCheck) {
    WiFiCheckCount = 0;
    CheckWiFi();
    if (ESP.getFreeHeap() < 100500) {  // Restart if free heap drops below 100K
      restartServer();
    }
  }

  delay(15000);  // Wait 15 seconds
}
