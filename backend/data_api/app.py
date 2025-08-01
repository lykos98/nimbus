import influxdb_client
from flask import Flask, request, jsonify
from influxdb_client.client.write_api import SYNCHRONOUS
import os
from datetime import datetime, timedelta
from utils import windows

app = Flask(__name__)

# InfluxDB Configuration (store securely, e.g., environment variables)
INFLUX_URL = os.getenv("INFLUX_BASEURL")
INFLUX_TOKEN = os.getenv("INFLUX_TOKEN") 
INFLUX_ORG = os.getenv("INFLUX_ORG")
INFLUX_BUCKET = os.getenv("INFLUX_BUCKET")

@app.route('/api/stations', methods=['GET'])
def get_stations():

    query = ''' import "influxdata/influxdb/schema"
                schema.tagValues(bucket: "t2", tag: "stationId", start: t_start) \
            ''' 
    try:
        client = influxdb_client.InfluxDBClient(url = INFLUX_URL, token = INFLUX_TOKEN, org = INFLUX_ORG)
        api = client.query_api()
        start = datetime.now() + timedelta(days = -1)
        print(start)
        params = {"t_start": start}
        res = api.query_csv(org = INFLUX_ORG, query = query, params = params)
        stations = [r[-1] for r in res if r[-2] == '0']

        return jsonify(stations)
    except:
        raise
        return jsonify({"error" : "Cannot perform query"}), 400
        




@app.route('/api/stations/<string:station>', methods=['GET'])
def get_df(station: str):
    start = request.args.get('start')
    end   = request.args.get('end')
    #station = request.args.get('station')

    start = datetime.fromisoformat(start)
    end   = datetime.fromisoformat(end)

    if "win" not in request.args:
        win = "None"
    else:
        win = request.args.get('win')

    for a in request.args:
        print(a)

    client = influxdb_client.InfluxDBClient(url = INFLUX_URL, token = INFLUX_TOKEN, org = INFLUX_ORG)
    api = client.query_api()

    win_selector = windows[win]

    params = {"t_start": start, "t_end": end, "station": station,
              "win_selector": win_selector}
    if win == "None":
        query = '''  from(bucket:"t2") 
                        |> range(start: t_start, stop: t_end)
                        |> filter(fn: (r) => r._measurement == "sensors") 
                        |> filter(fn: (r) => r["stationId"] == station) 
                        |> pivot(rowKey: ["_time"], columnKey: ["_field"], valueColumn: "_value") 
                        |> yield() 
                ''' 
    else:
        query = '''  from(bucket:"t2") 
                        |> range(start: t_start, stop: t_end)
                        |> filter(fn: (r) => r._measurement == "sensors") 
                        |> filter(fn: (r) => r["stationId"] == station) 
                        |> aggregateWindow(every: duration(v: win_selector), fn: mean, createEmpty: false)
                        |> pivot(rowKey: ["_time"], columnKey: ["_field"], valueColumn: "_value") 
                        |> yield() 
                ''' 
    res = api.query_data_frame(org = INFLUX_ORG, query = query, params = params )

    #return res.to_json(orient = "records", date_format = "iso")
    return res.to_json()

if __name__ == '__main__':
    app.run(debug=True, port=5000) # In production, set debug=False and use a WSGI server
