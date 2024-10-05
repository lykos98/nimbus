allowed_meausures = ['airTemperature', 'airHumidity', 'airPressure', 'batteryV', 'solarV']
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
        'airTemperature' : {'line_color' : "#E45756"},
        'batteryV'       : {'line_color' : "#AB63FA"},
        'solarV'         : {'line_color' : "#FFA15A"},
        'airPressure'    : {'line_color' : "#BAB0AC"},
        'airHumidity'    : {'line_color' : "#4C78A8"}
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
