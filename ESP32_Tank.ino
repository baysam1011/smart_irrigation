#include <Wire.h>
#include <WiFi.h>
#include <PubSubClient.h>

const char* WIFI_SSID = "CBB11"; //"ABDARSMZL";
const char* WIFI_PASSWORD = "syirasyad"; //"AAM021218";

// Raspberry Pi Mosquitto MQTT Broker
#define MQTT_HOST IPAddress (45, 149, 187, 197) //(192, 168, 1, 17) //(192, 168, 100, 108)
#define MQTT_PORT 1883

// MQTT Topics
#define MQTT_PUB "esp32/tank"

#define POWER_PIN       33 // Water sensor's VCC pin
#define SIGNAL_PIN      32 // Water sensor's signal pin
#define TDS_SENSOR_PIN  34 // TDS sensor's data pin
#define SSR_PIN         25 // SSR pin
#define trigPin         12
#define echoPin         14

#define VREF            3.3// analog reference voltage(Volt) of the ADC
#define SCOUNT          30 // sum of sample point
#define SENSOR_MIN      0
#define SENSOR_MAX      521
//define sound speed in cm/uS
#define SOUND_SPEED     0.034

WiFiClient wifiClient;
PubSubClient client(wifiClient);
int status = WL_IDLE_STATUS;
unsigned long lastSend;

int value = 0; // variable to store the water sensor value
int level = 0; // variable to store the water level
bool pump = 0; // variable to store the status of SSR (pump)

int analogBuffer[SCOUNT];     // store the analog value in the array, read from ADC
int analogBufferTemp[SCOUNT];
int analogBufferIndex = 0;
int copyIndex = 0;

float averageVoltage = 0;
float tdsValue = 0;       // variable to store the TDS sensor value
float temperature = 25;   // current temperature for compensation

long duration;
float distanceCm;

// median filtering algorithm
int getMedianNum(int bArray[], int iFilterLen) {
  int bTab[iFilterLen];
  for (byte i = 0; i<iFilterLen; i++)
  bTab[i] = bArray[i];
  int i, j, bTemp;
  for (j = 0; j < iFilterLen - 1; j++) {
    for (i = 0; i < iFilterLen - j - 1; i++) {
      if (bTab[i] > bTab[i + 1]) {
        bTemp = bTab[i];
        bTab[i] = bTab[i + 1];
        bTab[i + 1] = bTemp;
      }
    }
  }
  if ((iFilterLen & 1) > 0) {
    bTemp = bTab[(iFilterLen - 1) / 2];
  }
  else {
    bTemp = (bTab[iFilterLen / 2] + bTab[iFilterLen / 2 - 1]) / 2;
  }
  return bTemp;
}

void getSensorTDS() {
  static unsigned long analogSampleTimepoint = millis();
  if(millis()-analogSampleTimepoint > 40U) {                    //every 40 milliseconds,read the analog value from the ADC
    analogSampleTimepoint = millis();
    analogBuffer[analogBufferIndex] = analogRead(TDS_SENSOR_PIN); //read the analog value and store into the buffer
    analogBufferIndex++;
    if(analogBufferIndex == SCOUNT) { 
      analogBufferIndex = 0;
    }
  }   
  
  static unsigned long printTimepoint = millis();
  if(millis()-printTimepoint > 800U) {
    printTimepoint = millis();
    for(copyIndex=0; copyIndex<SCOUNT; copyIndex++) {
      analogBufferTemp[copyIndex] = analogBuffer[copyIndex];
      // read the analog value more stable by the median filtering algorithm, and convert to voltage value
      averageVoltage = getMedianNum(analogBufferTemp,SCOUNT) * (float)VREF / 4096.0;   
      // temperature compensation formula: fFinalResult(25^C) = fFinalResult(current)/(1.0+0.02*(fTP-25.0)); 
      float compensationCoefficient = 1.0+0.02*(temperature-25.0);
      // temperature compensation
      float compensationVoltage=averageVoltage/compensationCoefficient;
      // convert voltage value to tds value
      tdsValue=(133.42*compensationVoltage*compensationVoltage*compensationVoltage - 255.86*compensationVoltage*compensationVoltage + 857.39*compensationVoltage)*0.5;
    }
  }
}

