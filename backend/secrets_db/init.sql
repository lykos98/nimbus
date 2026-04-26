-- =====================================================
-- NIMBUS DATABASE SCHEMA
-- =====================================================

-- Drop existing tables if starting fresh (uncomment if needed)
-- DROP TABLE IF EXISTS station_messages CASCADE;
-- DROP TABLE IF EXISTS stations CASCADE;
-- DROP TABLE IF EXISTS users CASCADE;

CREATE TABLE users (
    id SERIAL PRIMARY KEY,
    username VARCHAR(80) UNIQUE NOT NULL,
    password_hash VARCHAR(256) NOT NULL,
    is_admin BOOLEAN NOT NULL DEFAULT FALSE
);

CREATE TABLE stations (
    id SERIAL PRIMARY KEY,
    station_id VARCHAR(80) UNIQUE NOT NULL,
    secret VARCHAR(120) NOT NULL,
    user_id INTEGER NOT NULL,
    description VARCHAR(500),
    is_public BOOLEAN DEFAULT TRUE,
    last_seen TIMESTAMP,
    FOREIGN KEY (user_id) REFERENCES users (id)
);

CREATE TABLE station_messages (
    id SERIAL PRIMARY KEY,
    station_id INTEGER NOT NULL,
    message TEXT,
    level VARCHAR(20) DEFAULT 'info',
    created_at TIMESTAMP DEFAULT NOW(),
    FOREIGN KEY (station_id) REFERENCES stations (id)
);

CREATE INDEX idx_station_messages_station_id_created_at 
ON station_messages (station_id, created_at DESC);

-- =====================================================
-- MIGRATION SCRIPT (for existing databases)
-- Run these commands if you already have the old schema
-- =====================================================

-- ALTER TABLE stations ADD COLUMN description VARCHAR(500);
-- ALTER TABLE stations ADD COLUMN is_public BOOLEAN DEFAULT TRUE;
-- ALTER TABLE stations ADD COLUMN last_seen TIMESTAMP;

-- CREATE TABLE IF NOT EXISTS station_messages (
--     id SERIAL PRIMARY KEY,
--     station_id INTEGER NOT NULL,
--     message TEXT,
--     level VARCHAR(20) DEFAULT 'info',
--     created_at TIMESTAMP DEFAULT NOW(),
--     FOREIGN KEY (station_id) REFERENCES stations (id)
-- );

-- CREATE INDEX IF NOT EXISTS idx_station_messages_station_id_created_at 
-- ON station_messages (station_id, created_at DESC);
