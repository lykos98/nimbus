# Nimbus project
Self build weather sensor network, experimenting on esp32, WiFi, LoRa and much more. 

## Aim of the project 
Hi! I am a PhD student in Data Science and I love to build stuff with microcontrollers and similar things. 
Here I will document my journey in building my weather sensors network, something like [https://sensor.community] but with more flexibility.

The idea is to open source all the kits, parts and codes, and also the instructions to build weather stations. 
Maybe one day to share these kits among people, and to grow a community around citizen science. 

## Project Structure
This repository is organized into several components:

- **[Data API (Backend)](backend/data_api/README.md)**: Flask-based REST API for data ingestion and retrieval.
- **Frontend**: Dashboard and admin interface (documentation pending).
- **PCB**: KiCad designs for the weather station boards.
- **Station Scripts**: Firmware for ESP32 and other microcontrollers.

## TODO
Frontend:
- fix thing date in axis of plots
- improve overall look
Stations:
- implement configurator to configure wifi at first boot 
- implement automatic certificate retrieval
- migrate all stations to mqtt with ssl
Backend:
- server for ca.crt obtaining
