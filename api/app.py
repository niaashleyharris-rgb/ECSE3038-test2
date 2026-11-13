import json
import os
import secrets
from datetime import datetime, timezone
from typing import Literal, Optional

import paho.mqtt.client as mqtt
from dotenv import load_dotenv
from fastapi import FastAPI, Header, HTTPException, Query, Response, status
from pydantic import BaseModel, Field
from pymongo import DESCENDING, MongoClient

load_dotenv()

MONGODB_URI = os.getenv("MONGODB_URI")
MQTT_NAME = os.getenv("MQTT_NAME", "student")
BROKER_HOST = "135.232.203.144"
BROKER_PORT = 1883

client = MongoClient(MONGODB_URI)
db = client["ecse3038_test2"]
fans_col = db["fans"]
readings_col = db["readings"]

app = FastAPI(title="Bathroom Fan IoT API")


# --- Pydantic Models ---
class FanCreate(BaseModel):
    mac: str
    room: str


class ReadingCreate(BaseModel):
    mac: str
    humidity: float = Field(..., ge=0, le=100)


class FanSettings(BaseModel):
    mode: Literal["auto", "on", "off"]
    max_humidity: float = Field(..., ge=0, le=100)

def serialize_fan(doc: dict) -> dict:
    out = {
        "mac": doc["mac"],
        "room": doc["room"],
    }
    if "settings" in doc:
        out["settings"] = doc["settings"]
    return out

# --- A1: Fans ---
@app.post("/fans", status_code=status.HTTP_201_CREATED)
def create_fan(fan: FanCreate):
    if fans_col.find_one({"mac": fan.mac}):
        raise HTTPException(status_code=409, detail="MAC already registered")

    api_key = secrets.token_urlsafe(16)
    doc = {
        "mac": fan.mac,
        "room": fan.room,
        "api_key": api_key,
    }
    fans_col.insert_one(doc)
    return {"mac": fan.mac, "room": fan.room, "api_key": api_key}


@app.get("/fans")
def list_fans():
    return [serialize_fan(f) for f in fans_col.find()]


@app.get("/fans/{mac}")
def get_fan(mac: str):
    fan = fans_col.find_one({"mac": mac})
    if not fan:
        raise HTTPException(status_code=404, detail="Fan not found")
    return serialize_fan(fan)


# --- A2: Post Reading ---
@app.post("/readings", status_code=status.HTTP_201_CREATED)
def post_reading(
    reading: ReadingCreate,
    x_api_key: Optional[str] = Header(None, alias="X-API-Key"),
):
    if not x_api_key:
        raise HTTPException(status_code=401, detail="Missing X-API-Key")

    # Check if key exists at all
    key_owner = fans_col.find_one({"api_key": x_api_key})
    if not key_owner:
        raise HTTPException(status_code=401, detail="Invalid API key")

    # Check if key belongs to the submitted MAC
    if key_owner["mac"] != reading.mac:
        raise HTTPException(status_code=403, detail="API key does not match MAC")

    doc = {
        "mac": reading.mac,
        "humidity": reading.humidity,
        "timestamp": datetime.now(timezone.utc).isoformat(),
    }
    readings_col.insert_one(doc)
    return {
        "mac": doc["mac"],
        "humidity": doc["humidity"],
        "timestamp": doc["timestamp"],
    }

# --- A3: Get Readings ---
@app.get("/readings")
def get_readings(
    mac: Optional[str] = None,
    limit: int = Query(10, ge=1, le=100),
):
    query = {"mac": mac} if mac else {}
    cursor = readings_col.find(query, {"_id": 0}).sort("timestamp", DESCENDING).limit(limit)
    return list(cursor)

# --- A4: Update Fan Settings & Publish MQTT ---
@app.put("/fans/{mac}/settings")
def update_settings(mac: str, settings: FanSettings, response: Response):
    fan = fans_col.find_one({"mac": mac})
    if not fan:
        raise HTTPException(status_code=404, detail="Fan not found")

    is_new = "settings" not in fan
    settings_dict = {"mode": settings.mode, "max_humidity": settings.max_humidity}

    fans_col.update_one({"mac": mac}, {"$set": {"settings": settings_dict}})

    # Publish retained JSON to class broker
    topic = f"ecse3038/{MQTT_NAME}/fan/settings"
    payload = json.dumps(settings_dict)

    mqtt_client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    mqtt_client.connect(BROKER_HOST, BROKER_PORT, 60)
    mqtt_client.publish(topic, payload, retain=True)
    mqtt_client.disconnect()

    response.status_code = status.HTTP_201_CREATED if is_new else status.HTTP_200_OK
    return settings_dict

# --- A5: Delete Fan ---
@app.delete("/fans/{mac}", status_code=status.HTTP_204_NO_CONTENT)
def delete_fan(
    mac: str,
    x_api_key: Optional[str] = Header(None, alias="X-API-Key"),
):
    fan = fans_col.find_one({"mac": mac})
    if not fan:
        raise HTTPException(status_code=404, detail="Fan not found")

    if not x_api_key:
        raise HTTPException(status_code=401, detail="Missing X-API-Key")

    key_owner = fans_col.find_one({"api_key": x_api_key})
    if not key_owner:
        raise HTTPException(status_code=401, detail="Invalid API key")

    if fan["api_key"] != x_api_key:
        raise HTTPException(status_code=403, detail="Forbidden: key belongs to another fan")

    fans_col.delete_one({"mac": mac})
    return Response(status_code=status.HTTP_204_NO_CONTENT)