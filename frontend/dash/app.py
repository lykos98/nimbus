import dash
import pytz
import influxdb_client
import os

import plotly.express as px
import dash_bootstrap_components as dbc

from dash import Dash, dash_table, html, dcc, callback, Output, Input
from datetime import datetime, timedelta
from utils import measures_names, measures_colors, measures_units
from utils import layout, icons, allowed_meausures, column_ignore
from queries import get_stations, get_last, get_df

external_stylesheets = [
        'https://fonts.googleapis.com/css2?family=SUSE:wght@100..800&display=swap',
        dbc.themes.BOOTSTRAP
        ]

app = dash.Dash(__name__, external_stylesheets=external_stylesheets)

tz = pytz.timezone("Europe/Rome")

url = "https://influx.lykos.cc"
org = "lykos-corp"
token = os.environ['INFLUX_TOKEN']
bucket = "t2"

client = influxdb_client.InfluxDBClient(url = url, token = token, org = org)
query_api = client.query_api()

t_start = datetime.now(pytz.timezone("Europe/Rome")) - timedelta(days = 1)
t_stop  = datetime.now(pytz.timezone("Europe/Rome"))

# ------------------------------------------------------------------------------
# CALLBACKS
# ------------------------------------------------------------------------------


def localize_time(date: datetime):
    return tz.localize(date)    

@callback(
        Output('date-picker', 'start_date'),
        Output('date-picker', 'end_date'),
        Input('date-picker', 'min_date_allowed'),
        )
def today_date(vv):
    end_date = (datetime.now()).date()
    start_date = (datetime.now() - timedelta(days = 2)).date()
    print(start_date, end_date)
    return start_date, end_date

@callback(
    Output('station-id-dropdown', 'options'),
    Input('date-picker', 'start_date'),
    Input('date-picker', 'end_date'),
    )
def retrieve_station(start_date, end_date):
    t_start = datetime.fromisoformat(f"{start_date}T00:00:00.0000")
    t_start = localize_time(t_start) 

    t_stop = datetime.fromisoformat(f"{end_date}T23:59:59.0000")
    t_stop = localize_time(t_stop) 

    print(t_start)
    print(t_stop)
    stations = get_stations(t_start, t_stop, query_api, org) 
    print(stations)
    return stations



    
@callback(
        Output('gauges', 'children'),
        Input('station-id-dropdown', 'value'),
        Input('interval-component', 'n_intervals')
        )
def update_gauges(station, interval):
    if not (station is None):
        res = get_last(station, query_api, org)

        ret_children = []
        ret_children.append(html.Div([html.H2(station)]))

        cols = [col for col in allowed_meausures if col in res.columns]
        print(res.head())

        for i, col in enumerate(cols):
            val =  res[col].values[0]
            if col == "airPressure":
                val = val/100
            cc = []

            name = f"{measures_names[col]}"
            value = f"{val : .1f} {measures_units[col]}"
            print(value)
            cc.append(dbc.Col(html.P(f"{name}", style = {"textAlign" : "center", 
                                                         "fontSize" : "16px", 
                                                         "color" : "rgba(55, 12, 146, 1)", 
                                                         "fontWeight" : "bold", 
                                                         "margin" : "auto"}),
                              style = {"margin" : "auto"}))
            cc.append(dbc.Col(html.Img(src = icons[col] , style = {'height' : "40px", 'margin' : 'auto'}), 
                              style = {"margin" : "auto", 'textAlign' : 'center'}))
            cc.append(dbc.Col(html.P(f"{value}", style = {"textAlign" : "center", "fontSize" : "18px", "margin" : "auto", "whiteSpace" : "nowrap"}), 
                              style = {"margin" : "auto", "flexGrow" : "1", "padding" : "1em"}))
            ret_children.append(dbc.Col(dbc.Card(dbc.Row(cc, style = {'marigin' :  'auto'}), 
                                                 className = "card-measures", 
                                                 style = {"padding" : "10px", "margin" : "auto", "minWidth" : "150px", "marginTop" : "10px" }
                                                 ), style = {'margin' : 'auto'}
                                        ))

        dt = str((res["_time"].values[0])).split("T")
        date = dt[0]
        time = dt[1].split(".")[0]
        ret_children.append(html.Div([html.P(f"Last measurement: {date} at {time}")]))
        return dbc.Row(ret_children)
    else:
        return html.H2("No station selected")

