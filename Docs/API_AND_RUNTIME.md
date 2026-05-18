# API and Runtime Behavior

This firmware talks to the Laravel Biztola IoT Platform over HTTP.

Use the Mac LAN IP in `API_BASE_URL`, not `localhost`.

Example:

```cpp
#define API_BASE_URL "http://192.168.0.113:8000"
```

Laravel must be served on LAN:

```bash
php artisan serve --host=0.0.0.0 --port=8000
```

## Authentication

Every device API request includes:

```http
X-DEVICE-KEY: <device api key>
```

The firmware also sends `device_uuid` in the query string or JSON payload depending on the endpoint.

## Endpoints

### Heartbeat

```http
POST /api/device/heartbeat
```

Payload:

```json
{
  "device_uuid": "..."
}
```

Purpose:

- Confirms the device is alive.
- Updates `last_seen_at` in Laravel.
- Marks Laravel as reachable when successful.

### Config fetch

```http
GET /api/device/config?device_uuid=...
```

Laravel response includes changing server time fields and stable config object:

```json
{
  "server_time_utc": "2026-05-17T19:01:10+00:00",
  "server_time_local": "2026-05-18 01:01:10",
  "config": {
    "device_name": "Plant Bed C3 Prototype",
    "timezone": "Asia/Dhaka",
    "timezone_offset_minutes": 360,
    "watering_mode": "schedule",
    "soil_moisture_threshold": 35,
    "max_watering_duration_seconds": 30,
    "cooldown_minutes": 10,
    "local_manual_duration_seconds": 30,
    "schedules": []
  }
}
```

Firmware behavior:

- Parses `server_time_utc` for Laravel UTC time sync.
- Parses `config` for device behavior.
- Caches only the stable `config` object, not `server_time_utc` or `server_time_local`.
- Skips flash write if config is unchanged.
- Uses cached schedules for offline schedule fallback.

### Sensor readings

```http
POST /api/device/readings
```

Payload examples:

Connected soil sensor:

```json
{
  "device_uuid": "...",
  "temperature": 31.8,
  "humidity": 73,
  "soil_moisture": 0
}
```

Disconnected soil sensor:

```json
{
  "device_uuid": "...",
  "temperature": 31.8,
  "humidity": 73,
  "soil_moisture": null
}
```

Important:

- Disconnected soil sensor is `null`, not `0`.
- `0` means very dry valid reading.
- DHT11 failure sends `temperature: null` and/or `humidity: null`.

### Command polling

```http
GET /api/device/commands?device_uuid=...
```

Supported command types now:

```text
valve_on
valve_off
```

### Command ACK

```http
POST /api/device/commands/{id}/ack
```

Payload:

```json
{
  "device_uuid": "...",
  "status": "acknowledged"
}
```

or:

```json
{
  "device_uuid": "...",
  "status": "executed"
}
```

Behavior:

- `valve_on` is acknowledged when accepted.
- `valve_on` is executed only after watering actually ends.
- `valve_off` is acknowledged and executed after the valve is turned off.

### Device state sync

```http
POST /api/device/state
```

Payload:

```json
{
  "device_uuid": "...",
  "device_type": "plant_bed_controller",
  "firmware_version": "smart-plant-bed-c3-m12-0.2",
  "operation_state": "idle",
  "valve_state": "closed",
  "watering_state": "idle"
}
```

When a command completes:

```json
{
  "last_completed_command_id": 123
}
```

## Runtime intervals

Online/reachable Laravel:

```text
heartbeat     every 15 seconds
command poll  every 5 seconds
config fetch   every 60 seconds
sensor reading every 30 seconds
schedule check every 5 seconds
```

Offline/not-confirmed Laravel:

```text
heartbeat retry     every 30 seconds
command poll retry  every 30 seconds
config fetch retry  every 120 seconds
sensor reading      every 30 seconds
schedule check      every 5 seconds
```

HTTP timeouts:

```text
connect timeout  = 1000 ms
response timeout = 1500 ms
```

Laravel reachable window:

```text
15000 ms
```

## Watering behavior

### Dashboard valve_on

1. Command is received.
2. Duration is validated and capped by config.
3. Valve turns ON.
4. State sync says watering/open.
5. Command is acknowledged.
6. Watering remains active until duration ends.
7. Valve turns OFF.
8. Original `valve_on` command is marked executed.
9. Final state sync says idle/closed.

### Dashboard valve_off

1. Stop command is received.
2. Valve turns OFF immediately.
3. Any active `valve_on` command is closed as executed.
4. Stop command is acknowledged/executed.
5. Final state sync says idle/closed.

### Manual button

Button press while idle:

```text
start local watering
```

Button press while watering:

```text
stop watering immediately
```

If manual button stops a Laravel command, firmware marks that command executed and syncs final state.

### Offline auto fallback

Only when Laravel is not recently reachable:

```text
watering_mode == auto
soil moisture available
soil moisture <= threshold
cooldown passed
not already watering
```

Then firmware starts local watering for `max_watering_duration_seconds`.

### Offline schedule fallback

Only when Laravel is not recently reachable:

```text
watering_mode == schedule
cached schedules exist
time is ready from NTP, Laravel UTC, or DS3231 RTC
schedule is enabled
schedule day_of_week matches current local ISO day
schedule HH:MM matches current local HH:MM
not already watering
same schedule/date/time not already triggered
```

Then firmware starts local watering for the schedule's `duration_seconds`.

Expected log:

```text
Local fallback schedule watering triggered.
Valve ON - local schedule fallback
Watering duration completed.
Valve OFF - duration completed
```
