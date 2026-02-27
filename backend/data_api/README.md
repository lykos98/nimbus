# Data API

This service provides a RESTful interface for managing station data, users, and station metadata. It interacts with InfluxDB for time-series sensor data and PostgreSQL for metadata (users, station secrets).

## Authentication

Most endpoints require a JWT token in the `Authorization` header:
`Authorization: Bearer <token>`

The `/api/login` endpoint is used to obtain this token.

## Endpoints

### Public / Basic
- `POST /api/login`: Authenticate a user and receive a JWT.
  - Request body: `{"username": "...", "password": "..."}`
  - Response: `{"access_token": "..."}`
- `GET /api/version`: Get the current version of the API.
- `GET /api/profile`: Get the logged-in user's profile (Requires JWT).

### Station Data (Sensor Readings)
- `GET /api/stations`: List available station IDs.
  - Query parameters (optional): `start`, `end` (ISO format).
  - Response: A JSON array of station ID strings (e.g., `["esp_ulp_test", "station_2"]`).
- `GET /api/stations/<station_id>/data`: Get time-series data for a station.
  - Query parameters:
    - `start`: (required) ISO 8601 start time (e.g., `2025-07-15T00:00:00`).
    - `stop`: (required) ISO 8601 end time (e.g., `2025-07-17T00:00:00`).
    - `win`: (optional) Aggregation window:
        - `None`: (default) No aggregation, returns all raw data.
        - `Hour`: Aggregate data by hour (mean value).
        - `Daily`: Aggregate data by day (mean value).
        - `Weekly`: Aggregate data by week (mean value).
  - Response: A JSON-serialized pandas DataFrame (columns format).
- `GET /api/stations/<station_id>/last`: Get the latest reading for a station.
  - Response: A JSON-serialized pandas DataFrame containing the most recent data point.
- `POST /api/stations/<station_id>/data`: Submit new sensor readings (Used by stations).
  - Request body (JSON):
    ```json
    {
      "stationId": "...",
      "secret": "...",
      "airTemperature": 25.5,
      ...
    }
    ```
  - *Note:* The `stationId` in the payload is used for authentication and data storage. The `secret` must match the one stored in the PostgreSQL database for that station. Supported sensor fields are listed below.

### Station Management (Protected)
- `GET /api/user/stations`: List stations owned by or accessible to the current user (Requires JWT).
- `GET /api/admin/stations`: List all stations in the system (Admin only).
- `POST /api/admin/stations`: Create a new station (Admin only).
  - Request body: `{"station_id": "...", "user_id": ...}`
- `DELETE /api/admin/stations/<station_id>`: Delete a station (Admin only).

### User Management (Admin Only)
- `GET /api/admin/users`: List all users.
- `POST /api/admin/users`: Create a new user.
  - Request body: `{"username": "...", "password": "...", "is_admin": ...}`
- `PUT /api/admin/users/<user_id>`: Update a user's details.
- `DELETE /api/admin/users/<user_id>`: Delete a user.

## Data Schema

When posting sensor data via `POST /api/stations/<station_id>/data`, the following fields are supported:

### Atmospheric
- `airTemperature`: Air temperature (°C)
- `airHumidity`: Relative humidity (%)
- `airPressure`: Atmospheric pressure (hPa)
- `airCO2ppm`: Carbon Dioxide concentration (ppm)
- `airNOXppm`: Nitrogen Oxides concentration (ppm)

### Wind and Rain
- `windDirection`: Wind direction (degrees)
- `windSpeed`: Wind speed (m/s)
- `rainmm`: Accumulated rainfall (mm)

### Terrain / Soil
- `terrainTemperature`: Soil/Ground temperature (°C)
- `terrainHumidity`: Soil moisture (%)
- `terrainPH`: Soil pH

### Water
- `waterTemperature`: Water temperature (°C)
- `waterHumidity`: Water humidity (%) - *Context dependent*
- `waterPH`: Water pH

### Power / System
- `batteryV`: Battery voltage (V)
- `batteryA`: Battery current (A)
- `solarV`: Solar panel voltage (V)
- `solarA`: Solar panel current (A)

### Mandatory Fields (for POST)
- `stationId`: The ID of the station.
- `secret`: The unique authentication secret for the station.