@callback(
    Output('graph-content', 'children'),
    Output('data-table', 'children'),
    #Output('gauges', 'figure'),
    Input('station-id-dropdown', 'value'),
    Input('date-picker', 'start_date'),
    Input('date-picker', 'end_date'),
    Input('interval-component', 'n_intervals'),
    Input('aggregate-window-selector', 'value')
)
def update_graph(station, start_date, end_date, interval, win):
    import numpy as np
    t_start = datetime.fromisoformat(f"{start_date}T00:00:00.0000")
    t_start = localize_time(t_start) 

    t_stop = datetime.fromisoformat(f"{end_date}T23:59:59.0000")
    t_stop = localize_time(t_stop) 
    print(station)

    if not (station is None):
        df = get_df(t_start, t_stop, station, win, query_api, org) 
        cc = [c for c in df.columns if c not in column_ignore]
        idx = 0
        figs = []
        
        for c in allowed_meausures:
            if c in cc:
                print(c)

                if c == 'windDirection':
                    sizes = np.array(df["windSpeed"].values) + 8
                    sw = np.isnan(sizes)
                    sizes[sw] = 0.
                    fig = px.scatter_polar(df, r = '_time', theta = "windDirection", color= "windSpeed",
                                           size = 8 + sizes, template='plotly_white', color_continuous_scale="viridis")
                    fig.update_traces(measures_colors[c])
                    fig.update_traces(marker_line=dict(width=0))
                else:
                    fig = px.line(df, x='_time', y=c, template='plotly_white', markers=True, line_shape='linear') 
                    fig.update_traces(measures_colors[c], fill = 'tonexty', marker = {'size' : 3})
                #fig.update_traces(measures_colors[c], marker = {'size' : 4})
                fig.update_layout(layout[c], paper_bgcolor='rgba(0,0,0,0)', xaxis_title = 'time', font_family = "SUSE" )
                fig.update_layout(margin=dict(l=10, r=20, t=40, b=10))
                fig.update_traces(
                    hoverlabel=dict(
                        font_family="SUSE",   # Font type
                        namelength=-1          # Prevents trace name from appearing
                    )
                )

                if(c == 'batteryV'):
                    fig.update_yaxes(range = [0,4.5])
                if(c == 'solarV'):
                    fig.update_yaxes(range = [0,6])
                if(c == 'airHumidity'):
                    fig.update_yaxes(range = [0,110])
                if(c == 'airPressure'):
                    fig.update_yaxes(range = [90000,110000])
                if(c == 'windDirection'):
                    fig.update_layout(
                            polar=dict(
                                angularaxis=dict(
                                    direction="clockwise",  # Flips the axis rotation
                                    rotation=180,  # Rotates the axis by 180 degrees
                                )
                            )
                        )

                figs.append(dbc.Col(html.Div(
                                dcc.Graph(
                                    figure = fig,
                                    className = 'graph-figure',
                                    style = {'borderRadius' : '10px', 'backgroundColor' : 'white',
                                             'borderColor' : 'rgba(16, 55, 92, 0.4)',
                                             'aspectRatio' : 16 / 9,
                                             'borderWidth' : '2px',
                                             #'borderStyle' : 'solid', 
                                             'height' : '100%', 
                                             'width' : '100%', 
                                             'margin' : 'auto', 
                                             'padding' : 'auto',
                                             'marginTop' : '10px',
                                             'paddingTop' : '10px'},
                                    ),
                                ),
                                md = 6, xs = 12
                            )
                        )
                idx += 1

        df = df.drop(columns =[c for c in df.columns if c in ["table", "result", "_start", "_stop", "_measurement", "RSSI"]])
        data_table = dash_table.DataTable(
                     id="table",
                     columns=[{"name": i, "id": i} for i in df.columns],
                     data=df.to_dict("records"),
                     export_format="csv",
                )
        
        figs2 = [dbc.Row(figs[i:min(i+2, len(figs))], className = "mb4") for i in range(0,len(figs),2)]
        return figs2, data_table
    else:
        return html.Div(), html.Div() 

