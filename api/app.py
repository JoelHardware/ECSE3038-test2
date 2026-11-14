
from fastapi import FastAPI, HTTPException, Header
from pydantic import BaseModel, Field
from datetime import datetime, timezone
from pydantic import BaseModel
from pymongo import MongoClient
from pymongo.errors import DuplicateKeyError
from dotenv import load_dotenv
import os
import secrets
from fastapi import Query

from fastapi import FastAPI, HTTPException, Header, Query, Response
from pydantic import BaseModel, Field
from typing import Literal
import paho.mqtt.publish as publish
import json

# Load environment variables
load_dotenv()

# Initialize FastAPI
app = FastAPI()

# Connect to MongoDB Atlas
client = MongoClient(os.getenv("MONGODB_URI"))
db = client["ecse3038_test2"]
fans_collection = db["fans"]

# Prevent duplicate MAC addresses
fans_collection.create_index("mac", unique=True)


# Request body for registering a fan
class Fan(BaseModel):
    mac: str
    room: str


# A1.1 - Register a new fan
@app.post("/fans", status_code=201)
def create_fan(fan: Fan):

    api_key = secrets.token_urlsafe(16)

    new_fan = {
        "mac": fan.mac,
        "room": fan.room,
        "api_key": api_key
    }

    try:
        fans_collection.insert_one(new_fan)
    except DuplicateKeyError:
        raise HTTPException(
            status_code=409,
            detail="Fan already exists"
        )

    return {
        "mac": fan.mac,
        "room": fan.room,
        "api_key": api_key
    }


# A1.2 - Get all fans
@app.get("/fans")
def get_fans():

    fans = list(
        fans_collection.find(
            {},
            {"_id": 0, "api_key": 0}
        )
    )

    return fans


# A1.3 - Get one fan by MAC address
@app.get("/fans/{mac}")
def get_fan(mac: str):

    fan = fans_collection.find_one(
        {"mac": mac},
        {"_id": 0, "api_key": 0}
    )

    if fan is None:
        raise HTTPException(
            status_code=404,
            detail="Fan not found"
        )

    return fan


# A2 - Model for humidity readings
class Reading(BaseModel):
    mac: str
    humidity: float = Field(ge=0, le=100)


# A2 - Submit a humidity reading
@app.post("/readings", status_code=201)
def create_reading(
    reading: Reading,
    x_api_key: str | None = Header(default=None)
):

    # Check whether an API key was supplied
    if not x_api_key:
        raise HTTPException(
            status_code=401,
            detail="Missing API key"
        )

    # Find the fan associated with the submitted key
    fan = fans_collection.find_one({
        "api_key": x_api_key
    })

    if fan is None:
        raise HTTPException(
            status_code=401,
            detail="Invalid API key"
        )

    # Verify that the key belongs to the submitted MAC
    if fan["mac"] != reading.mac:
        raise HTTPException(
            status_code=403,
            detail="API key belongs to another fan"
        )

    # Create reading with server-generated timestamp
    new_reading = {
        "mac": reading.mac,
        "humidity": reading.humidity,
        "timestamp": datetime.now(timezone.utc)
    }

    # Store reading in MongoDB
    db["readings"].insert_one(new_reading)

    return {
        "mac": reading.mac,
        "humidity": reading.humidity,
        "timestamp": new_reading["timestamp"]
    }

# A3 - GET readings endpoint

@app.get("/readings")
def get_readings(
    mac: str | None = None,
    limit: int = Query(default=10, ge=1, le=100)
):
    query = {}

    if mac is not None:
        query["mac"] = mac

    readings = list(
        db["readings"]
        .find(query, {"_id": 0})
        .sort("timestamp", -1)
        .limit(limit)
    )

    return readings


# A4 - Model for fan settings
class FanSettings(BaseModel):
    mode: Literal["auto", "on", "off"]
    max_humidity: float = Field(ge=0, le=100)


# A4 - Create or update fan settings
@app.put("/fans/{mac}/settings")
def update_fan_settings(
    mac: str,
    settings: FanSettings,
    response: Response
):

    # Find the registered fan
    fan = fans_collection.find_one({"mac": mac})

    if fan is None:
        raise HTTPException(
            status_code=404,
            detail="Fan not found"
        )

    # Check whether settings already exist
    settings_exist = "settings" in fan

    # Create settings object
    new_settings = {
        "mode": settings.mode,
        "max_humidity": settings.max_humidity
    }

    # Save settings in MongoDB
    fans_collection.update_one(
        {"mac": mac},
        {"$set": {"settings": new_settings}}
    )

    # Get MQTT name from .env
    mqtt_name = os.getenv("MQTT_NAME")

    if not mqtt_name:
        raise HTTPException(
            status_code=500,
            detail="MQTT_NAME is not configured"
        )

    # Build MQTT topic
    topic = f"ecse3038/{mqtt_name}/fan/settings"

    # Convert settings to JSON
    payload = json.dumps(new_settings)

    # Publish retained settings
    try:
        publish.single(
            topic=topic,
            payload=payload,
            hostname="135.232.203.144",
            port=1883,
            qos=1,
            retain=True
        )

    except Exception as error:
        raise HTTPException(
            status_code=503,
            detail=f"MQTT publish failed: {error}"
        )

    # Set response status
    if settings_exist:
        response.status_code = 200
    else:
        response.status_code = 201

    return {
        "mac": mac,
        "settings": new_settings,
        "mqtt_topic": topic
    }


# A5 - Delete a registered fan
@app.delete("/fans/{mac}", status_code=204)
def delete_fan(
    mac: str,
    x_api_key: str | None = Header(default=None)
):

    # Check whether the requested fan exists
    fan = fans_collection.find_one({"mac": mac})

    if fan is None:
        raise HTTPException(
            status_code=404,
            detail="Fan not found"
        )

    # Check whether an API key was provided
    if not x_api_key:
        raise HTTPException(
            status_code=401,
            detail="Missing API key"
        )

    # Check whether the API key is recognized
    key_owner = fans_collection.find_one({
        "api_key": x_api_key
    })

    if key_owner is None:
        raise HTTPException(
            status_code=401,
            detail="Invalid API key"
        )

    # Verify the key belongs to this fan
    if key_owner["mac"] != mac:
        raise HTTPException(
            status_code=403,
            detail="API key belongs to another fan"
        )

    # Delete the fan from MongoDB
    fans_collection.delete_one({"mac": mac})

    # Return 204 No Content
    return None
