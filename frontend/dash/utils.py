allowed_meausures = ['airTemperature', 'airHumidity', 'airPressure', 'batteryV', 'solarV', 'windSpeed']
icons = {
        'airTemperature' : 'assets/temp.svg',
        'batteryV'       : 'assets/batt.png',
        'solarV'         : 'assets/solr.png',
        'airPressure'    : 'assets/press.png',
        'airHumidity'    : 'assets/hum.png',
        }
gauges = {
        'airTemperature' : 
        {
            'axis': {'range': [-10, 45], 'tickwidth': 1, 'tickcolor': "darkblue"},
            'bar': {'color': "darkblue"},
            'bgcolor': "white",
            'borderwidth': 2,
            'bordercolor': "gray",
            'steps': [
                {'range': [-10, 20], 'color': 'cyan'},
                {'range': [20, 30], 'color': 'yellow'},
                {'range': [30, 45], 'color': 'red'}],
        },
        'airHumidity' : 
        {
            'axis': {'range': [0, 100], 'tickwidth': 1, 'tickcolor': "darkblue"},
            'bar': {'color': "darkblue"},
            'bgcolor': "white",
            'borderwidth': 2,
            'bordercolor': "gray",
        },
                'airPressure' : 
        {
            'axis': {'range': [90000, 120000], 'tickwidth': 1, 'tickcolor': "darkblue"},
            'bar': {'color': "darkblue"},
            'bgcolor': "white",
            'borderwidth': 2,
            'bordercolor': "gray",
        },
        'batteryV' :
        {
            'axis': {'range': [3.3, 4.2], 'tickwidth': 1, 'tickcolor': "darkblue"},
            'bar': {'color': "darkblue"},
            'bgcolor': "white",
            'borderwidth': 2,
            'bordercolor': "gray",
            'steps': [
                {'range': [0, 3.6], 'color': 'red'},
                {'range': [3.6, 3.85], 'color': 'yellow'},
                {'range': [3.85, 4.2], 'color': 'green'}],
        },

        'solarV' :
        {
            'axis': {'range': [0, 6.0], 'tickwidth': 1, 'tickcolor': "darkblue"},
            'bar': {'color': "darkblue"},
            'bgcolor': "white",
            'borderwidth': 2,
            'bordercolor': "gray",
            'steps': [
                {'range': [0, 4.4], 'color': 'red'},
                {'range': [4.4, 5.5], 'color': 'yellow'},
                {'range': [5.5, 6], 'color': 'green'}],
        },

    }

measures_colors = {
        'airTemperature' : {'line_color' : "rgb(228,0,0)", "fillcolor" : "rgba(228,0,0,0.25)"},
        'batteryV'       : {'line_color' : "rgb(171,99,250)", "fillcolor" : "rgba(171,99,250,0.25)"},
        'solarV'         : {'line_color' : "rgb(255,161,90)", "fillcolor" : "rgba(255,161,90,0.25)"},
        'airPressure'    : {'line_color' : "rgb(186,176,172)", "fillcolor" : "rgba(168,176,172,0.25)"},
        'airHumidity'    : {'line_color' : "rgb(76,120,168)", "fillcolor" : "rgba(76,120,168,0.25)"}
}

measures_names = {
        'airTemperature' : "Air Temperature",
        'batteryV'       : "Battery Voltage",
        'solarV'         : "Panel Voltage",
        'airPressure'    : "Air Pressure",
        'airHumidity'    : "Air Humidity" 
}

measures_units = {
        'airTemperature' : "°C",
        'batteryV'       : "V",
        'solarV'         : "V",
        'airPressure'    : "hPa",
        'airHumidity'    : "%" 
}

title_font_settings = {'size' : 20, 'weight' : 'bold'}
axis_font_settings = {'size' : 15}

layout = {
        'airTemperature' : {'title' : 'Temperature', 'font' : axis_font_settings, 'title_font' : title_font_settings},
        'batteryV'       : {'title' : 'Battery Voltage', 'font' : axis_font_settings, 'title_font' : title_font_settings},
        'solarV'         : {'title' : 'Panel Voltage', 'font' : axis_font_settings, 'title_font' : title_font_settings},
        'airPressure'    : {'title' : 'Pressure', 'font' : axis_font_settings, 'title_font' : title_font_settings},
        'airHumidity'    : {'title' : 'Humidity', 'font' : axis_font_settings, 'title_font' : title_font_settings}

        }
windows = {
        "None"    : "",
        "Hour"    : "|> aggregateWindow(every: 1h, fn: mean)",
        "Daily"   : "|> aggregateWindow(every: 1d, fn: mean)",
        "Weekly"  : "|> aggregateWindow(every: 1w, fn: mean)",
        }
