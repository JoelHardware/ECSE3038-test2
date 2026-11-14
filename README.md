# ECSE3038 Test 2 – Smart Bathroom Fan

## Project Overview

This project implements an IoT-based bathroom ventilation system using an ESP32, DHT22 humidity sensor, FastAPI, MongoDB Atlas, and MQTT.

The system monitors bathroom humidity and automatically controls an extractor fan, represented by an LED. Users can configure the fan's operating mode and humidity threshold through the FastAPI application.

## Technologies Used

- **ESP32:** Microcontroller responsible for sensor readings and fan control.
- **DHT22:** Measures relative humidity.
- **LED:** Simulates the bathroom extractor fan.
- **FastAPI:** Provides REST API endpoints for device registration, readings, and fan settings.
- **MongoDB Atlas:** Stores registered fans, API keys, humidity readings, and fan settings.
- **MQTT:** Enables communication between the API and ESP32.
- **PlatformIO and Wokwi:** Used for programming and simulating the ESP32.

## Hardware Configuration

| Component | ESP32 Connection |
|---|---|
| DHT22 Data | GPIO 15 |
| Fan LED | GPIO 2 |
| DHT22 VCC | 3.3V |
| DHT22 GND | GND |

The fan LED is connected through a 220 Ω resistor.

## API Endpoints

| Method | Endpoint | Description |
|---|---|---|
| POST | `/fans` | Register a fan and generate an API key |
| GET | `/fans` | Retrieve all registered fans |
| GET | `/fans/{mac}` | Retrieve a specific fan |
| POST | `/readings` | Submit humidity readings with API key authentication |
| GET | `/readings` | Retrieve humidity readings with optional filtering |
| PUT | `/fans/{mac}/settings` | Update fan settings and publish them through MQTT |
| DELETE | `/fans/{mac}` | Delete a registered fan using its API key |

## Running the API

1. Navigate to the API directory:

   ```bash
   cd api
   ```

2. Create and activate a Python virtual environment:

   ```bash
   python3 -m venv venv
   source venv/bin/activate
   ```

3. Install the required dependencies:

   ```bash
   pip install -r requirements.txt
   ```

4. Create a `.env` file containing:

   ```env
   MONGODB_URI=your_mongodb_connection_string
   MQTT_NAME=your_mqtt_name
   ```

5. Start the FastAPI server:

   ```bash
   uvicorn app:app --host 127.0.0.1 --port 8000
   ```

6. Access the deployed Swagger UI:

   http://iot-jhardware.chilecentral.cloudapp.azure.com/docs

The deployed API uses Nginx to forward requests to the FastAPI application.

## Running the ESP32 Board

1. Open the `board` directory as a PlatformIO project in Visual Studio Code.
2. Ensure the required libraries are installed through `platformio.ini`.
3. Create `board/include/secrets.h`:

   ```cpp
   #define DEVICE_KEY "your_generated_api_key"
   ```

4. Register the ESP32's MAC address using `POST /fans` to obtain its API key.
5. Build the project using PlatformIO.
6. Launch the simulation using Wokwi.
7. Open the Serial Monitor to observe humidity readings, HTTP responses, MQTT messages, and fan state changes.

The ESP32 reads humidity every 10 seconds and submits readings to the API.

## Fan Operating Modes

| Mode | Operation |
|---|---|
| `on` | Fan remains ON |
| `off` | Fan remains OFF |
| `auto` | Fan turns ON when humidity exceeds the configured threshold |

The fan remains OFF until valid settings are received.

## MQTT Communication

**Broker:** `135.232.203.144:1883`

**MQTT Name:** `tobia7140`

| Topic | Purpose |
|---|---|
| `ecse3038/tobia7140/fan/settings` | Receives fan mode and maximum humidity |
| `ecse3038/tobia7140/fan/state` | Publishes fan ON/OFF state |
| `ecse3038/tobia7140/status` | Publishes ESP32 online/offline status |

Messages are retained where required, and the ESP32 automatically reconnects after losing its MQTT connection.

## Security

API keys are generated during fan registration and are required for authenticated operations.

Sensitive files are excluded from GitHub using `.gitignore`, including:

- `.env`
- `venv/`
- `.pio/`
- `board/include/secrets.h`

## Author

Joel Hardware  
ECSE3038 – Engineering IoT Systems  
University of the West Indies, Mona