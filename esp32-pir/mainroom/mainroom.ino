#include <WiFi.h>
#include <PubSubClient.h>

#define PIR1  16
#define PIR2  17

const char* ssid = "LATITUDE-E6420";
const char* password = "31121998";
const char* mqttServer = "192.168.137.1";
const int   mqttPort = 1883;
//const char* subTopic = "hotel/room-1/admin";
const char* pubTopic = "hotel/room-1/esp32/sensor/pir/mainroom";

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
        //client.subscribe(subTopic);
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
}
