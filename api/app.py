
from fastapi import FastAPI, HTTPException, Header
from pydantic import BaseModel, Field
from datetime import datetime, timezone
from pydantic import BaseModel
from pymongo import MongoClient
from pymongo.errors import DuplicateKeyError
from dotenv import load_dotenv
import os
import secrets

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
