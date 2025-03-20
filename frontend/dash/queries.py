from utils import windows

def get_stations(start, stop, api, org):
    params = {"start": start, "stop": stop}
    query = ''' import "influxdata/influxdb/schema"
                schema.tagValues(bucket: "t2", tag: "stationId", start: start, stop: stop) \
            ''' 
    res = api.query_csv(org = org, query = query, params = params)
    stations = [r[-1] for r in res if r[-2] == '0']
    return stations

def get_df(t_start, t_end, station, win, api, org):
    win_selector = windows[win]

    params = {"t_start": t_start, "t_end": t_end, "station": station,
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
    res = api.query_data_frame(org = org, query = query, params = params )
    return res


def get_last(station, api, org):
    params = {"station": station}
    query = '''  from(bucket:"t2") 
                    |> range(start: -1w)
                    |> filter(fn: (r) => r._measurement == "sensors")
                    |> filter(fn: (r) => r["stationId"] == station)
                    |> last()
                    |> pivot(rowKey: ["_time"], columnKey: ["_field"], valueColumn: "_value")
                    |> yield()
            '''
    res = api.query_data_frame(org = org, query = query, params = params)
    return res
