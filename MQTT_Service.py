import paho.mqtt.client as mqtt

broker_address = "192.168.137.1"
subTopic = "hotel/+/relay"
hotel = [[0,0,0,0,0],[0,0,0,0,0],[0,0,0,0,0],[0,0,0,0,0]]

def on_message(client, userdata, message):
    global hotel
    _topic = str(message.topic)
    _data = int(str(message.payload.decode("utf-8")))

    start = "hotel/"
    end = "/relay"
    room = int(_topic[len(start):-len(end)])
    
    floor = room%100
    room_floor = int(room/100)

    hotel[room_floor-1][floor-1] = _data
    print(hotel)

def on_disconnect (client, userdata, rc):
    print("Disconnected")

def on_connect (client, obj, flags, rc):
    print("Connected")
    client.subscribe(subTopic)

client = mqtt.Client()
client.on_message=on_message
client.on_connect=on_connect
client.on_disconnect=on_disconnect
client.connect(broker_address)
print("Connecting...")
client.loop_forever()