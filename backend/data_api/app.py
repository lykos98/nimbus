import influxdb_client
from flask import Flask, request, jsonify, g
from influxdb_client.client.write_api import SYNCHRONOUS
from influxdb_client import Point
import os
from datetime import timedelta, datetime 
import psycopg2
from psycopg2 import extras 
from werkzeug.security import generate_password_hash, check_password_hash 
from flask_jwt_extended import create_access_token, jwt_required, JWTManager, get_jwt_identity 
import jwt # For decoding for logging
import sys # For direct stderr logging
from utils import windows, validFields

app = Flask(__name__)

# InfluxDB Configuration (store securely, e.g., environment variables)
INFLUX_URL = os.getenv("INFLUX_BASEURL")
INFLUX_TOKEN = os.getenv("INFLUX_TOKEN") 
INFLUX_ORG = os.getenv("INFLUX_ORG")
INFLUX_BUCKET = os.getenv("INFLUX_BUCKET")

# PostgreSQL Configuration
DB_HOST = os.getenv("DB_HOST", "localhost")
DB_NAME = os.getenv("DB_NAME", "nimbus_secrets")
DB_USER = os.getenv("DB_USER")
DB_PASSWORD = os.getenv("DB_PASSWORD")

# JWT Configuration
app.config["JWT_SECRET_KEY"] = os.getenv("JWT_SECRET_KEY") 
jwt_manager = JWTManager(app)

@app.before_request
def log_jwt_info():
    """Diagnostic logging to inspect JWT identity."""
    if 'Authorization' in request.headers:
        auth_header = request.headers['Authorization']
        if auth_header.startswith('Bearer '):
            token = auth_header.split()[1]
            try:
                # Decode without verification to inspect payload
                decoded_token = jwt.decode(token, options={"verify_signature": False})
                app.logger.info(f"Incoming JWT 'sub' (identity): {decoded_token.get('sub')}")
            except Exception as e:
                app.logger.error(f"Could not decode JWT for logging: {e}")

def get_db_connection():
    if "db_conn" not in g:
        g.db_conn = psycopg2.connect(host=DB_HOST, database=DB_NAME, user=DB_USER, password=DB_PASSWORD)
    return g.db_conn

@app.teardown_appcontext
def close_db_connection(exception):
    conn = g.pop("db_conn", None)
    if conn is not None:
        conn.close()

def get_influx_db_client():
    if "influx_db_client" not in g:
        g.influx_db_client = influxdb_client.InfluxDBClient(url = INFLUX_URL, token = INFLUX_TOKEN, org = INFLUX_ORG)
    return g.influx_db_client

@app.teardown_appcontext
def close_influxdb_client(exception):
    client = g.pop('influxdb_client', None)
    if client is not None:
        client.close()

# User Login Endpoint
@app.route("/api/login", methods=["POST"])
def login():
    username = request.json.get("username", None)
    password = request.json.get("password", None)

    conn = get_db_connection()
    cur = conn.cursor(cursor_factory=psycopg2.extras.DictCursor)
    cur.execute("SELECT id, username, password_hash, is_admin FROM users WHERE username = %s", (username,))
    user = cur.fetchone()
    cur.close()

    if user and check_password_hash(user["password_hash"], password):
        # CORRECT: Use a simple string for the identity
        access_token = create_access_token(identity=str(user["id"]))
        return jsonify(access_token=access_token), 200
    return jsonify({"msg": "Bad username or password"}), 401


# Version endpoint for debugging deployment
@app.route("/api/version")
def version():
    return jsonify({"version": "1.1.0-debug"}), 200


# Profile Endpoint to get current user's details
@app.route("/api/profile")
@jwt_required()
def profile():
    # CORRECT: Get the string ID from the token
    current_user_id = get_jwt_identity()
    conn = get_db_connection()
    cur = conn.cursor(cursor_factory=psycopg2.extras.DictCursor)
    # CORRECT: Fetch user from DB using the ID
    cur.execute("SELECT id, username, is_admin FROM users WHERE id = %s", (int(current_user_id),))
    user = cur.fetchone()
    cur.close()
    if not user:
        return jsonify({"msg": "User not found"}), 404
    return jsonify(dict(user)), 200


