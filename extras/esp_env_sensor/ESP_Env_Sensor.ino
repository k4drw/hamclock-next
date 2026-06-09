#include <WiFi.h> // Use <ESP8266WiFi.h> if using ESP8266
#include <WebServer.h> // Use <ESP8266WebServer.h> if using ESP8266
#include <Wire.h>

// Uncomment the sensor you are using:
#define USE_BME280
// #define USE_BME688

#ifdef USE_BME280
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
Adafruit_BME280 bme; 
#endif

#ifdef USE_BME688
#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>
#include <bsec.h> // Bosch BSEC library recommended for IAQ, but we'll do raw/simple for illustration
Adafruit_BME680 bme; 
#endif

// Replace with your network credentials
const char* ssid = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";

WebServer server(80);

void handleRoot() {
  server.send(200, "text/plain", "HamClock-Next Remote Env Sensor\nVisit /bme280 for JSON data.");
}

void handleSensorData() {
  String json = "{";
  
#ifdef USE_BME280
  float tempC = bme.readTemperature(); // Celsius
  float humidity = bme.readHumidity(); // %
  float pressure = bme.readPressure() / 100.0F; // hPa

  json += "\"tempC\":" + String(tempC, 2) + ",";
  json += "\"humidity\":" + String(humidity, 2) + ",";
  json += "\"pressHpa\":" + String(pressure, 2);
#endif

#ifdef USE_BME688
  if (!bme.performReading()) {
    server.send(500, "application/json", "{\"error\":\"Failed to perform BME688 reading\"}");
    return;
  }
  float tempC = bme.temperature;
  float humidity = bme.humidity;
  float pressure = bme.pressure / 100.0F;
  float gasKOhms = bme.gas_resistance / 1000.0F;

  json += "\"tempC\":" + String(tempC, 2) + ",";
  json += "\"humidity\":" + String(humidity, 2) + ",";
  json += "\"pressHpa\":" + String(pressure, 2) + ",";
  json += "\"gasKOhms\":" + String(gasKOhms, 2);
  // If you integrate BSEC, you can also output exact IAQ:
  // json += ",\"iaq\":" + String(iaqScore);
#endif

  json += "}";
  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);

  // Initialize Sensor
#ifdef USE_BME280
  if (!bme.begin(0x76)) { // Sometimes 0x77
    Serial.println("Could not find a valid BME280 sensor!");
    while (1);
  }
#endif

#ifdef USE_BME688
  if (!bme.begin(0x76)) { // Sometimes 0x77
    Serial.println("Could not find a valid BME688 sensor!");
    while (1);
  }
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150); // 320*C for 150 ms
#endif

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());

  // Setup Web Server Routes
  server.on("/", handleRoot);
  server.on("/bme280", handleSensorData); // Keep the route the same for backwards compatibility

  server.begin();
}

void loop() {
  server.handleClient();
}
