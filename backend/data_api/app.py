import influxdb_client
from flask import Flask, request, jsonify, g
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

def get_influx_db_client():
    if "influx_db_client" not in g:
        g.influx_db_client = influxdb_client.InfluxDBClient(url = INFLUX_URL, token = INFLUX_TOKEN, org = INFLUX_ORG)
    return g.influx_db_client

@app.teardown_appcontext
def close_influxdb_client(exception):
    client = g.pop('influxdb_client', None)
    if client is not None:
        client.close()


@app.route('/api/stations', methods=['GET'])
def get_stations():

    query = ''' import "influxdata/influxdb/schema"
                schema.tagValues(bucket: "t2", tag: "stationId", start: t_start, stop: t_end) \
            ''' 
    try:
        if request.args.get('start'):
            start = datetime.fromisoformat(request.args.get('start'))
        else:
            start = datetime.now() + timedelta(days = -1) 

        if request.args.get('end'):
            end = datetime.fromisoformat(request.args.get('end'))
        else:
            end = datetime.now()


        client = get_influx_db_client()
        api = client.query_api()

        params = {"t_start": start, "t_end": end}
        res = api.query_csv(org = INFLUX_ORG, query = query, params = params)
        stations = [r[-1] for r in res if r[-2] == '0']

        return jsonify(stations)
    except:
        return jsonify({"error" : "Cannot perform query"}), 400
        

# TODO: put try except

@app.route('/api/stations/<string:station>/data', methods=['GET'])
def get_df(station: str):
    start = request.args.get('start')
    stop  = request.args.get('stop')
    #station = request.args.get('station')

    start = datetime.fromisoformat(start)
    stop  = datetime.fromisoformat(stop)

    if "win" not in request.args:
        win = "None"
    else:
        win = request.args.get('win')

    client = get_influx_db_client()
    api = client.query_api()

    win_selector = windows[win]

    params = {"t_start": start, "t_stop": stop, "station": station,
              "win_selector": win_selector}
    if win == "None":
        query = '''  from(bucket:"t2") 
                        |> range(start: t_start, stop: t_stop)
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

    return res.to_json()



@app.route('/api/stations/<string:station>/last', methods=['GET'])
def get_last(station):
    params = {"station": station,
              "t_start": datetime.now() + timedelta(hours=-1),
              "t_stop" : datetime.now()}
    query = '''  from(bucket:"t2") 
                    |> range(start: t_start, stop: t_stop)
                    |> filter(fn: (r) => r._measurement == "sensors")
                    |> filter(fn: (r) => r["stationId"] == station)
                    |> last()
                    |> pivot(rowKey: ["_time"], columnKey: ["_field"], valueColumn: "_value")
                    |> yield()
            '''

    client = get_influx_db_client()
    api = client.query_api()
    res = api.query_data_frame(org = INFLUX_ORG, query = query, params = params)
    print(res.head())
    print(res.to_json())
    return res.to_json()

if __name__ == '__main__':
    app.run(debug=False, port=5555) # In production, set debug=False and use a WSGI server
