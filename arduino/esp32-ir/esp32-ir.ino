//  IR1 --> IR2 : in
//  IR2 --> IR1 : out

#include <WiFi.h>
#include <PubSubClient.h>

#define IR1   12
#define IR2   14

const char* ssid = "ssid";
const char* password = "password";
const char* mqttServer = "server IP";
const int   mqttPort = 1883;
//const char* subTopic = "hotel/room-1/admin";
const char* pubTopic = "hotel/room-1/sensor/ir";

int ir1, ir2, in_out_stat;

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
  pinMode(IR1, INPUT);
  pinMode(IR2, INPUT);
  
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
  
  ir1 = digitalRead(IR1);
  ir2 = digitalRead(IR2);
  
  if (ir1 && ir2) {
    if (in_out_stat == 3) {
      Serial.println("in");
      client.publish(pubTopic, "in");
    } else if (in_out_stat == -3) {
      Serial.println("out");
      client.publish(pubTopic, "out");
    }
    in_out_stat = 0;
  } else if (!ir1 && ir2) {
    if (in_out_stat == -2) {
      in_out_stat = -3;
    } else if ((in_out_stat == 0) || (in_out_stat == 2)) {
      in_out_stat = 1;
    }
  } else if (!ir1 && !ir2) {
    if ((in_out_stat == 1) || (in_out_stat == 3)) {
      in_out_stat = 2;
    } else if ((in_out_stat == -1) || (in_out_stat == -3)) {
      in_out_stat = -2;
    }
  } else {
    if (in_out_stat == 2) {
      in_out_stat = 3;
    } else if ((in_out_stat == 0) || (in_out_stat == -2)) {
      in_out_stat = -1;
    }
  }
}
