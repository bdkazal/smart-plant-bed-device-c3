# Future Feature: Persistent Offline Event Sync

Current firmware safely performs local fallback watering when Laravel is unavailable:

```text
local_auto
local_schedule
manual button
```

When Laravel is offline, the device may water without the server knowing immediately. For V1 this is acceptable because device safety and plant protection are the priority.

This document records the future design for syncing offline watering history back to Laravel.

## Why this is not a small feature

A simple RAM-only event list is not reliable.

Bad case:

```text
1. Laravel is offline.
2. Device waters at 16:10 for 30 seconds by local_schedule.
3. Device loses power or restarts before Laravel returns.
4. RAM event is lost.
5. Laravel never learns that watering happened.
```

A correct implementation must survive power loss and restart.

## Required device-side design

The ESP32-C3 needs a persistent event queue stored in flash/NVS or a small append-only file if a filesystem is introduced.

Each event should include:

```text
event_id
firmware_version
event_type: watering_completed
trigger_type: local_auto | local_schedule | manual
schedule_id, nullable
duration_seconds
started_at_utc, nullable
ended_at_utc, nullable
time_source: NTP | LARAVEL_UTC | RTC | NONE
soil_moisture_percent, nullable
temperature, nullable
humidity, nullable
sync_status: pending | synced
retry_count
created_at_device_uptime_ms
```

Rules:

```text
write event only after watering actually completes
store pending event before trying network sync
never delete until Laravel confirms it stored or skipped duplicate
limit queue size to protect flash
skip precise timestamps if time is not trusted
```

## Required Laravel-side design

Laravel needs a clear contract, either:

```http
POST /api/device/events
```

or an extension of:

```http
POST /api/device/state
```

Recommended payload:

```json
{
  "device_uuid": "...",
  "events": [
    {
      "event_id": "...",
      "event_type": "watering_completed",
      "trigger_type": "local_schedule",
      "schedule_id": 12,
      "duration_seconds": 30,
      "started_at_utc": "2026-05-18T10:14:00Z",
      "ended_at_utc": "2026-05-18T10:14:30Z",
      "time_source": "RTC",
      "soil_moisture_percent": 0
    }
  ]
}
```

Laravel should:

```text
authenticate with device_uuid + X-DEVICE-KEY
validate event payload
store in watering_logs or a new device_events table
dedupe by device_id + event_id
return stored/skipped event IDs
show clear dashboard message
```

Example dashboard wording:

```text
Device watered offline at 16:14 for 30 sec by local_schedule.
Synced later when the device reconnected.
```

## Important decisions before implementation

Need to decide:

```text
Should events become WateringLog rows directly?
Should there be a separate DeviceEvent model/table?
Should manual local watering be synced too?
How many events can be queued?
How many flash writes are acceptable?
How to show unknown time if RTC was invalid?
Should Laravel trust device-reported timestamps or mark them as device-reported?
```

## Current V1 decision

Do not implement now.

Current V1 behavior:

```text
Laravel online actions are logged by Laravel.
Offline local fallback actions run safely on device.
Device resumes normal state/reading sync after reconnect.
Offline watering history is not guaranteed on Laravel dashboard yet.
```

Future milestone name:

```text
V2 — Persistent offline event log and Laravel sync
```
