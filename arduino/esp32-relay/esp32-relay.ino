#include <WiFi.h>
#include <PubSubClient.h>

const char* ssid = "LATITUDE-E6420";
const char* password = "31121998";
const char* mqttServer = "192.168.137.1";
const int   mqttPort = 1883;
const char* subTopic = "hotel/room-1/#";
const char* pubTopic = "hotel/room-1/relay";

int human_count = 0;
int pir_main, pir_bath;

WiFiClient espClient;
PubSubClient client(espClient);

void callback(char* topic, byte* payload, unsigned int length) {
  String temp_mess = "";
  for (int i = 0; i < length; i++) {
    temp_mess += (char)payload[i];
    Serial.print((char)payload[i]);
  }
  Serial.println();
  Serial.println("-----------------------");

  if ((String)topic == "hotel/room-1/sensor/ir") {
    if (temp_mess == "in") {
      human_count += 1;
      Serial.print("Number of guests: ");
      Serial.println(human_count);
    } else if (temp_mess == "out") {
      if (human_count > 0) {
        human_count -= 1;
        Serial.print("Number of guests: ");
        Serial.println(human_count);
      }
    }
  } else if ((String)topic == "hotel/room-1/sensor/pir/mainroom") {
    pir_main = temp_mess.toInt();
  } else if ((String)topic == "hotel/room-1/sensor/pir/bathroom") {
    pir_bath = temp_mess.toInt();
  }
  if (human_count < pir_main + pir_bath) {
    human_count = pir_main + pir_bath;
  }
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

void relay_on() {
  Serial.println("relay on");
}

void relay_off() {
  Serial.println("relay off");
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
