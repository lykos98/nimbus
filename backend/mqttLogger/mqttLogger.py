#!/bin/python3
import os
import json

from datetime import datetime
from paho.mqtt.client import Client
from influxdb_client import InfluxDBClient, Point
from influxdb_client.client.write_api import SYNCHRONOUS

path = os.path.dirname(__file__) 

black_list = ["esp_lora_dummy"]

influx_org     = os.environ["INFLUX_ORG"]
influx_bucket  = os.environ["INFLUX_BUCKET"]
influx_url     = os.environ["INFLUX_BASEURL"] 
influx_token   = os.environ["INFLUX_TOKEN_LOGGER"]

broker_url     = os.environ["BROKER_URL"]
broker_user    = os.environ["BROKER_USER"]
broker_pswd    = os.environ["BROKER_PSWD"]
broker_port    = int(os.environ["BROKER_PORT"])

client = InfluxDBClient(url=influx_url, token=influx_token, org=influx_org)
write_api = client.write_api(write_options=SYNCHRONOUS)

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



def write_to_influx(data_json):
    point = Point("sensors")
    point.tag("stationId", data_json['stationId'])
    for k in data_json.keys(): 
        if k != "stationId":
            point.field(k, float(data_json[k]))

    point.time(datetime.now())
    print(point)
    if data_json["stationId"] not in black_list:
        write_api.write(bucket=influx_bucket, org=influx_org, record=point)
    else:
        write_logs("Station blacklisted")


def write_logs(msg, opt="l"):
    print(path)
    with open(os.path.join(path,"logs","msg.log"), "a") as f:
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
        write_logs(f"Recieved -> {d}", "l")
    except:
        write_logs("Cannot decode string")
    try:
        data = json.loads(d)
    except:
        write_logs(f"Error loading json got: {d}", "m")

    if not ("stationId" in data.keys()):
        return


    if "error" in data.keys():
        write_logs(f" !!! ERROR !!!: Recieved -> {data}", "l")
    else:
        try:
            write_to_influx(data)
        except:
            write_logs("Error writing to database", "l")

    return

if __name__ == "__main__":

    print("Got certificate from authority")
    print("Connecting to:")
    print(f"broker: {broker_url} port: {broker_port}")
    mqtt_client = Client()
    mqtt_client.username_pw_set(broker_user, broker_pswd)
    mqtt_client.connect(broker_url, broker_port, keepalive=20)
    mqtt_client.subscribe("esp32/sensors", qos=1)
    mqtt_client.on_message = on_message
    mqtt_client.loop_forever()
