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