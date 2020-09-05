import paho.mqtt.client as mqtt
import pymongo
import time
import threading

broker_address = "192.168.137.1"
subTopic = "hotel/+/relay"
localRooms = []
temp_localRooms = []

def on_message(client, userdata, message):
    global localRooms
    _topic = str(message.topic)
    _data = bool(int(str(message.payload.decode("utf-8"))))

    start = "hotel/"
    end = "/relay"
    room = int(_topic[len(start):-len(end)])
    print(room, _data)
    
    for localRoom in localRooms:
        if localRoom['name'] == room:
            localRoom['state'] = _data

    writebacktodb()

def on_disconnect (client, userdata, rc):
    print("MQTT Disconnected")

def on_connect (client, obj, flags, rc):
    print("MQTT Connected")
    client.subscribe(subTopic)
    polling_relay()

def mqtt_client():
    client.on_message=on_message
    client.on_connect=on_connect
    client.on_disconnect=on_disconnect
    client.connect(broker_address)
    print("Connecting MQTT...")
    client.loop_forever()

def polling_relay():
    for floor in range(4):
        for room in range(5):
            room_num = str((floor + 1)*100 + room + 1)
            client.publish("hotel/" + room_num + "/admin", "RELAY_STAT")

def initfromdb():
    for room in dbRooms.find({}, projection={'_id': False, 'name': True, 'state': True}):
        localRooms.append(room)
        temp_localRooms.append(room)
    for room in localRooms:
        print(room)

def retfromdb():
    for dbRoom in dbRooms.find({}, projection={'_id': False, 'name': True, 'state': True}):
        for localRoom in temp_localRooms:
            if localRoom['name'] == dbRoom['name']:
                localRoom['state'] = dbRoom['state']

def writebacktodb():
    for room in localRooms:
        dbRooms.find_one_and_update({'name': room['name']}, {'$set': {'state': room['state']}})

def db_read():
    while(1):
        retfromdb()
        # for i in temp_localRooms:
        #     print(i)
        time.sleep(1)

def db_change():
    global localRooms
    global temp_localRooms
    while(1):
        print("====================")
        for i in range(8):
            if (localRooms == temp_localRooms):
                print(temp_localRooms)
        time.sleep(1)

mongo_client = pymongo.MongoClient(
   "mongodb+srv://minhtaile2712:imissyou@cluster0-wyhfl.mongodb.net/test?retryWrites=true&w=majority")
db = mongo_client.test
dbRooms = db.rooms
initfromdb()

client = mqtt.Client()
mqtt_thread = threading.Thread(target=mqtt_client)
mqtt_thread.start()
db_thread = threading.Thread(target=db_read)
db_thread.start()
array_thead = threading.Thread(target=db_change)
array_thead.start()