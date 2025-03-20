allowed_meausures = ['airTemperature', 'airHumidity', 'airPressure', 'batteryV', 'solarV', 'windSpeed', 'windDirection']
column_ignore = ['table', 'result', '_start', '_stop', '_time', '_measurement', 'stationId', 'RSSI']
icons = {
        'airTemperature' : 'assets/temp.svg',
        'batteryV'       : 'assets/batt.png',
        'solarV'         : 'assets/solr.png',
        'airPressure'    : 'assets/press.png',
        'airHumidity'    : 'assets/hum.png',
        'windSpeed'      : 'assets/wind_speed.png',
        'windDirection'  : 'assets/wind_dir.png'
        }

measures_colors = {
        'airTemperature' : {'line_color' : "rgb(228,0,0)", "fillcolor" : "rgba(228,0,0,0.25)"},
        'batteryV'       : {'line_color' : "rgb(171,99,250)", "fillcolor" : "rgba(171,99,250,0.25)"},
        'solarV'         : {'line_color' : "rgb(255,161,90)", "fillcolor" : "rgba(255,161,90,0.25)"},
        'airPressure'    : {'line_color' : "rgb(186,176,172)", "fillcolor" : "rgba(168,176,172,0.25)"},
        'airHumidity'    : {'line_color' : "rgb(76,120,168)", "fillcolor" : "rgba(76,120,168,0.25)"},
        'windSpeed'      : {'line_color' : "rgb(76,120,168)", "fillcolor" : "rgba(76,120,168,0.25)"},
        'windDirection'  : {'line_color' : "rgb(255,161,90)", "fillcolor" : "rgba(255,161,90,0.25)"},
}

measures_names = {
        'airTemperature' : "Air Temperature",
        'batteryV'       : "Battery Voltage",
        'solarV'         : "Panel Voltage",
        'airPressure'    : "Air Pressure",
        'airHumidity'    : "Air Humidity", 
        'windSpeed'      : "Wind Speed",
        'windDirection'  : "Wind Direction"
}

measures_units = {
        'airTemperature' : "°C",
        'batteryV'       : "V",
        'solarV'         : "V",
        'airPressure'    : "hPa",
        'airHumidity'    : "%",
        'windSpeed'      : "m/s",
        'windDirection'  : "°"
}

title_font_settings = {'size' : 20, 'weight' : 'bold'}
axis_font_settings = {'size' : 15}

layout = {
        'airTemperature' : {'title' : 'Temperature', 'font' : axis_font_settings, 'title_font' : title_font_settings},
        'batteryV'       : {'title' : 'Battery Voltage', 'font' : axis_font_settings, 'title_font' : title_font_settings},
        'solarV'         : {'title' : 'Panel Voltage', 'font' : axis_font_settings, 'title_font' : title_font_settings},
        'airPressure'    : {'title' : 'Pressure', 'font' : axis_font_settings, 'title_font' : title_font_settings},
        'airHumidity'    : {'title' : 'Humidity', 'font' : axis_font_settings, 'title_font' : title_font_settings},
        'windSpeed'      : {'title' : 'Wind Speed', 'font' : axis_font_settings, 'title_font' : title_font_settings},
        'windDirection'  : {'title' : 'Wind Direction', 'font' : axis_font_settings, 'title_font' : title_font_settings}

        }
windows = {
        "None"    : "",
        "Hour"    : "1h",
        "Daily"   : "1d",
        "Weekly"  : "1w",
        }