@app.route('/api/stations', methods=['GET'])
def get_stations():

    query = ''' import "influxdata/influxdb/schema"
                schema.tagValues(bucket: "t2", tag: "stationId", start: t_start, stop: t_end) 
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
        

@app.route('/api/stations/<string:station>/data', methods=['GET', 'POST'])
def get_df(station: str):
    if request.method == "GET":
        try: 
            start = request.args.get('start')
            stop  = request.args.get('stop')
            
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
                                |> range(start: t_start, stop: t_stop)
                                |> filter(fn: (r) => r._measurement == "sensors") 
                                |> filter(fn: (r) => r["stationId"] == station) 
                                |> aggregateWindow(every: duration(v: win_selector), fn: mean, createEmpty: false)
                                |> pivot(rowKey: ["_time"], columnKey: ["_field"], valueColumn: "_value") 
                                |> yield() 
                        ''' 
            res = api.query_data_frame(org = INFLUX_ORG, query = query, params = params )

            return res.to_json(), 200
        except Exception as e:
            app.logger.error(f"Error in GET /api/stations/.../data: {e}")
            return jsonify({"error": "Cannot perform query"}), 400
    elif request.method == "POST":
        try:
            data_json = request.get_json()
            
            station_id_payload = data_json.get("stationId")
            provided_secret = data_json.get("secret")
            
            if not station_id_payload or not provided_secret:
                return jsonify({"status": "error", "message": "stationId and secret are required"}), 400

            conn = get_db_connection()
            cur = conn.cursor(cursor_factory=psycopg2.extras.DictCursor)
            cur.execute("SELECT secret FROM stations WHERE station_id = %s", (station_id_payload,))
            station_record = cur.fetchone()
            cur.close()

            if not station_record or provided_secret != station_record["secret"]:
                return jsonify({"status": "error", "message": "Unauthorized"}), 401

            del data_json['secret']
            
            client = get_influx_db_client()
            write_api = client.write_api(write_options=SYNCHRONOUS)
            
            point = Point("sensors")
            point.tag("stationId", data_json['stationId'])
            
            for k in data_json.keys(): 
                if k != "stationId" and k in validFields:
                    point.field(k, float(data_json[k]))
            
            point.time(datetime.now())
            write_api.write(bucket=INFLUX_BUCKET, org=INFLUX_ORG, record=point)
            app.logger.info(f" -> Recieved: {data_json}")
            print(f"STDERR LOG -> Recieved: {data_json}", file=sys.stderr)
            return jsonify({"status": "success"}), 201
        except Exception as e:
            return jsonify({"status": "error", "message": str(e)}), 400

@app.route('/api/stations/<string:station>/last', methods=['GET'])
def get_last(station):
    try:
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
        return res.to_json(), 200
    except:
        return jsonify({"error" : "Cannot perform query"}), 400

# Station Management Endpoints (Protected)
@app.route("/api/admin/stations", methods=["GET", "POST"])
@jwt_required()
def admin_stations_management():
    current_user_id = get_jwt_identity()
    conn = get_db_connection()
    cur = conn.cursor(cursor_factory=psycopg2.extras.DictCursor)

    cur.execute("SELECT * FROM users WHERE id = %s", (int(current_user_id),))
    current_user = cur.fetchone()

    if not current_user or not current_user["is_admin"]:
        cur.close()
        return jsonify({"msg": "Administration access required"}), 403

    if request.method == "GET":
        cur.execute("SELECT id, station_id, user_id, secret FROM stations")
        stations = cur.fetchall()
        cur.close()
        return jsonify([dict(s) for s in stations]), 200
    
    elif request.method == "POST":
        station_id = request.json.get("station_id", None)
        user_id = request.json.get("user_id", None)
        if not station_id or not user_id:
            cur.close()
            return jsonify({"msg": "station_id and user_id are required"}), 400

        secret = os.urandom(16).hex()

        try:
            cur.execute("INSERT INTO stations (station_id, secret, user_id) VALUES (%s, %s, %s) RETURNING id", (station_id, secret, user_id))
            new_station_id = cur.fetchone()[0]
            conn.commit()
            cur.close()
            return jsonify({"id": new_station_id, "station_id": station_id, "secret": secret, "user_id": user_id}), 201
        except Exception as e:
            conn.rollback()
            cur.close()
            return jsonify({"msg": str(e)}), 500

@app.route("/api/admin/stations/<int:station_id>", methods=["DELETE"])
@jwt_required()
def admin_delete_station(station_id):
    current_user_id = get_jwt_identity()
    conn = get_db_connection()
    cur = conn.cursor(cursor_factory=psycopg2.extras.DictCursor)

    cur.execute("SELECT * FROM users WHERE id = %s", (int(current_user_id),))
    current_user = cur.fetchone()

    if not current_user or not current_user["is_admin"]:
        cur.close()
        return jsonify({"msg": "Administration access required"}), 403

    cur.execute("DELETE FROM stations WHERE id = %s", (station_id,))
    conn.commit()
    rows_deleted = cur.rowcount
    cur.close()

    if rows_deleted == 0:
        return jsonify({"msg": "Station not found"}), 404
    return jsonify({"msg": "Station deleted successfully"}), 200

