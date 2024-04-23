#include <Wire.h>
#include <WiFi.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <Adafruit_SSD1306.h>
#include <PubSubClient.h>

const char* WIFI_SSID = "CBB11"; //"ABDARSMZL";
const char* WIFI_PASSWORD = "syirasyad"; //"AAM021218";

// Raspberry Pi Mosquitto MQTT Broker
#define MQTT_HOST IPAddress (45, 149, 187, 197) //(192, 168, 1, 17) //(192, 168, 100, 108)
#define MQTT_PORT 1883

// MQTT Topics
#define MQTT_PUB "esp32/chili"

Adafruit_BME280 bme; // I2C
Adafruit_SSD1306 display = Adafruit_SSD1306(128, 32, &Wire);

WiFiClient wifiClient;
PubSubClient client(wifiClient);
int status = WL_IDLE_STATUS;
unsigned long lastSend;

int moisture,sensor_analog;
const int sensor_pin = 34; // Soil moisture sensor's PIN

void getAndSendSensorData()
{
  Serial.println("Collecting sensor data.");
  
  sensor_analog = analogRead(sensor_pin);
  moisture = ( 100 - ( (sensor_analog/4095.00) * 100 ) );
  
  // Read temperature as Celsius (the default)
  float temperature = bme.readTemperature();
  float humidity = bme.readHumidity();
  float pressure = bme.readPressure() / 100.0F;

  // Check if any reads failed and exit early (to try again).
  if (isnan(temperature) || isnan(humidity) || isnan(pressure)) {
    Serial.println("Failed to read from BME280 sensor!");
    return;
  }
  
  Serial.println("Sending data to MQTT");

  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("Temp: ");
  display.print(temperature);
  display.println(" C");
  display.print("Humi: ");
  display.print(humidity);
  display.println(" %");
  display.print("Pres: ");
  display.print(pressure);
  display.println(" hPa");
  display.print("Soil: ");
  display.print(moisture);
  display.println(" %");
  
  display.display();
  delay (100);

  // Prepare a JSON payload string
  String payload = "{";
  payload += "\"temperature\":";
  payload += String(temperature);
  payload += ",";
  payload += "\"humidity\":";
  payload += String(humidity); 
  payload += ",";
  payload += "\"pressure\":";
  payload += String(pressure);
  payload += ",";
  payload += "\"moisture\":";
  payload += String(moisture);   
  payload += "}";

  char attributes[1000];
  payload.toCharArray( attributes, 1000 );
  client.publish( MQTT_PUB, attributes);
  Serial.println( attributes );
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    status = WiFi.status();
    if ( status != WL_CONNECTED) {
      // Koneksi ke jaringan Wi-Fi menggunakan SSID dan Password
      Serial.print("Menghubungkan ke ");
      Serial.println(WIFI_SSID);
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      while (WiFi.status() != WL_CONNECTED) {
        delay(500);
      Serial.print(".");
      }
      // Menampilkan local IP address
      Serial.println("");
      Serial.println("Terhubung dengan WiFi.");
      Serial.println("IP address: ");
      Serial.println(WiFi.localIP());
      Serial.println("");
    }
    Serial.print("Menghubungkan ke MQTT ...");
    // Attempt to connect (clientId, username, password)
    if ( client.connect(MQTT_PUB) ) {
      Serial.println( "[DONE]" );
    } else {
      Serial.print( "[FAILED] [ rc = " );
      Serial.println( " : retrying in 5 seconds]" );
      delay( 500 );
    }
  }
}

void setup() {
  lastSend = 0;
  Serial.begin(9600);
  // default settings
  if (!bme.begin(0x76)) {
    Serial.println("Could not find a valid BME280 sensor");
    while (1) delay(10);
  }
  Serial.println("Found a sensor");

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x32
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ; // Don't proceed, loop forever
  }
  display.display();
  delay(1000); // Pause for a seconds
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setRotation(0);

  // Koneksi ke jaringan Wi-Fi menggunakan SSID dan Password
  Serial.print("Menghubungkan ke ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  // Menampilkan local IP address
  Serial.println("");
  Serial.println("Terhubung dengan WiFi.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("");

  client.setServer(MQTT_HOST, MQTT_PORT);
}

void loop() {
  if ( !client.connected() ) 
  {
    reconnect();
  }

  if ( millis() - lastSend > 30000 ) { // kirim data tiap 30 detik
    getAndSendSensorData();
    lastSend = millis();
  }
}