void getSensorWATER() {
  digitalWrite(POWER_PIN, HIGH);  // turn the sensor ON
  delay(10);                      // wait 10 milliseconds
  value = analogRead(SIGNAL_PIN); // read the analog value from sensor
  digitalWrite(POWER_PIN, LOW);   // turn the sensor OFF

  Serial.print("TDS Value:");
  Serial.print(tdsValue,0);
  Serial.println(" ppm");

  // water level value
  level = map(value, SENSOR_MIN, SENSOR_MAX, 0, 4); // 4 levels
  Serial.print("Water level: ");
  Serial.println(level);
}

void getSensorDISTANCE() {
  // Clears the trigPin
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  // Sets the trigPin on HIGH state for 10 micro seconds
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  
  // Reads the echoPin, returns the sound wave travel time in microseconds
  duration = pulseIn(echoPin, HIGH);
  
  // Calculate the distance
  distanceCm = duration * SOUND_SPEED/2;
  
  // Prints the distance in the Serial Monitor
  Serial.print("Distance (cm): ");
  Serial.println(distanceCm);
}

void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;
  
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();
  
  // If a message is received on the topic esp32/output, you check if the message is either "on" or "off". 
  // Changes the output state according to the message
  if (String(topic) == "esp32/pump") {
    Serial.print("Changing pump to ");
    if(messageTemp == "on"){
      Serial.println("on");
      digitalWrite(SSR_PIN, HIGH);
      pump = 1;
    }
    else if(messageTemp == "off"){
      Serial.println("off");
      digitalWrite(SSR_PIN, LOW);
      pump = 0;
    }
  }
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
      // Subscribe
      client.subscribe("esp32/pump");
    } else {
      Serial.print( "[FAILED] [ rc = " );
      Serial.println( " : retrying in 5 seconds]" );
      delay( 500 );
    }
  }
}

void setup() {
  Serial.begin(9600);
  pinMode(POWER_PIN, OUTPUT);   // configure pin as an OUTPUT
  digitalWrite(POWER_PIN, LOW); // turn the sensor OFF
  pinMode(SIGNAL_PIN,INPUT);    // configure pin as an INPUT
  pinMode(TDS_SENSOR_PIN,INPUT);// configure pin as an INPUT
  pinMode(SSR_PIN, OUTPUT);     // configure pin as an OUTPUT
  pinMode(trigPin, OUTPUT);     // Sets the trigPin as an OUTPUT
  pinMode(echoPin, INPUT);      // Sets the echoPin as an INPUT

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
  client.setCallback(callback);
}

void loop() {
  if ( !client.connected() ) 
  {
    reconnect();
  }
  client.loop();

  if ( millis() - lastSend > 30000 ) { // kirim data tiap 30 detik
    getSensorTDS();
    // getSensorWATER();
    getSensorDISTANCE();

    // Prepare a JSON payload string
    String payload = "{";
    payload += "\"tds\":";
    payload += String(tdsValue);
    payload += ",";
    payload += "\"level\":";
    // payload += String(level);
    payload += String(distanceCm); 
    payload += ",";
    payload += "\"pump\":";
    payload += String(pump);
    payload += "}";

    char attributes[1000];
    payload.toCharArray( attributes, 1000 );
    client.publish( MQTT_PUB, attributes);
    Serial.println( attributes );
    
    lastSend = millis();
  }

  if (pump == 1) {
    digitalWrite(SSR_PIN, HIGH); // turn the pump ON
  }
  else digitalWrite(SSR_PIN, LOW);
  
}