@app.route("/api/user/stations", methods=["GET"])
@jwt_required()
def user_stations():
    current_user_id = get_jwt_identity()
    conn = get_db_connection()
    cur = conn.cursor(cursor_factory=psycopg2.extras.DictCursor)

    cur.execute("SELECT * FROM users WHERE id = %s", (int(current_user_id),))
    current_user = cur.fetchone()
    if not current_user:
        cur.close()
        return jsonify({"msg": "User not found"}), 404

    if current_user["is_admin"]:
        cur.execute("SELECT id, station_id, user_id, secret FROM stations")
    else:
        cur.execute("SELECT id, station_id, user_id, secret FROM stations WHERE user_id = %s", (current_user["id"],))
    
    stations = cur.fetchall()
    cur.close()
    return jsonify([dict(s) for s in stations]), 200

# User Management Endpoints (Admin Only)
@app.route("/api/admin/users", methods=["GET", "POST"])
@jwt_required()
def admin_users_management():
    current_user_id = get_jwt_identity()
    conn = get_db_connection()
    cur = conn.cursor(cursor_factory=psycopg2.extras.DictCursor)

    cur.execute("SELECT * FROM users WHERE id = %s", (int(current_user_id),))
    current_user = cur.fetchone()
    
    if not current_user or not current_user["is_admin"]:
        cur.close()
        return jsonify({"msg": "Administration access required"}), 403

    if request.method == "GET":
        cur.execute("SELECT id, username, is_admin FROM users")
        users = cur.fetchall()
        cur.close()
        return jsonify([dict(u) for u in users]), 200
    
    elif request.method == "POST":
        username = request.json.get("username", None)
        password = request.json.get("password", None)
        is_admin = request.json.get("is_admin", False)

        if not username or not password:
            cur.close()
            return jsonify({"msg": "Username and password are required"}), 400

        hashed_password = generate_password_hash(password)

        try:
            cur.execute("INSERT INTO users (username, password_hash, is_admin) VALUES (%s, %s, %s) RETURNING id", (username, hashed_password, is_admin))
            new_user_id = cur.fetchone()[0]
            conn.commit()
            cur.close()
            return jsonify({"id": new_user_id, "username": username, "is_admin": is_admin}), 201
        except Exception as e:
            conn.rollback()
            cur.close()
            return jsonify({"msg": str(e)}), 500

@app.route("/api/admin/users/<int:user_id>", methods=["PUT", "DELETE"])
@jwt_required()
def admin_user_detail_management(user_id):
    current_user_id = get_jwt_identity()
    conn = get_db_connection()
    cur = conn.cursor(cursor_factory=psycopg2.extras.DictCursor)
    
    cur.execute("SELECT * FROM users WHERE id = %s", (int(current_user_id),))
    current_user = cur.fetchone()

    if not current_user or not current_user["is_admin"]:
        cur.close()
        return jsonify({"msg": "Administration access required"}), 403

    if request.method == "PUT":
        username = request.json.get("username", None)
        password = request.json.get("password", None)
        is_admin = request.json.get("is_admin", None)

        updates = []
        params = []
        if username:
            updates.append("username = %s")
            params.append(username)
        if password:
            hashed_password = generate_password_hash(password)
            updates.append("password_hash = %s")
            params.append(hashed_password)
        if is_admin is not None:
            updates.append("is_admin = %s")
            params.append(is_admin)
        
        if not updates:
            cur.close()
            return jsonify({"msg": "No fields to update"}), 400
        
        params.append(user_id)
        update_query = f"UPDATE users SET {', '.join(updates)} WHERE id = %s"
        try:
            cur.execute(update_query, tuple(params))
            conn.commit()
            rows_updated = cur.rowcount
            cur.close()
            if rows_updated == 0:
                return jsonify({"msg": "User not found"}), 404
            return jsonify({"msg": "User updated successfully"}), 200
        except Exception as e:
            conn.rollback()
            cur.close()
            return jsonify({"msg": str(e)}), 500

    elif request.method == "DELETE":
        cur.execute("DELETE FROM users WHERE id = %s", (user_id,))
        conn.commit()
        rows_deleted = cur.rowcount
        cur.close()

        if rows_deleted == 0:
            return jsonify({"msg": "User not found"}), 404
        return jsonify({"msg": "User deleted successfully"}), 200

if __name__ == '__main__':
    app.run(debug=False, port=5555) # In production, set debug=False and use a WSGI server
