import paho.mqtt.client as mqtt
import pymongo
import time

broker_address = "192.168.137.1"
subTopic = "hotel/+/relay"
hotel = [[0,0,0,0,0],[0,0,0,0,0],[0,0,0,0,0],[0,0,0,0,0]]
#4 floors, 5 rooms per floor
# floor 1: room 101 -> 105

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
    print("By floors")
    for i in hotel:
        print(i)

def on_disconnect (client, userdata, rc):
    print("Disconnected")

def on_connect (client, obj, flags, rc):
    print("Connected")
    client.subscribe(subTopic)
    polling_relay()

def polling_relay():
    for floor in range(4):
        for room in range(5):
            room_num = str((floor + 1)*100 + room + 1)
            client.publish("hotel/" + room_num + "/admin", "RELAY_STAT")

mongo_client = pymongo.MongoClient(
   "mongodb+srv://minhtaile2712:imissyou@cluster0-wyhfl.mongodb.net/test?retryWrites=true&w=majority")
db = mongo_client.dev

# post = db.hotel
# post_data = {
#     "hotel_relay": hotel
# }
# result = post.insert_one(post_data)
# print('Posted: {0}'.format(result.inserted_id))

client = mqtt.Client()
client.on_message=on_message
client.on_connect=on_connect
client.on_disconnect=on_disconnect
client.connect(broker_address)
print("Connecting...")
client.loop_forever()