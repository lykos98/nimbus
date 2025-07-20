import requests
import pandas as pd

#r = requests.get("http://127.0.0.1:5000/api/sensor_data?start=2025-07-15T00:00:00&end=2025-07-17T00:00:00&station=esp_ulp_test&win=None")
r = requests.get("http://127.0.0.1:5000/api/stations/esp_ulp_test?start=2025-07-15T00:00:00&end=2025-07-17T00:00:00&win=None")

data = r.json()
print(type(data))
df = pd.DataFrame.from_dict(data)
print(df.head())

r = requests.get("http://127.0.0.1:5000/api/stations")
print(r.json())
print(r.status_code)

