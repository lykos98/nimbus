#!/bin/python3
import csv
import os
from paho.mqtt.client import Client
import json
import requests
from datetime import datetime
from dotenv import dotenv_values
d = dict()



path = os.path.dirname(__file__) 

secrets = dotenv_values(os.path.join(path,"../../.env"))

black_list = ["esp_lora_dummy"]

influx_org = secrets["influx_org"]
influx_bucket = secrets["influx_bucket"]
influx_baseUrl = secrets["influx_baseUrl"]
influx_token = secrets["influx_token"]

influx_url = f"{influx_baseUrl}/api/v2/write?org={influx_org}&bucket={influx_bucket}&precision=s" 

influx_headers = { "Authorization": f"Token {influx_token}",
            "Content-Type": "text/plain; charset=utf-8",
            "Accept": "application/json"}

validFields = [
            "airTemperature", "airHumidity", "airPressure", "airCO2ppm", "airNOXppm",
            "windDirection", "windSpeed",
            "rainmm",
            "terrainTemperature", "terrainHumidity", "terrainPH", 
            "waterTemperature", "waterHumidity", "waterPH",
            "batteryV", "batteryA", "solarV", "solarA"
            ]



def writeToInflux(dataJSON):
    databinary = f"sensors,stationId={dataJSON['stationId']} "
    for k in dataJSON.keys(): 
        if k != "stationId":
            databinary += f"{k}={dataJSON[k]},"
    databinary = databinary[:-1]
    databinary += f" {datetime.now().timestamp():.0f}"
    print(databinary)
    if dataJSON["stationId"] not in black_list:
        try:
            r = requests.post(influx_url, data=databinary, headers=influx_headers)
            print(r.status_code)
        except:
            print("Cannot write to influx")
    else:
        writeLogs("Station blacklisted")


def writeLogs(msg, opt="l"):
    with open(os.path.join(path,"msg.log"), "a") as f:
        if opt == "l":
            f.write(f"LOG {datetime.now()}: {msg}\n")
            print(f"LOG {datetime.now()}: {msg}\n")
        elif opt == "m":
            f.write(f"MSG {datetime.now()}: {msg}\n")
            print(f"MSG {datetime.now()}: {msg}\n")
    return

def on_message(client, userdata, msg):
    # standar interface to library
    try:
        d = msg.payload.decode()
    except:
        writeLogs("Cannot decode string")
    try:
        data = json.loads(d)
    except:
        try:
            writeLogs(f"Error loading json got: {d}", "m")
        except:
            writeLogs("Cannot decode string")
        return

    if not ("stationId" in data.keys()):
        return

    writeLogs(f"Recieved -> {data}", "l")
    stationId = data["stationId"]

   # writeToInflux(data)
    try:
        writeToInflux(data)
    except:
        writeLogs("Error writing to database", "m")
        return

    now = datetime.now()


mqtt_client = Client("python_logger", clean_session=False)
#mqtt_client.connect("127.0.0.1", keepalive=20)
mqtt_client.username_pw_set(secrets["loggerUser"], secrets["loggerPassword"])
mqtt_client.connect(secrets["broker_ip"], keepalive=20)
mqtt_client.subscribe("esp32/sensors", qos=1)
mqtt_client.on_message = on_message
mqtt_client.loop_forever()
