#include <WiFi.h>
#include <PubSubClient.h>

#define PIR1  16
#define PIR2  17

const char* ssid = "LATITUDE-E6420";
const char* password = "31121998";
const char* mqttServer = "192.168.137.1";
const int   mqttPort = 1883;
const char* subTopic = "hotel/room-1/#";
const char* pubTopic = "hotel/room-1/sensor/pir/mainroom";

long pir_time1, pir_time2;
int pir_count = 0;
int pir1, pir2;

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
    pir_time1 = millis();
    pir_time2 = millis();
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
  
  WiFi.begin(ssid, password);
  WiFi.setAutoReconnect(true);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting to WiFi..");
  }
  Serial.println("Connected to the WiFi network");
  
  pir_time1 = millis();
  pir_time2 = millis();
}

void loop() {
  connect_mqtt();
  client.loop();

  pir1 = digitalRead(PIR1);
  pir2 = digitalRead(PIR2);
  
  if (pir1 && pir2) {
    pir_time2 = millis();
  } else if ((pir1 && !pir2) || (!pir1 && pir2)) {
    if (pir_count < 1) {
      pir_count = 1;
      client.publish(pubTopic, "1");
      Serial.println(pir_count);
    }
    pir_time1 = millis();
  }

  if ((pir_count < 2) && (pir_time2 - pir_time1 > 1500)) {
      pir_count = 2;
      client.publish(pubTopic, "2");
      Serial.println(pir_count);
      pir_time1 = millis();
      pir_time2 = millis();
  }
}
