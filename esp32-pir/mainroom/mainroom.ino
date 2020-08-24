#include <WiFi.h>
#include <PubSubClient.h>

#define PIR1  16
#define PIR2  17

const char* ssid = "LATITUDE-E6420";
const char* password = "31121998";
const char* mqttServer = "192.168.137.1";
const int   mqttPort = 1883;
const char* subTopic = "hotel/room-1/admin";
const char* pubTopic = "hotel/room-1/sensor/pir/mainroom";

long pir1_time, pir2_time;
int pir_count = 0;
boolean pir_en = false;
long interval = 500;

WiFiClient espClient;
PubSubClient client(espClient);

void callback(char* topic, byte* payload, unsigned int length) {
  String temp_mess = "";
  Serial.print("Message arrived in topic: ");
  Serial.println(topic);
  
  Serial.print("Message:");
  for (int i = 0; i < length; i++) {
    temp_mess += (char)payload[i];
    Serial.print((char)payload[i]);
  }

  if (temp_mess == "reset") {
    pir_count = 0;
  }
  
  Serial.println();
  Serial.println("-----------------------");
}

void connect_mqtt() {
  if (WiFi.status() == WL_CONNECTED) {
    client.setServer(mqttServer, mqttPort);
    client.setCallback(callback);
    
    while (!client.connected()) {
      Serial.println("Connecting to MQTT...");
      
      if (client.connect("ESP8266Client")) {
        Serial.println("MQTT connected");
        client.subscribe(subTopic);
      } else {
        Serial.print("failed with state ");
        Serial.print(client.state());
        delay(2000);
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(PIR1, INPUT);
  pinMode(PIR2, INPUT);
  
  pir1_time = millis();
  pir2_time = millis();
  
  WiFi.begin(ssid, password);
  WiFi.setAutoReconnect(true);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting to WiFi..");
  }
  Serial.println("Connected to the WiFi network");
}

void loop() {
  connect_mqtt();
  client.loop();

  if (digitalRead(PIR1)) {
    if (pir_en) {
      pir1_time = millis();
    } else {
      pir1_time = millis() - 1000;
      pir_en = true;
    }
  }

  if (digitalRead(PIR2)) {
    if (pir_en) {
      pir2_time = millis();
    } else {
      pir2_time = millis() - 1000;
      pir_en = true;
    }
  }
  
  if (pir_en) {
    if (((pir1_time - pir2_time > interval) || (pir1_time - pir2_time < -interval)) && (pir_count < 1)) {
      pir_count = 1;
      Serial.println(pir_count);
    } else if (((pir1_time - pir2_time < interval) && (pir1_time - pir2_time > -interval)) && (pir_count < 2)) {
      pir_count = 2;
      Serial.println(pir_count);
    }
    pir_en = false;
  }
}
