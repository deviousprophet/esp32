import paho.mqtt.client as mqtt

broker_address = "192.168.137.1"

def on_message(client, userdata, message):
    _topic = str(message.topic)
    _value = int(str(message.payload.decode("utf-8")))

def on_disconnect (client, userdata, rc):
    print("Disconnected")

def on_connect (client, obj, flags, rc):
    print("Connected")
    client.subscribe("hotel/+/relay")

client = mqtt.Client()
client.on_message=on_message
client.on_connect=on_connect
client.on_disconnect=on_disconnect
client.connect(broker_address)
print("Connecting...")
client.loop_forever()