Markdown
# ECSE3038 Test 2: Smart Bathroom Extractor Fan

An IoT system that monitors bathroom humidity using an ESP32 and DHT22 sensor in Wokwi, logs readings to MongoDB Atlas via a FastAPI backend, and automatically controls an extractor fan (LED) using MQTT.

## Repository Structure
- `api/` — FastAPI backend (`app.py`) and Python dependencies (`requirements.txt`).
- `board/` — PlatformIO ESP32 project (`src/main.cpp`), `platformio.ini`, and Wokwi simulation files (`diagram.json`, `wokwi.toml`).

---

## How to Run the API

1. **Navigate to the API directory and create a virtual environment:**
   ```bash
   cd api
   python3 -m venv venv
   source venv/bin/activate  # On Windows: venv\Scripts\activate
Install dependencies:

Bash
pip install -r requirements.txt
Configure environment variables:
Create a .env file inside api/ with your MongoDB Atlas URI and MQTT topic prefix:

Code snippet
MONGODB_URI=mongodb+srv://<username>:<password>@<cluster>.mongodb.net/?retryWrites=true&w=majority
MQTT_NAME=lizzy
Start the FastAPI server:

Bash
uvicorn app:app --host 127.0.0.1 --port 8000
Interactive API documentation is available at http://iot-nharris2005.northcentralus.cloudapp.azure.com/docs .

Key Endpoints
POST /fans — Register a new fan ({"mac", "room"}) and generate an API key.

GET /fans / GET /fans/{mac} — Retrieve registered fan(s).

POST /readings — Submit humidity readings (requires X-API-Key header).

GET /readings — Query historical readings (supports ?mac= and ?limit=).

PUT /fans/{mac}/settings — Update fan settings (mode: auto | on | off, max_humidity) and publish retained JSON to ecse3038/lizzy/fan/settings.

DELETE /fans/{mac} — Delete a fan (requires that fan's X-API-Key).

How to Run the Board (ESP32 in Wokwi)
Open the board/ folder in VS Code with the PlatformIO and Wokwi extensions installed.

Create board/include/secrets.h:

C++
#pragma once
#define DEVICE_KEY ""
#define API_BASE_URL "http://iot-nharris2005.northcentralus.cloudapp.azure.com"
#define MQTT_NAME "lizzy"
Register the device:

Build the project and start the Wokwi simulation.

Copy the ESP32's MAC address from the Serial Monitor.

Register the MAC using POST /fans on the API to obtain an api_key.

Paste the api_key into DEVICE_KEY in board/include/secrets.h.

Run the simulation:

Rebuild the project and restart the Wokwi simulation.

The ESP32 posts humidity readings every 10 seconds, reports online/offline status via MQTT Last Will on ecse3038/lizzy/status, listens for settings on ecse3038/lizzy/fan/settings, and publishes fan state changes on ecse3038/lizzy/fan/state.