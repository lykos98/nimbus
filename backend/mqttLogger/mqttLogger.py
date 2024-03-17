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

GRAFANA_URL = secrets["GRAFANA_URL"]
GRAFANA_APIKEY = secrets["GRAFANA_APIKEY"]
GRAFANA_USER = secrets["GRAFANA_USER"] 

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
            "rainQuanity",
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
    try:
        r = requests.post(influx_url, data=databinary, headers=influx_headers)
        print(r.status_code)
    except:
        print("Cannot write to influx")


def writeLogs(msg, opt="l"):
    with open(os.path.join(path,"msg.log"), "a") as f:
        if opt == "l":
            f.write(f"LOG {datetime.now()}: {msg}\n")
            print(f"LOG {datetime.now()}: {msg}\n")
        elif opt == "m":
            f.write(f"MSG {datetime.now()}: {msg}\n")
            print(f"MSG {datetime.now()}: {msg}\n")
    return


def write_metrics(metrics, url, apikey):
    headers = {
        "Authorization": f"Bearer {GRAFANA_USER}:{GRAFANA_APIKEY}"
    }
    grafana_data = []
    for m in metrics:
        grafana_data.append(
            {
                'name': m[0],
                'metric': m[0],
                'value': float(m[2]),
                'interval': int(m[1]),
                'unit': '',
                'time': int(m[3].timestamp()),
                'mtype': 'count',
                'tags': [],
            }
        )
    # sort by ts
    grafana_data.sort(key=lambda obj: obj['time'])
    result = requests.post(url, json=grafana_data, headers=headers)
    if result.status_code != 200:
        raise Exception(result.text)
    writeLogs('%s: %s' % (result.status_code, result.text), "m")


def on_message(client, userdata, msg):
    # standar interface to library
    try:
        d = msg.payload.decode()
    except:
        writeLogs(msg)("Cannot decode string")
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

    metrics = []
    for k, v in zip(data.keys(), data.values()):
        if k != "stationId":
            metrics.append(
                (f"{stationId}.{k}", 180,  v, now),
            )

    try:
        write_metrics(metrics, GRAFANA_URL, GRAFANA_APIKEY)
    except:
        writeLogs("Error while sending data to grafana", "m")
        return


mqtt_client = Client("python_logger", clean_session=False)
#mqtt_client.connect("127.0.0.1", keepalive=20)
mqtt_client.username_pw_set(secrets["loggerUser"], secrets["loggerPassword"])
mqtt_client.connect(secrets["broker_ip"], keepalive=20)
mqtt_client.subscribe("esp32/sensors", qos=1)
mqtt_client.on_message = on_message
mqtt_client.loop_forever()
