#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"
#include "secrets.h"

MAX30105 particleSensor;

const byte RATE_SIZE = 10; //Increase this for more averaging. 4 is good.
byte rates[RATE_SIZE]; //Array of heart rates
byte rateSpot = 0;
long lastBeat = 0; //Time at which the last beat occurred

float beatsPerMinute;
int beatAvg;

// WiFi Credentials
const char* ssid     = SECRET_WIFI_SSID;
const char* password = SECRET_WIFI_PASSWORD;
WiFiClient wifi_client;

// MQTT Credentials
const char *mqtt_broker   = SECRET_MQTT_HOST;
const int mqtt_port       = SECRET_MQTT_PORT;
const char *mqtt_username = SECRET_MQTT_USERNAME;
const char *mqtt_password = SECRET_MQTT_PASSWORD;
const char *topic         = SECRET_MQTT_TOPIC;
PubSubClient mqtt_client;
String mqtt_client_id;

unsigned long lastTime = 0;
unsigned long timerDelay = 5000;

// Define NTP Client
WiFiUDP ntpUDP;
NTPClient time_client(ntpUDP, "pool.ntp.org");

// Variable to hold current epoch timestamp
unsigned long epoch_time; 

unsigned long get_time() {
  time_client.update();
  unsigned long now = time_client.getEpochTime();
  return now;
}

void setup() {
  Serial.begin(115200);
  Serial.println("Initializing...");

  // Initialize wifi
  WiFi.begin(ssid, password);
  Serial.print("Connecting");
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());

  // Initialize mqtt client
  mqtt_client.setClient(wifi_client);
  mqtt_client.setServer(mqtt_broker, mqtt_port);
  while (!mqtt_client.connected()) {
      mqtt_client_id = "monitor-";
      mqtt_client_id += String(WiFi.macAddress());
      Serial.printf("Connecting client %s...\n", mqtt_client_id.c_str());
      if (mqtt_client.connect(mqtt_client_id.c_str(), mqtt_username, mqtt_password)) {
          Serial.println("Connected to mqtt broker");
      } else {
          Serial.print("Failed to connect with state ");
          Serial.println(mqtt_client.state());
          delay(2000);
      }
  }

  // Initialize time client
  time_client.begin();

  // Initialize sensor
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("MAX30102 was not found. Please check wiring/power. ");
    while (1);
  }

  particleSensor.setup(); //Configure sensor with default settings
  particleSensor.setPulseAmplitudeRed(0x0A); //Turn Red LED to low to indicate sensor is running
  particleSensor.setPulseAmplitudeGreen(0); //Turn off Green LED
}

void loop() {
  long irValue = particleSensor.getIR();

  if (checkForBeat(irValue) == true) {
    //We sensed a beat!
    long delta = millis() - lastBeat;
    lastBeat = millis();

    beatsPerMinute = 60 / (delta / 1000.0);

    if (beatsPerMinute < 255 && beatsPerMinute > 20) {
      rates[rateSpot++] = (byte)beatsPerMinute; //Store this reading in the array
      rateSpot %= RATE_SIZE; //Wrap variable

      //Take average of readings
      beatAvg = 0;
      for (byte x = 0 ; x < RATE_SIZE ; x++)
        beatAvg += rates[x];
      beatAvg /= RATE_SIZE;
    }
  }

  Serial.print("IR=");
  Serial.print(irValue);
  Serial.print(", BPM=");
  Serial.print(beatsPerMinute);
  Serial.print(", Avg BPM=");
  Serial.print(beatAvg);

  if (irValue < 50000)
    Serial.print(" No finger?");

  Serial.println();

  if ((millis() - lastTime) > timerDelay) {

    send_data(irValue, beatsPerMinute, beatAvg);

    lastTime = millis();
  }
}

boolean send_data(long ir, float bpm, int avg_bpm){
    StaticJsonDocument<256> doc;
    doc["sensor"] = mqtt_client_id.c_str();
    doc["timestamp"] = get_time();

    JsonObject data = doc.createNestedObject("data");
    data["ir"] = ir;
    data["bpm"] = bpm;
    data["avg_bpm"] = avg_bpm;
    char out[128];
    int b = serializeJson(doc, out);
    Serial.print("Sending ");
    Serial.print(b,DEC);
    Serial.println(" bytes");
    boolean rc = mqtt_client.publish(topic, out);
    return rc;
}