# ------------------------------------------------------------------------------
# LAYOUT
# ------------------------------------------------------------------------------

# Create Dash App
app = Dash(external_stylesheets=external_stylesheets)

# Layout of the dashboard
app.layout = html.Div(style={ 'padding': '20px', "fontFamily" : "SUSE", }, children=[
    # Header
    dbc.Row(
        dbc.Col(html.H2("Nimbus 0.0.1", style={ 'fontSize': '40px', 'textAlign': 'center', 'padding' : '1em'}),
                width={"size": 12, 'offset': 0}, className = 'bg-primary bg-opacity-10 rounded-3' )
    ),
    
    # Date and Station Selector Row
    dbc.Row([
        dbc.Col([
            html.Label("Select Date Range:", style={ 'fontSize': '18px'}),
            dcc.DatePickerRange(
                id='date-picker',
                min_date_allowed = datetime(2024,1,1).date(),
                display_format='YYYY-MM-DD',
                style={'width': '100%', 'padding': '10px'}
            )
        ], md=6, xs=12),
        
        dbc.Col([
            dbc.Row([
                    dbc.Col([
                        html.Label("Select Station ID:", style={'fontSize': '17px'}),
                        dcc.Dropdown(
                            id='station-id-dropdown',
                            style={'width': '100%', })
                        ], width = 6),
                    dbc.Col([
                        html.Label("Average window:", style={'fontSize': '17px'}),
                        dcc.Dropdown(
                            id='aggregate-window-selector',
                            options = ["None", "Hour", "Daily", "Weekly"],
                            style={'width': '100%', }, value = "None"),
                        ], width = 6) 
                    ], style = {'padding' : '10px', 'justifyContent' : 'space-around'}),

        ], xs = 12, md = 6),
        dcc.Interval(
            id='interval-component',
            interval=60*1000, # in milliseconds
            n_intervals=0
        )
        ], style={'marginTop': '20px', 'marginBottom' : '20px'}),
    
    # Content Area
    dbc.Tabs([
        dbc.Tab(label = "Graphs",
                children = [dcc.Loading(dbc.Row(id = 'gauges', className = "m-3", style = {'margin' : 'auto', 'padding' : 'auto'}), delay_show = 500, delay_hide = 500),
                            dcc.Loading(dbc.Row(id = 'graph-content', 
                                                className = 'graph-content', 
                                                style={'marginTop': '30px', 'justifyContent' : 'space-around' }), 
                                        overlay_style={"visibility":"visible", "opacity": .5, "backgroundColor": "white"}, 
                                        delay_hide = 500),
                        ]
            ),
        dbc.Tab(label = "Table", id = 'data-table'),
        dbc.Tab(label = "Station info", children=html.P("Lorem ipsum")),
             ]),
    
    # Footer
    dbc.Row(
        dbc.Col(html.P("Nimbus 0.0.1 - Dashboard", 
                       style={'textAlign': 'center',  'marginTop': '40px'}),
                width={"size": 12})
    )
])

# Callbacks for interactivity (not implemented for this example)
# You can add callbacks here for the date picker and station dropdown interactivity.
server = app.server
if __name__ == '__main__':
    app.run_server(debug=True, host="0.0.0.0")
